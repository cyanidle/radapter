#include "radapter/worker.hpp"
#include "radapter/async_helpers.hpp"
#include <QTimer>
#include "instance_impl.hpp"
#include "glua/glua.hpp"
#include "worker_impl.hpp"
#include "tags.hpp"

namespace radapter
{

static QString sanitizeName(QString const& s) {
    QString out;
    for (auto c : s) {
        if (c.isLetterOrNumber() || c == '_' || c == '.') out += c;
        else if (!out.isEmpty() && out.back() != '_') out += '_';
    }
    while (out.endsWith('_')) out.chop(1);
    return out.isEmpty() ? QStringLiteral("worker") : out;
}

static bool isValidWorkerName(QString const& name) {
    if (name.isEmpty()) return false;
    for (auto c : name) {
        if (!c.isLetterOrNumber() && c != '.' && c != '_') return false;
    }
    return true;
}

static string luaOrigin(lua_State* L) {
    if (!L) return "<CPP>";
    lua_Debug ar;
    for (int lvl = 0; lua_getstack(L, lvl, &ar); ++lvl) {
        if (lua_getinfo(L, "Sl", &ar) && ar.currentline > 0) {
            return fmt::format("{}:{}", ar.short_src, ar.currentline);
        }
    }
    return "<CPP>";
}

Worker::Worker(Instance *parent, const char *category) :
    Worker(parent, WorkerConfig{}, category)
{
}

Worker::Worker(Instance *parent, WorkerConfig const& conf, const char *category) :
    QObject(parent),
    _Category(conf.category && !conf.category->isEmpty() ? conf.category->toStdString() : category)
{
    _Inst = parent;
    connect(this, &Worker::SendEventField, [this](const QString& key, const QVariant& data){
        emit SendEvent(QVariantMap{{key, data}});
    });
    connect(this, &Worker::SendMsgField, [this](const QString& key, const QVariant& data){
        emit SendMsg(QVariantMap{{key, data}});
    });
    // TODO: handle SendMsg/Event before LuaPush

    auto taken = [parent](QString const& n) {
        return parent->findChild<Worker*>(n, Qt::FindDirectChildrenOnly) != nullptr;
    };
    QString name = conf.name.value_or(QString{});
    bool autoNamed = conf.generated_name;
    if (name.isEmpty()) {
        name = QString::fromStdString(_Category);
        autoNamed = true;
    }
    if (autoNamed) {
        name = sanitizeName(name);
    } else if (!isValidWorkerName(name)) {
        Raise("Worker name '{}' must be alphanumeric, dots, or underscores only", name);
    }
    if (taken(name)) {
        if (!autoNamed) {
            Raise("Worker name '{}' is already taken", name);
        }
        QString candidate;
        int n = 2;
        do {
            candidate = name + "." + QString::number(n++);
        } while (taken(candidate));
        name = candidate;
    }
    setObjectName(name);
    auto stdName = name.toStdString();
    _LogCat = stdName == _Category ? _Category : _Category + "/" + stdName;
    auto& catLen = parent->_GetPrivate()->logCatLen;
    if (_LogCat.size() > catLen) catLen = unsigned(_LogCat.size());
    auto* caller = parent->_GetPrivate()->currentCaller;
    _Origin = luaOrigin(caller ? caller : parent->LuaState());
}

bool Worker::TagsEnabled() const {
    return _Inst->_GetPrivate()->tagRegistry != nullptr;
}

void Worker::AdvertiseFields(QStringList const& fields) {
    if (auto* reg = _Inst->_GetPrivate()->tagRegistry.get()) {
        reg->Advertise(this, fields);
    }
}

void Worker::Log(LogLevel lvl, fmt::string_view fmt, fmt::format_args args)
{
    _Inst->Log(lvl, _LogCat.c_str(), fmt, args);
}

lua_State *Worker::LuaState() const
{
    return _Inst->LuaState();
}

QVariant Worker::CurrentSender()
{
    return _Impl ? _Impl->currentSender : QVariant{};
}

void Worker::Destroy() {
    emit ShutdownDone();
}

fut::Future<void> Worker::shutdown() {
    fut::SharedPromise<void> promise;
    auto future = promise.GetFuture();
    // complete once the worker is actually gone: Destroy() emits ShutdownDone
    // (possibly deferred for async cleanup), which schedules deleteLater, whose
    // destroyed() resolves the promise. So await(worker:shutdown()) guarantees
    // the worker has been freed, not merely stopped.
    connect(this, &Worker::ShutdownDone, this, &QObject::deleteLater, Qt::UniqueConnection);
    connect(this, &QObject::destroyed, this, [promise] {
        // resolve on the next tick, not inside ~QObject: the continuation may
        // resume a coroutine or trigger more shutdowns, which must not run while
        // this worker is mid-destruction (it would reenter teardown and crash)
        QTimer::singleShot(0, [promise] { promise(); });
    });
    Destroy();
    return future;
}

Worker::~Worker()
{
}

struct FactoryContext {
    string name;
    Factory factory;
    ExtraMethods methods;
};
DESCRIBE("radapter::FactoryContext", FactoryContext, void) {}

static int get_listeners(lua_State* L) {
    auto* cls = lua_tostring(L, lua_upvalueindex(1));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, cls));
    Push(L, ud->listeners);
    return 1;
}


