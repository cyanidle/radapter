#include <future/future.hpp>
#include <QPointer>

#include "radapter/radapter.hpp"
#include "radapter/worker.hpp"

namespace radapter
{

using namespace fut;

template<typename T>
void resolveLuaCallback(Worker* worker, Future<T>& fut, LuaFunction& func) {
    // QPointer: the future may resolve as/after the worker is destroyed (e.g.
    // Worker::shutdown resolves on destroyed), so never deref a dead worker.
    fut.AtLastSync([worker = QPointer(worker), cb = std::move(func)](Result<T> res) mutable noexcept {
        if (!cb) {
            if (!res && worker) worker->Error("Unhandled error (invalid callback): {}");
            return;
        }
        QVariantList args;
        try {
            args = {res.get(), QVariant{}};
        } catch (std::exception& e) {
            args = {QVariant{}, e.what()};
        }
        try {
            cb.Call(std::move(args));
        } catch (std::exception& e) {
            if (worker) worker->Error("Error in callback: {}", e.what());
        }
    });
}


template<typename T>
QVariant makeLuaPromise(Worker* worker, Future<T>& future) {
    return MakeFunction([worker, _state = future.TakeState()](Instance*, QVariantList args) mutable -> QVariant {
        auto cb = args.value(0).value<LuaFunction>();
        if (!cb) {
            Raise("Expected function as single argument");
        }
        auto fut = Future(_state);
        resolveLuaCallback(worker, fut, cb);
        return {};
    });
}


}
