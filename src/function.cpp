#include "radapter/function.hpp"
#include "builtin.hpp"
#include "future/future.hpp"

using namespace radapter;

QVariant LuaFunction::Call(QVariantList const& args, TracebackMode trace) const
{
    if (!(*this)) {
        Raise("Attempt to call invalid lua function");
    }
    if (!lua_checkstack(_L, args.size() + 2)) {
        Raise("Could not reserve stack for call");
    }
    if (trace) {
        lua_pushcfunction(_L, builtin::traceback);
    }
    auto msgh = trace ? lua_gettop(_L) : 0;
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    for (auto& a: args) {
        glua::Push(_L, a);
    }
    auto status = lua_pcall(_L, args.size(), 1, msgh);
    if (status != LUA_OK) {
        Raise("{}", lua_tostring(_L, -1));
    }
    return builtin::help::toQVar(_L);
}

fut::Future<QVariant> LuaFunction::CallAsync(QVariantList args, TracebackMode mode) const
{
    fut::SharedPromise<QVariant> promise;
    fut::Future<QVariant> res = promise.GetFuture();
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
        auto res = Call(args, mode);
        if (res.isValid()) {
            promise(res);
        }
    } catch (...) {
        promise(std::current_exception());
    }
    return res;
}