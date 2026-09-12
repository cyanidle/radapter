#include "radapter/radapter.hpp"
#include "radapter/async_helpers.hpp"
#include <QVariant>
#include <QMap>
#include <QTimer>
#include <qdatetime.h>
#include <QDir>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
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

static void luaReload(lua_State* L) {
    Instance::FromLua(L)->RequestReload();
}

int Instance::Impl::onShutdown(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    Instance::FromLua(L)->d->shutdownHandlers.emplace_back(L, 1);
    return 0;
}

// workers["unique-name"] -> the worker object with that name, or nil
static int workers_index(lua_State* L) {
    auto* inst = Instance::FromLua(L);
    auto name = QString::fromUtf8(luaL_checkstring(L, 2));
    auto* w = inst->findChild<Worker*>(name, Qt::FindDirectChildrenOnly);
    if (!w || w->_luaSelfRef == LUA_NOREF) {
        lua_pushnil(L);
        return 1;
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, w->_luaSelfRef);
    return 1;
}

void Instance::EnableGui() {
    if constexpr (GUI) {
        builtin::workers::gui(this);
        builtin::workers::qml_test_init(this);
        RegisterGlobal("RADAPTER_GUI", true);
    } else {
        Raise("GUI support was not enabled during build");
    }
}

extern "C" {
int luaopen_lfs(lua_State *L);
int luaopen_socket_core(lua_State *L);
}

static int load_embedded_module(lua_State* L);

// Register a module loader in package.preload so require(name) works lazily,
// without eagerly running the module at startup.
static void set_preload(lua_State* L, const char* name, lua_CFunction openf) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, openf);
    lua_setfield(L, -2, name);
    lua_pop(L, 2);
}

static std::atomic<unsigned> _curr_id = 0;

static int _gen_id(lua_State* L) {
    lua_pushinteger(L, _curr_id.fetch_add(1, std::memory_order_relaxed));
    return 1;
}

// schema()                  -> table of every worker's config schema (as --schema prints)
// schema("RedisCache")       -> that worker's schema, or nil
// schema("RedisCache","Sql") -> map of just the named workers' schemas
static int lua_schema(lua_State* L) {
    QStringList only;
    for (int i = 1, n = lua_gettop(L); i <= n; ++i) {
        if (lua_type(L, i) == LUA_TSTRING) only << QString::fromUtf8(lua_tostring(L, i));
    }
    auto res = Instance::FromLua(L)->GetSchemas(only);
    if (only.size() == 1) {
        auto it = res.find(only.front());
        glua::Push(L, it == res.end() ? QVariant{} : *it);
    } else {
        glua::Push(L, res);
    }
    return 1;
}

// raise the message captured as upvalue 1 (used for both __call and __index)
static int unavailableError(lua_State* L) {
    return luaL_error(L, "%s", lua_tostring(L, lua_upvalueindex(1)));
}

// register a global stub that raises `msg` on any call or index, so a feature used
// before it is enabled reports the cause instead of a nil call/index error
static void registerUnavailable(lua_State* L, const char* name, const char* msg) {
    lua_newtable(L);                                  // stub
    lua_newtable(L);                                  // metatable
    lua_pushstring(L, msg);
    lua_pushcclosure(L, unavailableError, 1);         // closure capturing msg
    lua_pushvalue(L, -1);                             // use it for both metamethods
    lua_setfield(L, -3, "__index");
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);
    lua_setglobal(L, name);
}

// ShutdownDone fires only when the workers are gone AND every fiber entry has
// parked; the Shutdown(timeout) singleShot remains the escape hatch.
static void emitDoneWhenIdle(Instance* inst)
{
    auto* d = inst->_GetPrivate();
    if (std::exchange(d->shutdownIdleArmed, true)) {
        return;
    }
    inst->Fibers()->stop(d->shutdownTimeout);
    if (!std::exchange(d->shutdownDone, true)) {
        emit inst->ShutdownDone();
    }
}

