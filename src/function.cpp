#include "radapter/function.hpp"
#include "radapter/async_helpers.hpp"
#include "builtin.hpp"
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

static bool isCallable(lua_State* L, int idx)
{
    if (lua_isfunction(L, idx)) {
        return true;
    }
    if (!lua_getmetatable(L, idx)) {
        return false;
    }
    lua_getfield(L, -1, "__call");
    auto callable = !lua_isnil(L, -1);
    lua_pop(L, 2);
    return callable;
}

void LuaFunction::CallNoWait(QVariantList const& args, std::string ctx) const
{
    if (!(*this)) {
        Raise("Attempt to call invalid lua function");
    }
    auto* inst = Instance::FromLua(_L);
    inst->Fibers()->Run([inst, self = *this, args, MV(ctx)](Fiber* f) {
        try {
            callOn(f->LuaState(), self, args, false);
        } catch (ForcedShutdown const&) {
            throw;
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
    inst->Fibers()->Run([self = *this, MV(promise), args = std::move(args)](Fiber* f) mutable {
        try {
            promise(callOn(f->LuaState(), self, args, true));
        } catch (ForcedShutdown const&) {
            throw;
        } catch (...) {
            promise(std::current_exception());
        }
    });
    return res;
}

int builtin::api::SpawnNative(lua_State* L)
{
    if (!isCallable(L, 1)) {
        luaL_argerror(L, 1, "function or callable expected");
    }
    auto* inst = Instance::FromLua(L);
    auto args = builtin::help::toArgs(L, 2);
    auto fn = LuaFunction(L, 1);
    fut::Promise<QVariant> promise;
    auto fut = promise.GetFuture();
    QMetaObject::invokeMethod(inst, [inst, fn, args = std::move(args), MV(promise)]() mutable {
        inst->Fibers()->Run([fn, args = std::move(args), MV(promise)](Fiber* f) mutable {
            try {
                promise(callOn(f->LuaState(), fn, args, true));
            } catch (ForcedShutdown const&) {
                throw;
            } catch (...) {
                promise(std::current_exception());
            }
        });
    }, Qt::QueuedConnection);
    glua::Push(L, MakeLuaPromise(inst, fut));
    return 1;
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
