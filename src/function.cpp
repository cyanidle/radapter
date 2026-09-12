#include "radapter/function.hpp"
#include "radapter/async_helpers.hpp"
#include "builtin.hpp"
#include <boost/context/detail/exception.hpp>
#include <QMetaObject>
#include <QThread>

using namespace radapter;

static QVariant callOn(lua_State* T, LuaFunction const& fn, QVariantList const& args, bool result)
{
    if (!lua_checkstack(T, int(args.size() + 2))) {
        Raise("Could not reserve stack for call");
    }
    auto base = lua_gettop(T);
    lua_pushcfunction(T, builtin::traceback);
    auto msgh = lua_gettop(T);
    lua_rawgeti(T, LUA_REGISTRYINDEX, fn._ref);
    for (auto& a: args) {
        glua::Push(T, a);
    }
    auto status = lua_pcall(T, int(args.size()), 1, msgh);
    if (status != LUA_OK) {
        auto err = QString::fromUtf8(lua_tostring(T, -1));
        lua_settop(T, base);
        Raise("{}", err);
    }
    if (result) {
        auto res = builtin::help::toQVar(T);
        lua_settop(T, base);
        return res;
    } else {
        lua_settop(T, base);
        return {};
    }
}

void LuaFunction::CallNoWait(QVariantList const& args, std::string ctx) const
{
    if (!(*this)) {
        Raise("Attempt to call invalid lua function");
    }
    auto* inst = Instance::FromLua(_L);
    inst->Fibers()->run([inst, self = *this, args, MV(ctx)](Fiber* f) {
        try {
            callOn(f->LuaState(), self, args, false);
        } catch (std::exception& e) {
            inst->Error(ctx.c_str(), "Uncaught error:\n\t{}", e.what());
        }
    });
}

fut::Future<QVariant> LuaFunction::Call(QVariantList args) const
{
    fut::Promise<QVariant> promise;
    fut::Future<QVariant> res = promise.GetFuture();
    if (!(*this)) {
        promise(std::make_exception_ptr(std::runtime_error("Attempt to call invalid lua function")));
        return res;
    }
    auto* inst = Instance::FromLua(_L);
    inst->Fibers()->run([self = *this, MV(promise), args = std::move(args)](Fiber* f) mutable {
        try {
            promise(callOn(f->LuaState(), self, args, true));
        } catch (boost::context::detail::forced_unwind const&) {
            throw;
        } catch (...) {
            promise(std::current_exception());
        }
    });
    return res;
}

int builtin::api::SpawnNative(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    auto* inst = Instance::FromLua(L);
    QMetaObject::invokeMethod(inst, [fn = LuaFunction(L, 1)]() mutable {
        fn.CallNoWait({}, "spawn");
    }, Qt::QueuedConnection);
    return 0;
}

void radapter::RunOnLoop(QObject* ctx, fut::MoveFunc<void()> body)
{
    if (!Fiber::Current() && QThread::currentThread() == ctx->thread()) {
        body();
        return;
    }
    fut::Promise<void> prom;
    auto fut = prom.GetFuture();
    QMetaObject::invokeMethod(ctx, [body = std::move(body), MV(prom)] () mutable {
        try {
            body();
            prom();
        } catch (...) {
            prom(std::current_exception());
        }
    }, Qt::QueuedConnection);
    Await(std::move(fut));
}
