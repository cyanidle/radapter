#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <future/future.hpp>
#include "radapter/worker.hpp"

namespace radapter
{

using namespace fut;

template<typename T>
void resolveLuaCallback(Worker* worker, Future<T>& fut, LuaFunction& func) {
    fut.AtLastSync([worker, cb = std::move(func)](Result<T> res) mutable noexcept {
        if (!cb) {
            if (!res) worker->Error("Unhandled error (invalid callback): {}");
            return;
        }
        QVariantList args;
        if (res) {
            args = {res.get(), QVariant{}};
        } else {
            try {
                std::rethrow_exception(res.get_exception());
            } catch (std::exception& e) {
                args = {QVariant{}, e.what()};
            }
        }
        try {
            cb(std::move(args));
        } catch (std::exception& e) {
            worker->Error("Error in callback: {}", e.what());
        }
    });
}


template<typename T>
QVariant makeLuaPromise(Worker* worker, Future<T>& future) {
    return MakeFunction([worker, _state = future.TakeState()](Instance*, QVariantList args) mutable -> QVariant {
        auto cb = args.value(0).value<LuaFunction>();
        if (!cb) {
            throw Err("Expected function as single argument");
        }
        auto fut = Future(_state);
        resolveLuaCallback(worker, fut, cb);
        return {};
    });
}

}