Instance::Instance(QObject *parent) :
    QObject(parent),
    d(new Impl)
{
    d->self = this;
    auto L = d->L = luaL_newstate();
    init_qrc();
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

    lua_register(L, "next_id", _gen_id);
    lua_register(L, "json_decode", glua::protect<builtin::api::json_decode>);
    lua_register(L, "json_encode", glua::protect<builtin::api::json_encode>);

    d->fibers = std::make_unique<FiberPool>(this);
    lua_register(L, "__await_native", glua::protect<builtin::api::AwaitNative>);
    lua_register(L, "__spawn_native", glua::protect<builtin::api::SpawnNative>);

    lua_register(L, "shutdown", glua::Wrap<luaShutdown>);
    lua_register(L, "reload", glua::Wrap<luaReload>);
    lua_register(L, "on_shutdown", glua::protect<Impl::onShutdown>);
    lua_register(L, "fmt", glua::protect<builtin::api::Format>);
    lua_register(L, "bytes", glua::protect<builtin::api::Bytes>);
    lua_register(L, "each", glua::protect<builtin::api::Each>);
    lua_register(L, "after", glua::protect<builtin::api::After>);
    lua_register(L, "get", glua::protect<builtin::api::Get>);
    lua_register(L, "set", glua::protect<builtin::api::Set>);
    lua_register(L, "schema", glua::protect<lua_schema>);
    lua_register(L, "load_plugin", glua::protect<builtin::api::LoadPlugin>);

    lua_newtable(L);
    lua_newtable(L); // metatable
    lua_pushcfunction(L, glua::protect<workers_index>);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "workers");

    radapter::compat::prequiref(L, "lfs", luaopen_lfs, 0);
    lua_pop(L, 1);
    LoadEmbeddedFile("builtins");
    LoadEmbeddedFile("async");

    set_preload(L, "socket.core", luaopen_socket_core);
    set_preload(L, "socket", glua::protect<load_embedded_module>);

    connect(this, &Instance::WorkerCreated, this, [this](Worker* w){
        d->workers.insert(w);
        connect(this, &Instance::ShutdownRequest, w, &Worker::Destroy);
        connect(w, &QObject::destroyed, this, [this, w]{
            auto it = d->workers.find(w);
            if (it != d->workers.end()) {
                d->workers.erase(it);
            }
            if (d->workers.empty() && d->shutdown) {
                emitDoneWhenIdle(this);
            }
        });
        connect(w, &Worker::ShutdownDone, w, &QObject::deleteLater);
    });
    for (size_t i{}; i < builtin::workers::count; ++i) {
        builtin::workers::all[i](this);
    }

    // Globals that only exist once a feature is enabled (QML -> EnableGui via --gui,
    // tags -> EnableTags via --tags) get an erroring stub so that using them otherwise
    // reports the cause instead of a bare "attempt to call/index a nil value". The
    // real value overwrites the stub when the feature is enabled.
    registerUnavailable(L, "QML", "QML worker is unavailable: run with --gui (needs a RADAPTER_GUI build)");
    registerUnavailable(L, "QML_Tester", "QML_Tester is unavailable: run with --gui (needs a RADAPTER_GUI build)");
    registerUnavailable(L, "tags", "tag API is unavailable: run with --tags");

    // flipped to true by EnableGui() (GUI build + --gui)
    lua_pushboolean(L, false);
    lua_setglobal(L, "RADAPTER_GUI");
}

static string load_builtin(QString name) {
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        Raise("Could not open builtin file: {}", name);
    }
    string s(size_t(f.size()), ' ');
    f.read(s.data(), f.size());
    return s;
}

static int load_embedded_module(lua_State* L) {
    const char* mod = lua_tostring(L, 1);
    std::string name = mod ;//std::string("__builtin_") + mod;
    auto src = load_builtin(fmt::format(":/scripts/{}.lua", mod).c_str());
    lua_pushcfunction(L, builtin::traceback);
    auto msgh = lua_gettop(L);
    auto status = luaL_loadbufferx(L, src.data(), src.size(), name.c_str(), radapter::JIT || radapter::Cross ? "t" : "b");
    if (status != LUA_OK) {
        Raise("Could not compile {}: {}", name, builtin::help::toSV(L));
    }
    status = lua_pcall(L, 0, 1, msgh);
    if (status != LUA_OK) {
        Raise("Could not load {}: {}", name, builtin::help::toSV(L));
    }
    return 1;
}

