#include "radapter/fibers.hpp"

#include <boost/coroutine2/coroutine.hpp>
#include <boost/context/protected_fixedsize_stack.hpp>

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "radapter/radapter.hpp"
#include "radapter/async_helpers.hpp"
#include "future/multi_future.hpp"
#include "glua/glua.hpp"
#include "instance_impl.hpp"

using namespace radapter;
namespace coro = boost::coroutines2;

thread_local Fiber* t_current = nullptr;

struct CurrentGuard
{
    Fiber* was;

    explicit CurrentGuard(Fiber* f)
    {
        was = std::exchange(t_current, f);
    }

    ~CurrentGuard()
    {
        t_current = was;
    }
};

struct Fiber::Impl
{
    explicit Impl(FiberPool* pool) : pool(pool) {}

    FiberPool* pool = nullptr;
    lua_State* mainL = nullptr;

    coro::coroutine<void>::push_type* yield = nullptr;
    std::optional<coro::coroutine<void>::pull_type> step;

    fut::MoveFunc<void(Fiber*)> closure;
    std::exception_ptr pendingExc = nullptr;
    std::exception_ptr exc = nullptr;
    std::atomic<bool> wake{false};
    bool done = true;
    size_t suspends = 0;

    rc::Strong<Fiber> selfHold;

    lua_State* thread = nullptr;
    int threadRef = LUA_NOREF;

    void init(Fiber* self);
};

struct FiberPool::Impl
{
    Instance* inst;
    size_t pooledFibers;
    size_t stackSize;

    std::vector<rc::Strong<Fiber>> parked;
    std::vector<Fiber*> flying;
    std::vector<Fiber*> stack;
    size_t noWaitFor = 0;

    Impl(Instance* inst, size_t pooledFibers, size_t stackSize)
        : inst(inst), pooledFibers(pooledFibers), stackSize(stackSize)
    {
    }
};

FiberPool::FiberPool(Instance* inst, size_t pooledFibers, size_t stackSize)
    : d(inst, pooledFibers, stackSize)
{
}

FiberPool::~FiberPool()
{
    unwindStragglers();
    d->parked.clear();
}

rc::Strong<Fiber> FiberPool::takeFiber()
{
    rc::Strong<Fiber> f;
    if (!d->parked.empty()) {
        f = std::move(d->parked.back());
        d->parked.pop_back();
    } else {
        f = new Fiber(this);
        f->d->init(f.get());
    }
    return f;
}

void FiberPool::returnFiber(Fiber* f)
{
    auto* di = f->d.data();
    if (di->thread) {
        lua_settop(di->thread, 0);
    }
    d->flying.erase(std::find(d->flying.begin(), d->flying.end(), f));
    if (d->flying.size() <= d->noWaitFor) {
        emit idle();
    }
    auto hold = std::move(di->selfHold);
    if (d->parked.size() < d->pooledFibers) {
        d->parked.push_back(std::move(hold));
    }
}

void FiberPool::run(fut::MoveFunc<void(Fiber*)> closure)
{
    if (auto* current = t_current) {
        closure(current);
        return;
    }
    auto f = takeFiber();
    f->d->closure = std::move(closure);
    f->d->done = false;
    f->d->selfHold = f;
    d->flying.push_back(f.get());
    d->stack.push_back(f.get());
    {
        CurrentGuard guard_(f.get());
        (*f->d->step)();
    }
    d->stack.pop_back();
    bool completed = f->d->done;
    auto exc = std::move(f->d->pendingExc);
    if (completed) {
        returnFiber(f.get());
    }
    if (exc) {
        std::rethrow_exception(exc);
    }
}

void FiberPool::stop(unsigned timeout)
{
    d->noWaitFor = d->stack.size();
    QDeadlineTimer deadline(timeout);
    while (d->flying.size() > d->noWaitFor && !deadline.hasExpired()) {
        QEventLoop loop;
        QTimer::singleShot(int(deadline.remainingTime()), &loop, &QEventLoop::quit);
        connect(this, &FiberPool::idle, &loop, &QEventLoop::quit);
        loop.exec();
    }
    unwindStragglers();
}

void FiberPool::unwindStragglers()
{
    auto inFlight = std::move(d->flying);
    d->flying.clear();
    for (auto* f : inFlight) {
        if (std::find(d->stack.begin(), d->stack.end(), f) != d->stack.end()) {
            d->flying.push_back(f);
        } else {
            f->unwind();
        }
    }
}

void FiberPool::reportError(std::exception_ptr exc)
{
    try {
        std::rethrow_exception(exc);
    } catch (std::exception& e) {
        d->inst->Error("fibers", "Unhandled error in fiber: {}", e.what());
    } catch (...) {
        d->inst->Error("fibers", "Unhandled non-standard exception in fiber");
    }
}

Fiber::Fiber(FiberPool* pool) : d(pool)
{
}

void Fiber::run()
{
    auto* di = d.data();
    di->suspends = 0;
    di->pendingExc = nullptr;
    auto closure = std::move(di->closure);
    try {
        closure(this);
    } catch (boost::context::detail::forced_unwind const&) {
        throw;
    } catch (...) {
        di->pendingExc = std::current_exception();
    }
    di->done = true;
}

void Fiber::resume()
{
    auto* pool = d->pool;
    pool->d->stack.push_back(this);
    {
        CurrentGuard guard_(this);
        (*d->step)();
    }
    pool->d->stack.pop_back();
    bool completed = d->done;
    auto exc = std::move(d->pendingExc);
    if (completed) {
        pool->returnFiber(this);
    }
    if (exc) {
        pool->reportError(exc);
    }
}

