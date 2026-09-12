#ifndef RADAPTER_ASYNC_HELPERS_HPP
#define RADAPTER_ASYNC_HELPERS_HPP

#include <future/future.hpp>
#include <optional>
#include <QPointer>

#include "radapter/radapter.hpp"
#include "radapter/worker.hpp"
#include "radapter/function.hpp"

namespace radapter
{

using namespace fut;

template<typename T>
void ResolveLuaCallback(Worker* worker, Future<T>& fut, LuaFunction& func) {
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
        cb.CallNoWait(std::move(args), "worker callback: " + (worker ? worker->_Origin : "<dead>"));
    });
}

QVariant RADAPTER_API MakeLuaPromise(Worker* worker, Future<QVariant>& future);

void RunOnLoop(QObject* ctx, fut::MoveFunc<void()> body);

}

#endif //RADAPTER_ASYNC_HELPERS_HPP