void Instance::LoadEmbeddedFile(string name, int opts)
{
    compat::prequiref(d->L, name.c_str(), glua::protect<load_embedded_module>, opts & LoadEmbedGlobal ? 1 : 0);
    if (!(opts & LoadEmbedNoPop)) {
        lua_pop(d->L, 1);
    }
}

optional<fs::path> Instance::CurrentFile()
{
    return d->currentFile;
}

Instance *Instance::FromLua(lua_State *L)
{
    lua_pushlightuserdata(L, instKey);
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto res = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return static_cast<Instance*>(res);
}

QVariantMap Instance::GetSchemas(QStringList const& only)
{
    QVariantMap res;
    for (auto& [k, v]: d->schemas) {
        auto key = QString::fromStdString(k);
        if (only.isEmpty() || only.contains(key)) {
            res[key] = v();
        }
    }
    return res;
}

QSet<Worker *> Instance::GetWorkers()
{
    return d->workers;
}

Worker *Instance::GetWorker(QString const& name)
{
    return findChild<Worker*>(name, Qt::FindDirectChildrenOnly);
}

std::runtime_error detail::doErr(fmt::string_view fmt, fmt::format_args args)
{
    return std::runtime_error(fmt::vformat(fmt, args));
}

void detail::doRaise(fmt::string_view fmt, fmt::format_args args)
{
    throw doErr(fmt, args);
}

void Instance::Log(LogLevel lvl, const char *cat, fmt::string_view fmt, fmt::format_args args) try
{
    if (d->globalLevel > lvl) return;
    // cat may carry the worker name appended as "category/name" - filter by category alone
    auto baseCat = string_view{cat};
    baseCat = baseCat.substr(0, baseCat.find('/'));
    auto it = d->perCat.find(baseCat);
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

    fmt::print(stderr,
        FMT_COMPILE("{}.{:0>3}|{}|{:>{}}| {}\n"),
        dt.toString(Qt::DateFormat::ISODate), dt.time().msec(),
        name, cat, d->logCatLen, fmt::vformat(fmt, args));
    ::fflush(stderr);

    if (d->luaLogHandler != LUA_NOREF && !d->insideLogHandler) {
        auto L = d->L;
        lua_rawgeti(L, LUA_REGISTRYINDEX, d->luaLogHandler);
        LuaFunction handler(L, ConsumeTop);
        QVariantMap record;
        record.insert("level", QString::fromUtf8(name.data(), int(name.size())));
        record.insert("timestamp", dt.toSecsSinceEpoch());
        record.insert("msg", QString::fromUtf8(fmt::vformat(fmt, args).c_str()));
        record.insert("category", QString::fromUtf8(cat));
        // the guard spans the whole async hop (set here, cleared when the handler
        // returns on its fiber), so a handler that logs cannot re-enter forever
        d->insideLogHandler = true;
        Fibers()->run([impl = d.get(), self = std::move(handler), record](Fiber*) {
            defer reset([&]{
                impl->insideLogHandler = false;
            });
            self.CallNoWait(QVariantList{record}, "log handler");
        });
    }
} catch (std::exception& e) {
    fprintf(stderr, "Error in Log(): %s\n", e.what());
}

void Instance::RegisterSchema(const char *name, ExtraSchema schemaGen)
{
    d->schemas[name] = schemaGen;
}

static fut::Future<QByteArray> httpGet(QUrl const& url, int timeoutMs = 30000)
{
    static QNetworkAccessManager manager;
    fut::Promise<QByteArray> prom;
    auto fut = prom.GetFuture();
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = manager.get(req);
    auto* timer = new QTimer(reply); // dies with the reply
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [prom = std::move(prom), reply] {
        if (reply->error() == QNetworkReply::NoError) {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status >= 200 && status < 300) {
                prom(reply->readAll());
            } else {
                prom(std::runtime_error(fmt::format("HTTP {}", status)));
            }
        } else {
            prom(std::runtime_error(reply->errorString().toStdString()));
        }
        reply->deleteLater();
    });
    timer->start(timeoutMs);
    return fut;
}

