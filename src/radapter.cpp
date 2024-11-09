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
        string_view cat = "lua";
        lua_Debug ar;
        if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "S", &ar)) {
            cat = ar.short_src;
            auto pos = cat.find_last_of("/\\");
            if (pos != string_view::npos) {
                cat = cat.substr(pos + 1);
            }
        }
        // cat is null-terminated
        Instance::FromLua(L)->Log(lvl, cat.data(), "{}", fmt::make_format_args(sv));
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

    // convert __call(t, ...) -> luaLog(...)
    static int log__call(lua_State* L) {
        lua_remove(L, 1);
        return luaLog(L);
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
    lua_pushinteger(L, info);
    lua_pushcclosure(L, protect<Impl::log__call>, 1);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_setglobal(L, "log");

    lua_register(L, "fmt", protect<format>);
    lua_register(L, "each", protect<each>);
    lua_register(L, "after", protect<after>);
    lua_register(L, "get", protect<get>);
    lua_register(L, "set", protect<set>);

    LoadEmbeddedFile("builtins");
    LoadEmbeddedFile("async", LoadEmbedGlobal);

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

#if defined(RADAPTER_JIT) || defined(CMAKE_CROSSCOMPILING)
const auto loadmode = "t";
#else
const auto loadmode = "b";
#endif

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
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto status = luaL_loadbufferx(L, src.data(), src.size(), name, loadmode);
    if (status != LUA_OK) {
        throw Err("Could not compile {}: {}", name, toSV(L));
    }
    status = lua_pcall(L, 0, 1, msgh);
    if (status != LUA_OK) {
        throw Err("Could not load {}: {}", name, toSV(L));
    }
}

static int load_embedded_module(lua_State* L) {
    const char* mod = lua_tostring(L, 1);
    load_mod(L, mod, load_builtin(fmt::format(":/scripts/{}.lua", mod).c_str()));
    return 1;
}

void Instance::LoadEmbeddedFile(string name, int opts)
{
    compat::prequiref(d->L, name.c_str(), protect<load_embedded_module>, opts & LoadEmbedGlobal ? 1 : 0);
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

namespace radapter {

struct FactoryContext {
    string name;
    Factory factory;
    ExtraMethods methods;
};
DESCRIBE(radapter::FactoryContext)

// object which represents worker inside lua
struct WorkerImpl {
    lua_State* L;
    QPointer<Worker> self{};
    QMetaObject::Connection conn{};
    int listenersRef = LUA_NOREF;

    static int get_listeners(lua_State* L) {
        auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
        auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, ctx->name.c_str()));
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->listenersRef);
        return 1;
    };

    static int worker_call(lua_State* L) {
        auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
        auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, ctx->name.c_str()));
        auto w = ud->self.data();
        if (!w) {
            throw Err("worker not usable");
        }
        w->OnMsg(toQVar(L, 2));
        return 1;
    }

    static int call_extra(lua_State* L) {
        auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
        auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, ctx->name.c_str()));
        auto* method = reinterpret_cast<ExtraMethod>(lua_touserdata(L, lua_upvalueindex(2)));
        auto w = ud->self.data();
        if (!w) {
            throw Err("worker not usable");
        }
        Push(L, method(w, toArgs(L, 2)));
        return 1;
    }

    ~WorkerImpl() {
        luaL_unref(L, LUA_REGISTRYINDEX, listenersRef);
        if (self) {
            QObject::disconnect(conn);
            self->deleteLater();
        }
    }
};
DESCRIBE(radapter::WorkerImpl)

} //radapter