static int worker_destroy(lua_State* L) {
    auto* cls = lua_tostring(L, lua_upvalueindex(1));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, cls));
    auto w = ud->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    w->Destroy();
    w->deleteLater();
    w = nullptr;
    return 0;
}

static int worker_shutdown(lua_State* L) {
    auto* cls = lua_tostring(L, lua_upvalueindex(1));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, cls));
    auto w = ud->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    auto fut = w->shutdown().ThenSync([]() -> QVariant { return QVariant{}; });
    glua::Push(L, makeLuaPromise(w, fut));
    return 1;
}

static int worker_call(lua_State* L) {
    auto* cls = lua_tostring(L, lua_upvalueindex(1));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, cls));
    auto w = ud->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    auto was = ud->currentSender;
    defer revert([&]{
        ud->currentSender = was;
    });
    ud->currentSender = builtin::help::toQVar(L, 3);
    w->OnMsg(builtin::help::toQVar(L, 2));
    return 1;
}

static int call_extra(lua_State* L) {
    auto* cls = lua_tostring(L, lua_upvalueindex(1));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, cls));
    auto w = ud->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    auto* method = reinterpret_cast<ExtraMethod>(lua_touserdata(L, lua_upvalueindex(2)));
    glua::Push(L, method(w, builtin::help::toArgs(L, 2)));
    return 1;
}

struct radWorkerEvents {
    LuaUserData worker;

    LuaValue get_listeners() {
        return static_cast<WorkerImpl*>(worker.UnsafeData())->evListeners;
    }
};

RAD_DESCRIBE(radWorkerEvents) {
    RAD_MEMBER(get_listeners);
}

static Worker* checkWorker(lua_State* L) {
    auto* ud = static_cast<WorkerImpl*>(lua_touserdata(L, 1));
    auto w = ud->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    return w;
}

static int worker_index(lua_State* L) {
    constexpr string_view events = "events";
    constexpr string_view name = "name";
    constexpr string_view origin = "origin";
    auto key = lua_type(L, 2) == LUA_TSTRING ? string_view{lua_tostring(L, 2)} : string_view{};
    if (key == events) {
        glua::Push(L, radWorkerEvents{LuaUserData(L, 1)});
    } else if (key == name) {
        auto n = checkWorker(L)->objectName().toStdString();
        lua_pushlstring(L, n.data(), n.size());
    } else if (key == origin) {
        auto& o = checkWorker(L)->_Origin;
        lua_pushlstring(L, o.data(), o.size());
    } else {
        lua_rawget(L, lua_upvalueindex(1));
    }
    return 1;
}

static void worker_notify(WorkerImpl* impl, QVariant const& msg, int workerSelfRef, bool is_event) {
    if (!msg.isValid()) return;
    auto* L = impl->L;
    auto* w = impl->self.data();
    if (!w) {
        Raise("worker not usable");
    }
    if (auto* reg = w->_Inst->_GetPrivate()->tagRegistry.get()) {
        if (is_event) reg->onWorkerEvent(w, msg);
        else          reg->onWorkerMsg(w, msg);
    }
    if (!lua_checkstack(L, 4)) {
        w->Error("Could not reserve stack to send {}", is_event ? "msg" : "event");
        return;
    }
    lua_pushcfunction(L, builtin::traceback);
    auto msgh = lua_gettop(L);
    lua_getglobal(L, "call_all");
    Push(L, is_event ? impl->evListeners : impl->listeners);
    glua::Push(L, msg);
    lua_rawgeti(L, LUA_REGISTRYINDEX, workerSelfRef);
    auto ok = lua_pcall(L, 3, 0, msgh);
    if (ok != LUA_OK) {
        w->Error("Could not notify listeners: {}", lua_tostring(L, -1));
    }
    lua_settop(L, msgh - 1);
}

