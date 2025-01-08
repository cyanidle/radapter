#include "radapter/worker.hpp"
#include "instance_impl.hpp"
#include "glua/glua.hpp"
#include "worker_impl.hpp"

namespace radapter
{

Worker::Worker(Instance *parent, const char *category) : QObject(parent), _category(category)
{
    _Inst = parent;
}

void Worker::Log(LogLevel lvl, fmt::string_view fmt, fmt::format_args args)
{
    _Inst->Log(lvl, _category, fmt, args);
}

lua_State *Worker::LuaState() const
{
    return _Inst->LuaState();
}

QVariant Worker::CurrentSender()
{
    return _Impl ? _Impl->currentSender : QVariant{};
}

void Worker::Shutdown() {
    emit ShutdownDone();
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
    auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, ctx->name.c_str()));
    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->listenersRef);
    return 1;
};

static int worker_call(lua_State* L) {
    auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* ud = static_cast<WorkerImpl*>(luaL_checkudata(L, 1, ctx->name.c_str()));
    auto was = ud->currentSender;
    defer revert([&]{
        ud->currentSender = was;
    });
    ud->currentSender = builtin::help::toQVar(L, 3);
    auto w = ud->self.data();
    if (!w) {
        throw Err("worker not usable");
    }
    w->OnMsg(builtin::help::toQVar(L, 2));
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
    glua::Push(L, method(w, builtin::help::toArgs(L, 2)));
    return 1;
}

static int workerFactory(lua_State* L) {
    auto* ctx = static_cast<FactoryContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto ctorArgs = builtin::help::toArgs(L, 1);
    auto* inst = Instance::FromLua(L);
    auto* w = ctx->factory(ctorArgs, inst);
    auto* ud = lua_udata(L, sizeof(WorkerImpl));
    auto* impl = new (ud) WorkerImpl{L};

    impl->self = w;
    w->_Impl = impl;

    lua_pushvalue(L, -1); // prevent gc, while actual worker is alive
    auto workerSelfRef = luaL_ref(L, LUA_REGISTRYINDEX);
    QObject::connect(w, &QObject::destroyed, w, [=]{
        luaL_unref(L, LUA_REGISTRYINDEX, workerSelfRef);
    });

    lua_createtable(L, 0, 0); //subs
    impl->listenersRef = luaL_ref(L, LUA_REGISTRYINDEX);

    impl->conn = QObject::connect(w, &Worker::SendMsg, w, [=](QVariant const& msg){
        if (!lua_checkstack(L, 4)) {
            w->Error("Could not reserve stack to send msg");
            return;
        }
        lua_pushcfunction(L, builtin::help::traceback);
        auto msgh = lua_gettop(L);
        lua_getglobal(L, "call_all");
        lua_rawgeti(L, LUA_REGISTRYINDEX, impl->listenersRef);
        glua::Push(L, msg);
        lua_rawgeti(L, LUA_REGISTRYINDEX, workerSelfRef);
        auto ok = lua_pcall(L, 3, 0, msgh);
        if (ok != LUA_OK) {
            w->Error("Could not notify listeners: {}", lua_tostring(L, -1));
        }
        lua_settop(L, msgh - 1);
    });
    if (luaL_newmetatable(L, ctx->name.c_str())) {

        lua_pushlightuserdata(L, &builtin::workers::Marker);
        lua_setfield(L, -2, "__marker");

        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, glua::protect<get_listeners>, 1);
        lua_setfield(L, -2, "get_listeners");

        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushcclosure(L, glua::protect<worker_call>, 1);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "__call");
        lua_setfield(L, -2, "call");

        for (auto it = ctx->methods.begin(); it != ctx->methods.end(); ++it) {
            auto name = it.key().toStdString();
            auto method = it.value();
            glua::Push(L, it.key().toStdString());
            static_assert(sizeof(void*) >= sizeof(method));
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushlightuserdata(L, reinterpret_cast<void*>(method));
            lua_pushcclosure(L, glua::protect<call_extra>, 2);
            lua_settable(L, -3);
        }

        lua_pushcfunction(L, glua::dtor_for<WorkerImpl>);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
    emit inst->WorkerCreated(w);
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