// package.searchers entry (installed only in HTTP mode): map a module name to a relative
// path and fetch it against the running script's base URL. Returns the loaded chunk on a
// hit, or an error string so `require` keeps trying the remaining searchers.
static int httpSearcher(lua_State* L)
{
    auto* inst = Instance::FromLua(L);
    QString base = inst->_GetPrivate()->scriptBaseUrl;
    if (base.isEmpty()) {
        lua_pushstring(L, "\n\tno http base url");
        return 1;
    }
    QString rel = QString::fromUtf8(luaL_checkstring(L, 1));
    rel.replace('.', '/');
    rel += ".lua";
    QUrl url = QUrl(base).resolved(QUrl(rel));

    QByteArray body;
    try {
        auto fut = httpGet(url);
        body = Await(std::move(fut));
    } catch (std::exception& e) {
        lua_pushfstring(L, "\n\tno http '%s' (%s)",
                        url.toString().toUtf8().constData(), e.what());
        return 1;
    }
    auto chunk = ("@" + url.toString()).toUtf8();
    if (luaL_loadbufferx(L, body.constData(), size_t(body.size()), chunk.constData(), "t") != LUA_OK) {
        return lua_error(L);
    }
    lua_pushstring(L, url.toString().toUtf8().constData()); // loader arg (module "filename")
    return 2;
}

// Insert httpSearcher into package.searchers (5.4) / package.loaders (LuaJIT) at index 2,
// right after the preload searcher so preloaded modules (e.g. socket) still win, but
// everything else is tried over HTTP before the filesystem searchers.
static void installHttpSearcher(lua_State* L)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_getfield(L, -1, "loaders");
    }
    if (!lua_istable(L, -1)) {
        Raise("could not install http searcher: package.searchers missing");
    }
#ifdef RADAPTER_JIT
    int len = int(lua_objlen(L, -1));
#else
    int len = int(lua_rawlen(L, -1));
#endif
    for (int i = len; i >= 2; --i) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
    }
    lua_pushcfunction(L, httpSearcher);
    lua_rawseti(L, -2, 2);
    lua_pop(L, 2);
}

static void runChunkOnFiber(Instance* inst, lua_State* L, string what)
{
    inst->Fibers()->run([inst, L, what = std::move(what)](Fiber* f) {
        auto* T = f->LuaState();
        lua_pushcfunction(T, builtin::traceback);
        lua_xmove(L, T, 1);
        int msgh = lua_gettop(T) - 1;
        auto base = f->suspends();
        auto status = lua_pcall(T, 0, 0, msgh);
        if (status == LUA_OK) {
            return;
        }
        auto err = builtin::help::toSV(T);
        if (f->suspends() == base) {
            Raise("{} error:\n\t{}", what, err);
        }
        inst->Error("radapter", "In ({}): {}", what, err);
    });
}

void Instance::EvalHttp(QString const& url)
{
    Fibers()->run([this, url](Fiber*) {
        QByteArray body;
        try {
            auto fut = httpGet(QUrl(url));
            body = Await(std::move(fut));
        } catch (std::exception& e) {
            Error("radapter", "Could not fetch script {}: {}", url.toStdString(), e.what());
            Shutdown();
            return;
        }
        auto L = d->L;
        d->scriptBaseUrl = url;
        installHttpSearcher(L);

        QUrl dir = QUrl(url).resolved(QUrl("."));
        RegisterGlobal("SCRIPT_PATH", QVariant(url));
        RegisterGlobal("SCRIPT_DIR", QVariant(dir.toString()));

        try {
            auto chunk = ("@" + url).toStdString();
            if (luaL_loadbufferx(L, body.constData(), size_t(body.size()), chunk.c_str(), "t") != LUA_OK) {
                Raise("Error loading {}: {}", url.toStdString(), builtin::help::toSV(L));
            }
            runChunkOnFiber(this, L, std::move(chunk));
        } catch (std::exception& e) {
            Error("radapter", "EvalHttp: {}", e.what());
            Shutdown();
        }
    });
}