static int workerFactory(lua_State* L) {
    auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto ctorArgs = toArgs(L, 1);
    auto* inst = Instance::FromLua(L);
    auto* w = ctx->factory(ctorArgs, inst);
    auto* ud = lua_udata(L, sizeof(WorkerImpl));
    auto* impl = new (ud) WorkerImpl{L};
    impl->self = w;

    lua_pushvalue(L, -1); // prevent gc, while actual worker is alive
    auto workerSelfRef = luaL_ref(L, LUA_REGISTRYINDEX);
    QObject::connect(w, &QObject::destroyed, w, [=]{
        luaL_unref(L, LUA_REGISTRYINDEX, workerSelfRef);
    });

    lua_createtable(L, 0, 0); //subs
    impl->listenersRef = luaL_ref(L, LUA_REGISTRYINDEX);

    impl->conn = QObject::connect(w, &Worker::SendMsg, w, [impl, w, L](QVariant const& msg){
        if (!lua_checkstack(L, 4)) {
            w->Error("Could not reserve stack to send msg");
            return;
        }
        lua_pushcfunction(L, traceback);
        auto msgh = lua_gettop(L);
        lua_getglobal(L, "call_all");
        lua_rawgeti(L, LUA_REGISTRYINDEX, impl->listenersRef);
        Push(L, msg);
        auto ok = lua_pcall(L, 2, 0, msgh);
        if (ok != LUA_OK) {
            w->Error("Could not notify listeners: {}", lua_tostring(L, -1));
        }
        lua_settop(L, msgh - 1);
    });
    if (luaL_newmetatable(L, ctx->name.c_str())) {
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, protect<WorkerImpl::get_listeners>, 1);
        lua_setfield(L, -2, "get_listeners");

        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, protect<WorkerImpl::worker_call>, 1);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__call");
        lua_setfield(L, -2, "call");

        for (auto it = ctx->methods.begin(); it != ctx->methods.end(); ++it) {
            auto name = it.key().toStdString();
            auto method = it.value();
            Push(L, it.key().toStdString());
            static_assert(sizeof(void*) >= sizeof(method));
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushlightuserdata(L, reinterpret_cast<void*>(method));
            lua_pushcclosure(L, protect<WorkerImpl::call_extra>, 2);
            lua_settable(L, -3);
        }

        lua_pushcfunction(L, dtor_for<WorkerImpl>);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    emit inst->WorkerCreated(w);
    return 1;
}

void radapter::Instance::RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra)
{
    auto L = d->L;
    Push(L, FactoryContext{name, factory, extra});
    lua_pushcclosure(L, protect<workerFactory>, 1);
    lua_setglobal(L, name);
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

void radapter::Instance::Eval(string_view code, string_view chunk)
{
    auto L = d->L;
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto load = luaL_loadbufferx(L, code.data(), code.size(), string{chunk}.c_str(), "t");
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
        if (!std::exchange(d->shutdownDone, true)) {
            emit HasShutdown();
        }
    });
    auto temp = d->workers; // may get modified due to Shutdown()
    for (auto w: temp) {
        w->Shutdown();
    }
}

lua_State *Instance::LuaState()
{
    return d->L;
}

radapter::Instance::~Instance()
{
    auto temp = d->workers; // modified due to deletion of each entry
    qDeleteAll(temp);
}

void Instance::RegisterGlobal(const char *name, const QVariant &value)
{
    auto L = d->L;
    Push(L, value);
    lua_setglobal(L, name);
}

namespace {
struct ExtraHelper {
    ExtraFunction func;
    int dummy{};
};
[[maybe_unused]]
DESCRIBE(ExtraHelper, &_::dummy)
}

static int wrapFunc(lua_State* L) {
    auto top = lua_gettop(L);
    auto args = QVariantList();
    args.reserve(top);
    for (auto i = 1; i <= top; ++i) {
        lua_pushvalue(L, i);
        args.push_back(toQVar(L));
        lua_pop(L, 1);
    }
    Push(L, CheckUData<ExtraHelper>(L, lua_upvalueindex(1)).func(Instance::FromLua(L), std::move(args)));
    return 1;
}

void Instance::RegisterFunc(const char *name, ExtraFunction func)
{
    glua::Push(d->L, ExtraHelper{std::move(func)});
    lua_pushcclosure(d->L, protect<wrapFunc>, 1);
    lua_setglobal(d->L, name);
}
