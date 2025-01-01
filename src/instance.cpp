#include "radapter/radapter.hpp"
#include <QVariant>
#include <QMap>
#include <QTimer>
#include <qdatetime.h>
#include <QDir>
#include "fmt/compile.h"
#include "glua/glua.hpp"
#include "instance_impl.hpp"

static void init_qrc() {
    Q_INIT_RESOURCE(radapter);
}

namespace radapter {

static int _dummy;
static void* instKey = &_dummy;

static void luaShutdown(lua_State* L, optional<unsigned> timeout) {
    Instance::FromLua(L)->Shutdown(timeout ? *timeout : 5000);
}

void Instance::EnableGui() {
    if constexpr (GUI) {
        builtin::workers::gui(this);
    } else {
        throw Err("GUI support was not enabled during build");
    }
}

Instance::Instance() : d(new Impl)
{
    init_qrc();
    auto L = d->L;
    lua_gc(L, LUA_GCSTOP, 0);
    defer _restart([&]{
        lua_gc(L, LUA_GCRESTART, 0);
    });

    luaL_openlibs(L);

    lua_pushlightuserdata(L, instKey);
    lua_pushlightuserdata(L, this);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_newtable(L);
    lua_pushinteger(L, debug);
    lua_pushcclosure(L, glua::protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "debug");
    lua_pushinteger(L, info);
    lua_pushcclosure(L, glua::protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "info");
    lua_pushinteger(L, warn);
    lua_pushcclosure(L, glua::protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "warn");
    lua_pushinteger(L, error);
    lua_pushcclosure(L, glua::protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "error");
    lua_pushcfunction(L, glua::protect<Impl::log_handler>);
    lua_setfield(L, -2, "set_handler");
    lua_pushcfunction(L, glua::protect<Impl::log_level>);
    lua_setfield(L, -2, "set_level");

    lua_newtable(L); //log. metatable
    lua_pushinteger(L, info);
    lua_pushcclosure(L, glua::protect<Impl::log__call>, 1);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setglobal(L, "log");

    lua_register(L, "shutdown", glua::Wrap<luaShutdown>);
    lua_register(L, "fmt", glua::protect<builtin::api::Format>);
    lua_register(L, "temp_file", glua::protect<builtin::api::TempFile>);
    lua_register(L, "each", glua::protect<builtin::api::Each>);
    lua_register(L, "after", glua::protect<builtin::api::After>);
    lua_register(L, "get", glua::protect<builtin::api::Get>);
    lua_register(L, "set", glua::protect<builtin::api::Set>);

    LoadEmbeddedFile("builtins");
    LoadEmbeddedFile("coro_patch");
    LoadEmbeddedFile("async", LoadEmbedGlobal);

    connect(this, &Instance::WorkerCreated, this, [this](Worker* w){
        d->workers.insert(w);
        connect(this, &Instance::ShutdownRequest, w, &Worker::Shutdown);
        connect(w, &QObject::destroyed, this, [this, w]{
            auto it = d->workers.find(w);
            if (it != d->workers.end()) {
                d->workers.erase(it);
            }
            if (d->workers.empty() && d->shutdown) {
                if (!std::exchange(d->shutdownDone, true)) {
                    emit ShutdownDone();
                }
            }
        });
        connect(w, &Worker::ShutdownDone, w, &QObject::deleteLater);
    });
    for (auto system: builtin::workers::all) {
        system(this);
    }
}

static string load_builtin(QString name) {
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        throw Err("Could not open builtin file: {}", name);
    }
    string s(size_t(f.size()), ' ');
    f.read(s.data(), f.size());
    return s;
}

static void load_mod(lua_State* L, const char* name, string_view src) {
    lua_pushcfunction(L, builtin::help::traceback);
    auto msgh = lua_gettop(L);
    auto status = luaL_loadbufferx(L, src.data(), src.size(), name, radapter::JIT || radapter::Cross ? "t" : "b");
    if (status != LUA_OK) {
        throw Err("Could not compile {}: {}", name, builtin::help::toSV(L));
    }
    status = lua_pcall(L, 0, 1, msgh);
    if (status != LUA_OK) {
        throw Err("Could not load {}: {}", name, builtin::help::toSV(L));
    }
}

static int load_embedded_module(lua_State* L) {
    const char* mod = lua_tostring(L, 1);
    load_mod(L, mod, load_builtin(fmt::format(":/scripts/{}.lua", mod).c_str()));
    return 1;
}

void Instance::LoadEmbeddedFile(string name, int opts)
{
    compat::prequiref(d->L, name.c_str(), glua::protect<load_embedded_module>, opts & LoadEmbedGlobal ? 1 : 0);
    if (!(opts & LoadEmbedNoPop)) {
        lua_pop(d->L, 1);
    }
}

Instance *Instance::FromLua(lua_State *L)
{
    lua_pushlightuserdata(L, instKey);
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto res = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<Instance*>(res);
}

QVariantMap Instance::GetSchemas()
{
    QVariantMap res;
    for (auto& [k, v]: d->schemas) {
        res[QString::fromStdString(k)] = v();
    }
    return res;
}

