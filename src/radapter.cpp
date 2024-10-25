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
    LogLevel globalLevel;
    std::map<string, LogLevel, std::less<>> perCat;
    std::map<string, ExtraSchema> schemas;
    bool shutdown = false;
    bool shutdownDone = false;
    bool insideLog = false;
    int luaHandler = LUA_NOREF;

    static int luaLog(lua_State* L) {
        format(L);
        auto lvl = LogLevel(lua_tointeger(L, lua_upvalueindex(1)));
        size_t len;
        auto s = lua_tolstring(L, -1, &len);
        auto sv = string_view{s, len};
        auto inst = Instance::FromLua(L);
        if (inst->d->insideLog) {
            QMetaObject::invokeMethod(inst, [lvl, inst, m = string{sv}]{
                inst->Log(lvl, "lua", "{}", fmt::make_format_args(m));
            }, Qt::QueuedConnection);
        } else {
            Instance::FromLua(L)->Log(lvl, "lua", "{}", fmt::make_format_args(sv));
        }
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
        luaL_checktype(L, 1, LUA_TFUNCTION);
        auto inst = static_cast<Instance*>(lua_touserdata(L, lua_upvalueindex(1)));
        lua_rawgeti(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
        luaL_unref(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
        lua_pushvalue(L, 1);
        inst->d->luaHandler = luaL_ref(L, LUA_REGISTRYINDEX);
        return 1; //push previous handler
    }

    Impl() {
        L = luaL_newstate();
    }

    ~Impl() {
        luaL_unref(L, LUA_REGISTRYINDEX, luaHandler);
        lua_close(L);
    }

    struct FactoryContext {
        Factory factory;
        int metaRef = LUA_NOREF;
        Instance* inst;
        ~FactoryContext() {
            luaL_unref(inst->d->L, LUA_REGISTRYINDEX, metaRef);
        }
    };
};

static int _dummy;
static void* instKey = &_dummy;

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

    lua_newtable(L); //metatable
    lua_getfield(L, -2, "info");
    lua_pushcclosure(L, wrapInfo, 1);
    lua_setfield(L, -2, "__call");  //make log() call log.info()
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
    lua_register(L, "set_log_handler", protect<Impl::log_handler>);
    lua_register(L, "set_log_level", protect<Impl::log_level>);
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
    lua_gc(L, LUA_GCRESTART, 0);
}

Instance *Instance::FromLua(lua_State *L)
{
    lua_pushlightuserdata(L, instKey);
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto res = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<Instance*>(res);
}

radapter::Instance::~Instance()
{

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

void Instance::Log(LogLevel lvl, const char *cat, fmt::string_view fmt, fmt::format_args args)
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
    if (d->luaHandler != LUA_NOREF && !d->insideLog) {
        d->insideLog = true;
        auto L = d->L;
        lua_pushcfunction(L, traceback);
        auto msgh = lua_gettop(L);
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
            fmt::print(FMT_COMPILE("{}.{:0>3}|{:>5}|{:>12}| {}\n"),
                       dt.toString(Qt::DateFormat::ISODate),
                       dt.time().msec(),
                       "error",
                       "radapter",
                       "Error in lua log handler: " + string{lua_tostring(L, -1)});
        }
        lua_settop(L, msgh - 1);
        d->insideLog = false;
    }
    fmt::print(FMT_COMPILE("{}.{:0>3}|{:>5}|{:>12}| {}\n"),
               dt.toString(Qt::DateFormat::ISODate),
               dt.time().msec(),
               name,
               cat,
               fmt::vformat(fmt, args));
}

void Instance::RegisterSchema(const char *name, ExtraSchema schemaGen)
{
    d->schemas[name] = schemaGen;
}

static int wrapFunc(lua_State* L) {
    auto& f = *static_cast<ExtraFunction*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto top = lua_gettop(L);
    auto args = QVariantList();
    args.reserve(top);
    for (auto i = 1; i <= top; ++i) {
        lua_pushvalue(L, i);
        args.push_back(toQVar(L));
        lua_pop(L, 1);
    }
    push(L, f(Instance::FromLua(L), args));
    return 1;
}

