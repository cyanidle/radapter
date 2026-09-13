#include "radapter/fibers.hpp"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <boost/context/protected_fixedsize_stack.hpp>
#include <boost/coroutine2/coroutine.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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

using push_type = boost::coroutines2::coroutine<void>::push_type;
using pull_type = boost::coroutines2::coroutine<void>::pull_type;

thread_local Fiber* t_current = nullptr;

struct Fiber::Impl
{
    Fiber* self;
    FiberPool* pool;

    std::optional<pull_type> step;
    push_type* yield = nullptr;

    bool busy = true;

    std::exception_ptr unwind = nullptr;
    std::exception_ptr exc = nullptr;
    fut::MoveFunc<void(Fiber*)> closure;
    size_t suspendCount = 0;

    LuaValue thread;

    void afterStep();
    void operator()(push_type& yield);
    void await(fut::Future<void>& sig);
    void resume(std::exception_ptr exc);
};

struct FiberPool::Impl
{
    FiberPool* self;
    Instance* inst;
    size_t pooledFibers;
    size_t stackSize;

    std::vector<rc::Strong<Fiber>> parked;
    std::set<Fiber*> flying;
    std::mutex stopMutex;

    void returnFiber(Fiber* fib) {
        if (parked.size() < pooledFibers)
            parked.push_back(fib);
        flying.erase(fib);
        if (flying.empty())
            emit self->idle();
    }
};

struct FiberPool::StepGuard
{
    Fiber* was;

    explicit StepGuard(Fiber* f) {
        was = std::exchange(t_current, f);
    }

    ~StepGuard() {
        t_current = was;
    }
};

Fiber::Fiber(FiberPool* pool) : d()
{
    d->pool = pool;
    d->self = this;
    d->step.emplace(boost::context::protected_fixedsize_stack{pool->d->stackSize}, std::ref(*d.data()));
}

void Fiber::Impl::operator()(push_type& yield)
{
    this->yield = &yield;
    while (yield) {
        yield();
        busy = true;
        Q_ASSERT(exc == nullptr);
        Q_ASSERT(suspendCount == 0);
        try {
            closure(self);
        } catch (ForcedShutdown const&) {
            // pass
        } catch (...) {
            exc = std::current_exception();
        }
        busy = false;
        if (unwind)
            std::rethrow_exception(unwind);
    }
}

FiberPool::FiberPool(Instance* inst, size_t pooledFibers, size_t stackSize)
    : d()
{
    d->self = this;
    d->inst = inst;
    d->pooledFibers = pooledFibers;
    d->stackSize = stackSize;
}

FiberPool::~FiberPool()
{
    Stop(1);
}

void Fiber::Impl::afterStep() {
    if (!busy)
        pool->d->returnFiber(self);
    if (unwind)
        throw ForcedShutdown{};
}

void FiberPool::Run(fut::MoveFunc<void(Fiber*)> closure)
{
    if (auto* current = t_current) {
        closure(current);
        return;
    }
    rc::Strong<Fiber> fiber;
    if (d->parked.empty()) {
        fiber = new Fiber(this);
    } else {
        fiber = std::move(d->parked.back());
        d->parked.pop_back();
    }
    d->flying.insert(fiber.get());
    fiber->d->closure = std::move(closure);
    {
        StepGuard guard_(fiber.get());
        fiber->d->step.value()();
    }
    fiber->d->afterStep();
}