void Instance::EvalFile(fs::path path)
{
    path = fs::absolute(fs::weakly_canonical(path));

    auto L = d->L;
    auto was = std::move(d->currentFile);
    defer revert([&]{
        d->currentFile = std::move(was);
    });
    d->currentFile = path;
    auto load = luaL_loadfile(L, path.string().c_str());
    if (load != LUA_OK) {
        Raise("Error loading file {}: {}", path.string(), builtin::help::toSV(L));
    }

    auto dir = path.parent_path();
    // let `require` find modules sitting next to the script (absolute, so it holds
    // regardless of cwd or when the require runs). Prepend once.
    {
        auto entry = fmt::format("{0}/?.lua;{0}/?/init.lua;", dir.u8string());
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");
        std::string cur = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        if (cur.compare(0, entry.size(), entry) != 0) {
            lua_pushstring(L, (entry + cur).c_str());
            lua_setfield(L, -2, "path");
        }
        lua_pop(L, 1);
    }

    RegisterGlobal("SCRIPT_PATH", QVariant(QString::fromStdString(path.u8string())));
    RegisterGlobal("SCRIPT_DIR", QVariant(QString::fromStdString(dir.u8string())));

    runChunkOnFiber(this, L, path.string());
}

void Instance::Eval(string_view code, string_view chunk)
{
    RegisterGlobal("SCRIPT_PATH", QVariant());
    RegisterGlobal("SCRIPT_DIR", QVariant());

    auto L = d->L;
    auto load = luaL_loadbufferx(L, code.data(), code.size(), string{chunk}.c_str(), "t");
    if (load != LUA_OK) {
        Raise("Error loading code: {}", builtin::help::toSV(L));
    }
    runChunkOnFiber(this, L, string{chunk});
}

void Instance::RequestReload()
{
    // Defer so the calling Lua frame unwinds before the host tears this instance down.
    QTimer::singleShot(0, this, [this]{ emit ReloadRequest(); });
}

void Instance::Shutdown(unsigned int timeout)
{
    if (d->shutdown) {
        return;
    }
    d->shutdown = true;
    d->shutdownTimeout = timeout;
    Warn("radapter", "Shutting down...");
    for (auto& fn : d->shutdownHandlers) {
        fn.CallNoWait({}, "on_shutdown");
    }
    d->shutdownHandlers.clear();
    // each()/after() timers are direct children; stop them so they can't fire Lua
    // callbacks into workers that are already mid-teardown
    for (auto* t : findChildren<QTimer*>(QString(), Qt::FindDirectChildrenOnly)) {
        t->stop();
    }
    emit ShutdownRequest();
    QTimer::singleShot(timeout, this, [this]{
        if (!std::exchange(d->shutdownDone, true)) {
            emit ShutdownDone();
        }
    });
    if (d->workers.empty()) {
        emitDoneWhenIdle(this);
    }
}

lua_State *Instance::LuaState()
{
    return d->L;
}

FiberPool* Instance::Fibers()
{
    return d->fibers.get();
}

Instance::~Instance()
{
    d->shutdownHandlers.clear();
    auto temp = d->workers; // modified due to deletion of each entry
    qDeleteAll(temp);
    luaL_unref(d->L, LUA_REGISTRYINDEX, d->luaLogHandler);
    // the tag registry holds LuaFunctions whose destructors luaL_unref into L, so it
    // must be torn down before lua_close (else it unrefs into a freed state -> crash)
    d->tagRegistry.reset();
    // fibers hold refs to their Lua threads: release them before lua_close (the pool
    // unwinds whatever is still suspended, so nothing outlives the state)
    d->fibers.reset();
    lua_close(d->L);
}

void Instance::RegisterGlobal(const char *name, const QVariant &value)
{
    auto L = d->L;
    glua::Push(L, value);
    lua_setglobal(L, name);
}

QVariant MakeFunction(ExtraFunction func)
{
    return QVariant::fromValue(std::make_shared<ExtraFunction>(std::move(func)));
}

void Instance::RegisterFunc(const char* name, ExtraFunction func)
{
    glua::Push(d->L, MakeFunction(std::move(func)));
    lua_setglobal(d->L, name);
}

#ifndef RADAPTER_GUI
constexpr auto unavail_fmt = "{}: available only in GUI builds";
RADAPTER_API void gui::StartRecording(Instance*) {Raise(unavail_fmt, __func__);}
RADAPTER_API QVariantList gui::StopRecording(Instance*) {Raise(unavail_fmt, __func__);}
RADAPTER_API void gui::ReplayFile(QString const&, double) {Raise(unavail_fmt, __func__);}
RADAPTER_API void gui::RecordNote(radapter::Instance*, QVariant const&) {Raise(unavail_fmt, __func__);}
#endif

}
