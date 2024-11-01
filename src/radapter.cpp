#include "radapter.hpp"
#include <QVariant>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <qdatetime.h>
#include <QDir>
#include "builtin.hpp"
#include "builtin_funcs.hpp"
#include "fmt/compile.h"

struct radapter::Instance::Impl {
    lua_State* L;
    QSet<Worker*> workers;
    LogLevel globalLevel = LogLevel::debug;
    std::map<string, LogLevel, std::less<>> perCat;
    std::map<string, ExtraSchema> schemas;
    bool shutdown = false;
    bool shutdownDone = false;
    int insideLogHandler = false;
    int luaHandler = LUA_NOREF;

    static int luaLog(lua_State* L) {
        auto inst = Instance::FromLua(L);
        if (inst->d->insideLogHandler) return 0;
        format(L);
        auto lvl = LogLevel(lua_tointeger(L, lua_upvalueindex(1)));
        size_t len;
        auto s = lua_tolstring(L, -1, &len);
        auto sv = string_view{s, len};
        Instance::FromLua(L)->Log(lvl, "lua", "{}", fmt::make_format_args(sv));
        return 0;
    }

    static int log_level(lua_State* L) {
        luaL_checktype(L, 1, LUA_TSTRING);
        auto count = lua_gettop(L);
        auto inst = Instance::FromLua(L);
        if (count == 1) {
            auto lvl = toSV(L, 1);
            if (!describe::name_to_enum(lvl, inst->d->globalLevel)) {
                throw Err("Invalid log_level passed: {}, avail: [{}]", lvl, fmt::join(describe::field_names<LogLevel>(), ", "));
            }
        } else {
            luaL_checktype(L, 2, LUA_TSTRING);
            auto cat = toSV(L, 1);
            auto lvl = toSV(L, 2);
            if (!describe::name_to_enum(lvl, inst->d->perCat[string{cat}])) {
                throw Err("Invalid log_level passed: {}, avail: [{}]", lvl, fmt::join(describe::field_names<LogLevel>(), ", "));
            }
        }
        return 0;
    }

    static int log_handler(lua_State* L) {
        auto inst = Instance::FromLua(L);
        if (lua_isnil(L, 1)) {
            luaL_unref(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
            inst->d->luaHandler = LUA_NOREF;
        } else {
            luaL_checktype(L, 1, LUA_TFUNCTION);
            luaL_unref(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
            lua_pushvalue(L, 1);
            inst->d->luaHandler = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        return 0;
    }

    Impl() {
        L = luaL_newstate();
    }

    ~Impl() {
        luaL_unref(L, LUA_REGISTRYINDEX, luaHandler);
        lua_close(L);
    }

};

static int _dummy;
static void* instKey = &_dummy;

// convert __call(t, ...) -> upvalue(1)(...)
static int wrapInfo(lua_State* L) noexcept {
    auto args = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 2);
    lua_call(L, args - 1, 0);
    return 0;
}

static void init_qrc() {
    Q_INIT_RESOURCE(radapter);
}

radapter::Instance::Instance() : d(new Impl)
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
    lua_pushcclosure(L, protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "debug");
    lua_pushinteger(L, info);
    lua_pushcclosure(L, protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "info");
    lua_pushinteger(L, warn);
    lua_pushcclosure(L, protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "warn");
    lua_pushinteger(L, error);
    lua_pushcclosure(L, protect<Impl::luaLog>, 1);
    lua_setfield(L, -2, "error");
    lua_pushcfunction(L, protect<Impl::log_handler>);
    lua_setfield(L, -2, "set_handler");
    lua_pushcfunction(L, protect<Impl::log_level>);
    lua_setfield(L, -2, "set_level");

    lua_newtable(L); //log. metatable
    lua_getfield(L, -2, "info");
    lua_pushcclosure(L, wrapInfo, 1);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setglobal(L, "log");

    lua_register(L, "fmt", protect<format>);
    lua_register(L, "each", protect<each>);
    lua_register(L, "after", protect<after>);
    lua_register(L, "pipe", pipeAll);
    lua_register(L, "get", protect<get>);
    lua_register(L, "set", protect<set>);
    lua_register(L, "wrap", protect<wrap>);
    lua_register(L, "unwrap", protect<unwrap>);
    connect(this, &Instance::WorkerCreated, this, [this](Worker* w){
        d->workers.insert(w);
        connect(w, &QObject::destroyed, this, [this, w]{
            auto it = d->workers.find(w);
            if (it != d->workers.end()) {
                d->workers.erase(it);
            }
            if (d->workers.empty() && d->shutdown) {
                if (!std::exchange(d->shutdownDone, true)) {
                    emit HasShutdown();
                }
            }
        });
        connect(w, &Worker::ShutdownDone, w, &QObject::deleteLater);
    });
    for (auto system: builtin::all) {
        system(this);
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

std::runtime_error radapter::detail::doErr(fmt::string_view fmt, fmt::format_args args)
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
    fmt::print(
        FMT_COMPILE("{}.{:0>3}|{:>5}|{:>12}| {}\n"),
        dt.toString(Qt::DateFormat::ISODate), dt.time().msec(),
        name, cat, fmt::vformat(fmt, args));

    if (d->luaHandler != LUA_NOREF && !d->insideLogHandler) {
        auto L = d->L;
        lua_pushcfunction(L, traceback);
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

struct radapter::WorkerImpl {

};

void radapter::Instance::RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra)
{
    auto L = d->L;
    // TODO

}

void radapter::Instance::EvalFile(fs::path path)
{
    auto L = d->L;

    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto load = luaL_loadfile(L, path.string().c_str());
    if (load != LUA_OK) {
        throw Err("Error loading file {}: {}", path.string(), toSV(L));
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
        auto e = toSV(L);
        throw Err("EvalFile error: {}", e);
    }
}

void radapter::Instance::Eval(std::string_view code)
{
    auto L = d->L;
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto load = luaL_loadbufferx(L, code.data(), code.size(), "<eval>", "t");
    if (load != LUA_OK) {
        throw Err("Error loading code: {}", toSV(L));
    }
    auto res = lua_pcall(L, 0, 0, msgh);
    if (res != LUA_OK) {
        auto e = toSV(L);
        throw Err("Eval error: {}", e);
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
        auto ws = d->workers;
        for (auto w: ws) {
            delete w;
        }
        if (!std::exchange(d->shutdownDone, true)) {
            emit HasShutdown();
        }
    });
    auto ws = d->workers;
    for (auto w: ws) {
        w->Shutdown();
    }
}

lua_State *Instance::LuaState()
{
    return d->L;
}

radapter::Instance::~Instance()
{

}
