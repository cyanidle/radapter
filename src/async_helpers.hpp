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
            cb.Call(std::move(args));
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
            Raise("Expected function as single argument");
        }
        auto fut = Future(_state);
        resolveLuaCallback(worker, fut, cb);
        return {};
    });
}

inline Future<QVariant> fromLuaPromise(LuaFunction func, QVariantList args)
{
    SharedPromise<QVariant> promise;
    Future<QVariant> res = promise.GetFuture();
    QVariant on_done = MakeFunction([promise](Instance* inst, QVariantList args) mutable -> QVariant {
        if (args.size() == 0) {
            promise(QVariant{});
        } else if (args.size() == 1) {
            promise(args.at(0));
        } else if (args.size() == 2) {
            auto res = args.at(0);
            auto err = args.at(1);
            if (res.isValid()) {
                promise(std::move(res));
            } else {
                if (!err.canConvert<QString>()) {
                    inst->Error("async", "In async function: second return value (res, _err_) should be convertible to string");
                }
                promise(std::make_exception_ptr(std::runtime_error(err.toString().toStdString())));
            }
        } else {
            Raise("Expected 0, 1 or 2 return values from async function. ([ok], [err])");
        }
        return {};
    });
    try {
        args.append(on_done);
        auto res = func.Call(args);
        if (res.isValid()) {
            promise(res);
        }
    } catch (...) {
        promise(std::current_exception());
    }
    return res;
}


}