static int worker_tostring(lua_State* L) {
    auto* impl = static_cast<WorkerImpl*>(lua_touserdata(L, 1));
    auto cls = lua_tostring(L, lua_upvalueindex(1));
    auto* w = impl ? impl->self.data() : nullptr;
    if (!w) {
        lua_pushfstring(L, "%s(destroyed)", cls);
    } else if (auto name = w->objectName(); !name.isEmpty()) {
        lua_pushfstring(L, "%s(%p, \"%s\")", cls, (void*)w, name.toUtf8().constData());
    } else {
        lua_pushfstring(L, "%s(%p)", cls, (void*)w);
    }
    return 1;
}

static void push_worker(lua_State* L, Instance* inst, const char* clsname, Worker* w, ExtraMethods const& methods)
{
    lua_pushstring(L, clsname);
    auto clsIdx = lua_gettop(L);

    auto* ud = lua_udata(L, sizeof(WorkerImpl));
    // L may be a (collectable) coroutine: keep the main state for later use
    auto* mainL = inst->LuaState();
    auto* impl = new (ud) WorkerImpl{mainL};

    impl->self = w;
    w->_Impl = impl;

    lua_pushvalue(L, -1); // prevent gc, while actual worker is alive
    auto workerSelfRef = luaL_ref(L, LUA_REGISTRYINDEX);
    w->_luaSelfRef = workerSelfRef;
    QObject::connect(w, &QObject::destroyed, w, [=]{
        luaL_unref(mainL, LUA_REGISTRYINDEX, workerSelfRef);
    });

    lua_createtable(L, 0, 0); //subs
    impl->listeners = LuaValue(L, ConsumeTop);
    lua_createtable(L, 0, 0); //evSubs
    impl->evListeners = LuaValue(L, ConsumeTop);

    impl->conns[0] = QObject::connect(w, &Worker::SendEvent, w, [=](QVariant const& msg){
        worker_notify(impl, msg, workerSelfRef, true);
    });
    impl->conns[1] = QObject::connect(w, &Worker::SendMsg, w, [=](QVariant const& msg){
        worker_notify(impl, msg, workerSelfRef, false);
    });

    if (luaL_newmetatable(L, clsname)) {

        lua_pushlightuserdata(L, &builtin::workers::Marker);
        lua_setfield(L, -2, "__marker");

        lua_pushvalue(L, clsIdx);
        lua_pushcclosure(L, worker_tostring, 1);
        lua_setfield(L, -2, "__tostring");

        lua_pushvalue(L, clsIdx);
        lua_pushcclosure(L, glua::protect<worker_destroy>, 1);
        lua_setfield(L, -2, "destroy");

        lua_pushvalue(L, clsIdx);
        lua_pushcclosure(L, glua::protect<worker_shutdown>, 1);
        lua_setfield(L, -2, "shutdown");

        lua_pushvalue(L, clsIdx);
        lua_pushcclosure(L, glua::protect<get_listeners>, 1);
        lua_setfield(L, -2, "get_listeners");

        lua_pushvalue(L, clsIdx);
        lua_pushcclosure(L, glua::protect<worker_call>, 1);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__call");
        lua_setfield(L, -2, "call");

        for (auto it = methods.begin(); it != methods.end(); ++it) {
            auto name = it.key().toStdString();
            auto method = it.value();
            glua::Push(L, it.key().toStdString());
            static_assert(sizeof(void*) >= sizeof(method));
            lua_pushvalue(L, clsIdx);
            lua_pushlightuserdata(L, reinterpret_cast<void*>(method));
            lua_pushcclosure(L, glua::protect<call_extra>, 2);
            lua_settable(L, -3);
        }

        lua_pushcfunction(L, glua::dtor_for<WorkerImpl>);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, glua::protect<worker_index>, 1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    lua_insert(L, -2);
    lua_pop(L, 1);
    emit inst->WorkerCreated(w);
}

static int workerFactory(lua_State* L) {
    auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto ctorArgs = builtin::help::toArgs(L, 1);
    auto* inst = Instance::FromLua(L);
    auto* d = inst->_GetPrivate();
    auto was = std::exchange(d->currentCaller, L);
    defer revert([&]{
        d->currentCaller = was;
    });
    auto* w = ctx->factory(ctorArgs, inst);
    push_worker(L, inst, ctx->name.c_str(), w, ctx->methods);
    return 1;
}

void Instance::RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra)
{
    auto L = d->L;
    glua::Push(L, FactoryContext{name, factory, extra});
    lua_pushcclosure(L, glua::protect<workerFactory>, 1);
    lua_setglobal(L, name);
}

} //radapter