void Instance::RegisterFunc(const char* name, ExtraFunction func)
{
    pushGced<ExtraFunction>(d->L, std::move(func));
    lua_pushcclosure(d->L, protect<wrapFunc>, 1);
    lua_setglobal(d->L, name);
}

void Instance::RegisterGlobal(const char* name, const QVariant &value)
{
    auto L = d->L;
    push(L, value);
    lua_setglobal(L, name);
}

static Worker* getWorker(lua_State* L, int idx) {
    return (*static_cast<Worker**>(lua_touserdata(L, idx)));
}

static int __worker_call(lua_State* L) {
    if (auto w = getWorker(L, 1)) {
        w->OnMsg(toQVar(L));
    }
    return 0;
}

constexpr auto subs = "__subscribers";

static int pipeCall(lua_State* L) {
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    //call func
    auto* ref = static_cast<int*>(lua_touserdata(L, 1));
    lua_rawgeti(L, LUA_REGISTRYINDEX, *ref);
    lua_pushvalue(L, 2);
    auto ok = lua_pcall(L, 1, 1, msgh);
    if (ok != LUA_OK) {
        Instance::FromLua(L)->Error("radapter", "Error calling function in pipe: {}", lua_tostring(L, -1));
        return 0;
    }
    auto t = lua_type(L, -1);
    if (t == LUA_TNIL) {
        return 0;
    }
    lua_getfield(L, 1, subs);
    iterateTable(L, [&]{
        lua_pushvalue(L, -1);
        lua_pushvalue(L, -5);
        auto ok = lua_pcall(L, 1, 0, msgh);
        if (ok != LUA_OK) {
            Instance::FromLua(L)->Error("radapter", "Error calling listener: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    return 0;
}

namespace {
struct RadPiper {
    int funcRef = LUA_NOREF;
    int subsRef = LUA_NOREF;
};
}

static int pipe(lua_State* L);
static int pipeGc(lua_State* L) {
    auto* piper = static_cast<RadPiper*>(lua_touserdata(L, 1));
    luaL_unref(L, LUA_REGISTRYINDEX, piper->funcRef);
    luaL_unref(L, LUA_REGISTRYINDEX, piper->subsRef);
    return 0;
}

template<auto pushSubs>
static int subsOrFallback(lua_State* L) noexcept {
    auto metaTable = lua_upvalueindex(1);
    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len;
        auto s = lua_tolstring(L, 2, &len);
        if (string_view{s, len} == subs) {
            pushSubs(L);
            return 1;
        }
    }
    lua_rawget(L, metaTable);
    return 1;
}

static void pushPiperSubs(lua_State* L) {
    auto* piper = static_cast<RadPiper*>(lua_touserdata(L, 1));
    lua_rawgeti(L, LUA_REGISTRYINDEX, piper->subsRef);
}

static void makePiper(lua_State* L) {
    auto ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L);
    auto subsRef = luaL_ref(L, LUA_REGISTRYINDEX);
    auto ud = lua_udata(L, sizeof(RadPiper));
    new (ud) RadPiper{ref, subsRef};
    if (luaL_newmetatable(L, "__rad_piper")) {
        luaL_Reg pipeMethods[] = {
            {"pipe", protect<pipe>},
            {"__shr", protect<pipe>},
            {"__call", protect<pipeCall>},
            {"__gc", pipeGc},
            {nullptr, nullptr},
        };
        luaL_setfuncs(L, pipeMethods, 0);

        lua_pushvalue(L, -1); // metatable to closure
        lua_pushcclosure(L, subsOrFallback<pushPiperSubs>, 1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

static int pipe(lua_State* L) {
    luaL_checktype(L, 1, LUA_TUSERDATA); //self
    luaL_checkany(L, 2); //target

    lua_pushvalue(L, 2);
    auto t = lua_type(L, -1);
    if (t == LUA_TFUNCTION) {
        makePiper(L); //replace function with RadPiper
    } else if (t == LUA_TUSERDATA) {
        //pass
    } else {
        throw Err("Cannot pipe to value of type: {}", lua_typename(L, t));
    }
    // on top is target
    lua_getfield(L, 1, subs); //-3
    auto subsLen = lua_Integer(isArray(L) + 1);
    lua_pushinteger(L, subsLen); //-2
    lua_pushvalue(L, -3); //-1 copy target
    lua_rawset(L, -3);
    lua_pop(L, 1); //pop subs table
    // target on top
    return 1;
}

// self on top, call() all listeners
static void notifyListeners(lua_State* L, QVariant const& msg) noexcept try {
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    if (!msg.isValid()) {
        return;
    }
    push(L, msg);
    lua_getfield(L, -3, subs);
    iterateTable(L, [&]{
        lua_pushvalue(L, -1);
        lua_pushvalue(L, -5);
        auto ok = lua_pcall(L, 1, 0, msgh);
        if (ok != LUA_OK) {
            Instance::FromLua(L)->Error("radapter", "Error calling listener: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    });
    lua_settop(L, msgh - 1);
} catch (std::exception& e) {
    Instance::FromLua(L)->Error("radapter", "Error in notify listeners: {}", e.what());
}

static int __worker_gc(lua_State* L) {
    if (auto w = getWorker(L, 1)) {
        w->~Worker();
    }
    return 0;
}

static int callExtra(lua_State* L) {
    const char* cls = lua_tostring(L, lua_upvalueindex(1));
    ExtraMethod& extra = *static_cast<ExtraMethod*>(lua_touserdata(L, lua_upvalueindex(2)));
    assert(extra && "Invalid ExtraMethod");
    void* raw = luaL_checkudata(L, 1, cls);
    Worker* w = *static_cast<Worker**>(raw);
    auto res = extra(w, toArgs(L, 2));
    push(L, res);
    return 1;
}

static void pushWorkerSubs(lua_State* L) {
    auto* w = getWorker(L, 1);
    lua_rawgeti(L, LUA_REGISTRYINDEX, w->_subs);
}

static void pushMeta(lua_State* L, const char* name, ExtraMethods const& extra)
{
    if (!luaL_newmetatable(L, name)) {
        throw Err("{} already registered", name);
    }
    luaL_Reg funcs[] = {
        {"pipe", protect<pipe>},
        {"__shr", protect<pipe>},
        {"__call", protect<__worker_call>},
        {"__gc", __worker_gc},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, funcs, 0);

    lua_pushvalue(L, -1); //metatable to closure
    lua_pushcclosure(L, subsOrFallback<pushWorkerSubs>, 1);
    lua_setfield(L, -2, "__index");

    for (auto it = extra.keyValueBegin(); it != extra.keyValueEnd(); ++it) {
        auto&& [k, func] = *it;
        lua_pushstring(L, name);
        pushGced<ExtraMethod>(L, func);
        lua_pushcclosure(L, protect<callExtra>, 2);
        lua_setfield(L, -2, k.toStdString().c_str());
    }
}


using FactoryContext = Instance::Impl::FactoryContext;

static int factoryFunc(lua_State* L)
{
    FactoryContext* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto w = ctx->factory(toArgs(L), ctx->inst);
    auto mem = lua_udata(L, sizeof(w));
    memcpy(mem, &w, sizeof(w));
    lua_newtable(L);
    w->_subs = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, -1);
    w->_self = luaL_ref(L, LUA_REGISTRYINDEX);
    w->connect(w, &Worker::SendMsg, ctx->inst, [w, self = ctx->inst](QVariant const& msg){
        auto L = self->LuaState();
        lua_rawgeti(L, LUA_REGISTRYINDEX, w->_self);
        notifyListeners(L, msg);
        lua_pop(L, 1);
    });
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->metaRef);
    lua_setmetatable(L, -2);
    emit ctx->inst->WorkerCreated(w);
    return 1;
}

void radapter::Instance::RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra)
{
    auto L = d->L;
    pushMeta(L, name, extra);
    auto metaRef = luaL_ref(L, LUA_REGISTRYINDEX);

    pushGced<FactoryContext>(L, factory, metaRef, this);
    lua_pushcclosure(L, protect<factoryFunc>, 1);
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
