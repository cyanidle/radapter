#pragma once

#include <QVariant>
#include <QString>
#include <QObject>
#include <QThread>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <future/move_func.hpp>
#include "rpcxx/utils.hpp"
#include "future/future.hpp"
#include "rc/rc.hpp"

struct lua_State;

namespace radapter
{

class Instance;
class Fiber;

struct RADAPTER_API FiberPool : QObject
{
    Q_OBJECT

public:
    FiberPool(Instance* inst, size_t pooledFibers = 8, size_t stackSize = 4 * 1024 * 1024);
    ~FiberPool() override;

    FiberPool(FiberPool const&) = delete;
    FiberPool& operator=(FiberPool const&) = delete;

    void Run(fut::MoveFunc<void(Fiber*)> closure);

    // Must not be called from a Fiber
    void Stop(unsigned timeout);
signals:
    void idle();

private:
    friend class Fiber;
    struct StepGuard;
    struct Impl;
    std::unique_ptr<Impl> d;
};

struct RADAPTER_API ForcedShutdown : std::exception
{
    ForcedShutdown();
    char const *what() const noexcept override;
};

class RADAPTER_API Fiber : public rc::DefaultBase
{
public:
    lua_State* LuaState();
    size_t SuspendCount() const;
    bool Unwinding() const;

    static Fiber* Current();

    ~Fiber();
private:
    friend struct FiberPool;
    Fiber(FiberPool* pool);
    friend void Await(fut::Future<void> signal);
    struct Impl;
    std::unique_ptr<Impl> d;
};

void Await(fut::Future<void> signal);

template<typename T>
T Await(fut::Future<T> fut) {
    std::optional<T> value;
    auto sig = fut.ThenSync([&](T res) {
        value.emplace(std::move(res));
    });
    Await(std::move(sig));
    return std::move(*value);
}

} //radapter