void FiberPool::Stop(unsigned timeout)
{
    QDeadlineTimer deadline(timeout);
    while (d->flying.size() && !deadline.hasExpired()) {
        QEventLoop loop;
        QTimer::singleShot(int(deadline.remainingTime()), &loop, &QEventLoop::quit);
        connect(this, &FiberPool::idle, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QEventLoop loop;
    connect(this, &FiberPool::idle, &loop, &QEventLoop::quit, Qt::QueuedConnection);
    for (auto f: d->flying) {
        f->d->step.reset();
    }
    loop.exec();
}

void Fiber::Impl::await(fut::Future<void>& sig)
{
    if (unwind) {
        throw ForcedShutdown{};
    }
    sig.AtLastSync([f = rc::Strong<Fiber>(self)](fut::Result<void> res) mutable {
        QMetaObject::invokeMethod(f->d->pool->d->inst, [MV(f), exc = res.get_exception()]() mutable {
            f->d->resume(std::move(exc));
        }, Qt::QueuedConnection);
    });
    ++suspendCount;
    try {
        (*yield)();
    } catch (boost::context::detail::forced_unwind&) {
        unwind = std::current_exception();
        throw ForcedShutdown{};
    }
    if (exc) {
        std::rethrow_exception(std::exchange(exc, nullptr));
    }
}

void Fiber::Impl::resume(std::exception_ptr exc)
{
    suspendCount--;
    this->exc = std::move(exc);
    {
        FiberPool::StepGuard guard(self);
        step.value()();
    }
    afterStep();
}

Fiber::~Fiber()
{
}

lua_State* Fiber::LuaState()
{
    Instance* inst = d->pool->d->inst;
    auto* L = inst->LuaState();
    lua_State* T;
    if (d->thread) {
        d->thread.Push(L);
        T = lua_tothread(L, -1);
        lua_pop(L, 1);
    } else {
        T = lua_newthread(L);
        d->thread = {L, ConsumeTop};
        inst->_GetPrivate()->newThreadCreated(T);
    }
    return T;
}

size_t Fiber::SuspendCount() const
{
    return d->suspendCount;
}

Fiber* Fiber::Current()
{
    return t_current;
}

static void awaitBlocking(fut::Future<void>& signal)
{
    auto* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread()) {
        Raise("await() without a fiber on the event loop thread would deadlock");
    }
    std::mutex mx;
    std::condition_variable cv;
    bool settled = false;
    std::exception_ptr exc = nullptr;
    signal.AtLastSync([&](fut::Result<void> res) {
        if (!res) {
            exc = std::move(res).get_exception();
        }
        std::lock_guard lk(mx);
        settled = true;
        cv.notify_all();
    });
    std::unique_lock lk(mx);
    cv.wait(lk, [&] { return settled; });
    if (exc) {
        std::rethrow_exception(exc);
    }
}

void radapter::Await(fut::Future<void> signal)
{
    if (Fiber* fiber = t_current) {
        fiber->d->await(signal);
    } else {
        awaitBlocking(signal);
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
    AwaitRes out;
    try {
        out = Await(std::move(fut));
    } catch (ForcedShutdown const&) {
        return luaL_error(L, "Forced shutdown");
    }
    glua::Push(L, out.res);
    glua::Push(L, out.err);
    return 2;
}

static QVariant makeLuaPromise(lua_State* L, QPointer<Worker> worker, std::string cbCtx, Future<QVariant>& future)
{
    fut::MultiFuture<QVariant> multi{std::move(future)};
    lua_getfield(L, LUA_REGISTRYINDEX, "radapter_promise_mt");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        Raise("promise metatable is not registered (async.lua missing?)");
    }
    lua_createtable(L, 0, 1);
    QVariant subscribe = MakeFunction(
        [multi = std::move(multi), worker = std::move(worker), cbCtx = std::move(cbCtx)](
            Instance*, QVariantList const& args) mutable -> QVariant {
            auto cb = args.value(0).value<LuaFunction>();
            if (!cb) {
                Raise("promise: expected a callback function");
            }
            auto fut = multi.GetFuture();
            ResolveLuaCallback(worker.data(), fut, cb, cbCtx);
            return {};
        });
    glua::Push(L, subscribe);
    lua_setfield(L, -2, "_subscribe");
    lua_pushvalue(L, -2);
    lua_setmetatable(L, -2);
    lua_remove(L, -2);
    return QVariant::fromValue(LuaValue(L, ConsumeTop));
}

QVariant radapter::MakeLuaPromise(Worker* worker, Future<QVariant>& future)
{
    return makeLuaPromise(worker->LuaState(), QPointer(worker), {}, future);
}

QVariant radapter::MakeLuaPromise(Instance* instance, Future<QVariant>& future)
{
    return makeLuaPromise(instance->LuaState(), QPointer<Worker>{}, "spawn callback", future);
}

char const *radapter::ForcedShutdown::what() const noexcept {
    return "Forced shutdown";
}

radapter::ForcedShutdown::ForcedShutdown()
{

}