void Fiber::unwind()
{
    auto* di = d.data();
    di->pool = nullptr;
    di->step.reset();
    di->yield = nullptr;
    if (di->threadRef != LUA_NOREF) {
        luaL_unref(di->mainL, LUA_REGISTRYINDEX, di->threadRef);
        di->threadRef = LUA_NOREF;
        di->thread = nullptr;
    }
    di->mainL = nullptr;
    di->selfHold = {};
}

Fiber::~Fiber()
{
    if (d->threadRef != LUA_NOREF && d->mainL) {
        luaL_unref(d->mainL, LUA_REGISTRYINDEX, d->threadRef);
    }
}

lua_State* Fiber::LuaState()
{
    if (!d->thread) {
        d->thread = lua_newthread(d->mainL);
        d->threadRef = luaL_ref(d->mainL, LUA_REGISTRYINDEX);
        d->pool->d->inst->_GetPrivate()->onThread(d->thread);
    }
    return d->thread;
}

size_t Fiber::suspends() const
{
    return d->suspends;
}

void Fiber::Impl::init(Fiber* self)
{
    mainL = pool->d->inst->LuaState();
    step.emplace(
        boost::context::protected_fixedsize_stack{pool->d->stackSize},
        [this, self](coro::coroutine<void>::push_type& yield) {
            this->yield = &yield;
            while (yield) {
                yield();
                self->run();
            }
        });
}

Fiber* Fiber::Current()
{
    return t_current;
}

void radapter::Await(fut::Future<void> signal)
{
    auto* f = t_current;
    if (!f) {
        struct Rendezvous
        {
            std::mutex mx;
            std::condition_variable cv;
            bool settled = false;
            std::exception_ptr exc;
        };
        auto state = std::make_shared<Rendezvous>();
        signal.AtLastSync([state](fut::Result<void> res) {
            if (!res) {
                state->exc = std::move(res).get_exception();
            }
            std::lock_guard lk(state->mx);
            state->settled = true;
            state->cv.notify_all();
        });
        std::unique_lock lk(state->mx);
        auto* app = QCoreApplication::instance();
        if (!state->settled && app && QThread::currentThread() == app->thread()) {
            Raise("await() without a fiber on the event loop thread would deadlock");
        }
        state->cv.wait(lk, [&] { return state->settled; });
        if (state->exc) {
            std::rethrow_exception(state->exc);
        }
        return;
    }
    auto* di = f->d.data();
    di->wake.store(false, std::memory_order_relaxed);
    di->exc = nullptr;
    signal.AtLastSync([f = rc::Strong<Fiber>(f)](fut::Result<void> res) {
        auto* di = f->d.data();
        auto* pool = di->pool;
        if (!pool) {
            return;
        }
        if (!res) {
            di->exc = std::move(res).get_exception();
        }
        if (!di->wake.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        f->resume();
    });
    if (!di->wake.exchange(true, std::memory_order_acq_rel)) {
        ++di->suspends;
        (*di->yield)();
    }
    if (di->exc) {
        auto exc = std::move(di->exc);
        di->exc = nullptr;
        std::rethrow_exception(exc);
    }
}

const char AwaitBoxMt[] = "radapter.awaitbox";

struct AwaitRes
{
    QVariant res, err;
};

struct AwaitBox
{
    fut::SharedPromise<AwaitRes> prom;
};

static int awaitSettleTrampoline(lua_State* L)
{
    auto& prom = static_cast<AwaitBox*>(lua_touserdata(L, lua_upvalueindex(1)))->prom;
    prom(AwaitRes{builtin::help::toQVar(L, 1), builtin::help::toQVar(L, 2)});
    return 0;
}

int builtin::api::AwaitNative(lua_State* L)
{
    if (!lua_isfunction(L, 1)) {
        if (luaL_getmetafield(L, 1, "__call") == LUA_TNIL) {
            Raise("await(): _subscribe must be callable");
        }
        lua_pop(L, 1);
    }
    auto* box = static_cast<AwaitBox*>(lua_newuserdata(L, sizeof(AwaitBox)));
    new (box) AwaitBox{};
    auto fut = box->prom.GetFuture();
    if (luaL_newmetatable(L, AwaitBoxMt)) {
        lua_pushcfunction(L, glua::dtor_for<AwaitBox>);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, glua::protect<awaitSettleTrampoline>, 1);
    lua_pushvalue(L, 1);
    lua_insert(L, -2);
    lua_pcall(L, 1, 0, 0);
    auto out = Await(std::move(fut));
    glua::Push(L, out.res);
    glua::Push(L, out.err);
    return 2;
}

QVariant radapter::MakeLuaPromise(Worker* worker, Future<QVariant>& future)
{
    fut::MultiFuture<QVariant> multi{std::move(future)};
    auto* L = worker->LuaState();
    lua_getfield(L, LUA_REGISTRYINDEX, "radapter_promise_mt");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        Raise("promise metatable is not registered (async.lua missing?)");
    }
    lua_createtable(L, 0, 1);
    QVariant subscribe = MakeFunction(
        [multi = std::move(multi), worker = QPointer(worker)](
            Instance*, QVariantList const& args) mutable -> QVariant {
            auto cb = args.value(0).value<LuaFunction>();
            if (!cb) {
                Raise("promise: expected a callback function");
            }
            auto fut = multi.GetFuture();
            ResolveLuaCallback(worker.data(), fut, cb);
            return {};
        });
    glua::Push(L, subscribe);
    lua_setfield(L, -2, "_subscribe");
    lua_pushvalue(L, -2);
    lua_setmetatable(L, -2);
    lua_remove(L, -2);
    return QVariant::fromValue(LuaValue(L, ConsumeTop));
}