QSet<Worker *> Instance::GetWorkers()
{
    return d->workers;
}

std::runtime_error detail::doErr(fmt::string_view fmt, fmt::format_args args)
{
    return std::runtime_error(fmt::vformat(fmt, args));
}

void Instance::Log(LogLevel lvl, const char *cat, fmt::string_view fmt, fmt::format_args args) try
{
    if (d->globalLevel > lvl) return;
    auto it = d->perCat.find(string_view{cat});
    if (it != d->perCat.end() && it->second > lvl) {
        return;
    }
    string_view name;
    if (!describe::enum_to_name(lvl, name)) {
        name = "<inval>";
    }
    auto dt = QDateTime::currentDateTime();
    auto c = string_view(cat);
    if (c.size() > d->logCatLen) {
        d->logCatLen = unsigned(c.size());
    }
    fmt::print(
        FMT_COMPILE("{}.{:0>3}|{:>5}|{:>{}}| {}\n"),
        dt.toString(Qt::DateFormat::ISODate), dt.time().msec(),
        name, cat, d->logCatLen, fmt::vformat(fmt, args));

    if (d->luaHandler != LUA_NOREF && !d->insideLogHandler) {
        auto L = d->L;
        lua_pushcfunction(L, builtin::help::traceback);
        auto msgh = lua_gettop(L);
        d->insideLogHandler = true;
        defer reset([&]{
            d->insideLogHandler = false;
            lua_settop(L, msgh - 1);
        });
        if (!lua_checkstack(L, 3)) {
            throw Err("Could not reserve stack for log handler");
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, d->luaHandler);
        lua_createtable(L, 0, 4);
        lua_pushliteral(L, "level");
        lua_pushlstring(L, name.data(), name.size());
        lua_rawset(L, -3);
        lua_pushliteral(L, "timestamp");
        lua_pushinteger(L, dt.toSecsSinceEpoch());
        lua_rawset(L, -3);
        lua_pushliteral(L, "msg");
        lua_pushstring(L, fmt::vformat(fmt, args).c_str());
        lua_rawset(L, -3);
        lua_pushliteral(L, "category");
        lua_pushstring(L, cat);
        lua_rawset(L, -3);
        if (lua_pcall(L, 1, 0, msgh) != LUA_OK) {
            throw Err("Error in lua log handler: {}", lua_tostring(L, -1));
        }
    }
} catch (std::exception& e) {
    fprintf(stderr, "Error in Log(): %s\n", e.what());
}

void Instance::RegisterSchema(const char *name, ExtraSchema schemaGen)
{
    d->schemas[name] = schemaGen;
}

void Instance::EvalFile(fs::path path)
{
    auto L = d->L;

    lua_pushcfunction(L, builtin::help::traceback);
    auto msgh = lua_gettop(L);
    auto load = luaL_loadfile(L, path.string().c_str());
    if (load != LUA_OK) {
        throw Err("Error loading file {}: {}", path.string(), builtin::help::toSV(L));
    }

    auto dir = path.parent_path();
    auto wasCwd = QDir::currentPath();
    if (!dir.empty()) {
        QDir::setCurrent(QString::fromUtf8(dir.u8string().c_str()));
    }
    auto res = lua_pcall(L, 0, 0, msgh);
    if (!dir.empty()) {
        QDir::setCurrent(wasCwd);
    }
    if (res != LUA_OK) {
        auto e = builtin::help::toSV(L);
        throw Err("EvalFile error: \n{}", e);
    }
}

void Instance::Eval(string_view code, string_view chunk)
{
    auto L = d->L;
    lua_pushcfunction(L, builtin::help::traceback);
    auto msgh = lua_gettop(L);
    auto load = luaL_loadbufferx(L, code.data(), code.size(), string{chunk}.c_str(), "t");
    if (load != LUA_OK) {
        throw Err("Error loading code: {}", builtin::help::toSV(L));
    }
    auto res = lua_pcall(L, 0, 0, msgh);
    if (res != LUA_OK) {
        auto e = builtin::help::toSV(L);
        throw Err("Eval error: \n{}", e);
    }
}

void Instance::Shutdown(unsigned int timeout)
{
    if (d->shutdown) {
        return;
    }
    d->shutdown = true;
    Warn("radapter", "Shutting down...");
    emit ShutdownRequest();
    QTimer::singleShot(timeout, this, [this]{
        if (!std::exchange(d->shutdownDone, true)) {
            emit ShutdownDone();
        }
    });
    if (d->workers.empty()) {
        emit ShutdownDone();
    }
}

lua_State *Instance::LuaState()
{
    return d->L;
}

Instance::~Instance()
{
    auto temp = d->workers; // modified due to deletion of each entry
    qDeleteAll(temp);
}

void Instance::RegisterGlobal(const char *name, const QVariant &value)
{
    auto L = d->L;
    glua::Push(L, value);
    lua_setglobal(L, name);
}


QVariant MakeFunction(ExtraFunction func) {
    return QVariant::fromValue(std::move(func));
}

void Instance::RegisterFunc(const char *name, ExtraFunction func)
{
    glua::Push(d->L, MakeFunction(std::move(func)));
    lua_setglobal(d->L, name);
}

}
