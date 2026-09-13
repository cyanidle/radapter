#ifndef RADAPTER_LUA_FUNC_HPP
#define RADAPTER_LUA_FUNC_HPP

#include "radapter/value.hpp"
#include <functional>
#include <memory>
#include <string_view>
#include "future/move_func.hpp"

namespace fut
{

template<typename T>
struct Future;

}


namespace radapter
{

struct RADAPTER_API LuaFunction : LuaValue {
    using LuaValue::LuaValue;

    /// The global `name` as a callable, for the Lua-side helpers (`call_all`, ...)
    static LuaFunction Global(lua_State* L, char const* name);

    /// Calls on a fiber of the instance, reporting a lua error as its traceback through
    /// `onError`, which runs on that fiber - so it may raise before a synchronous entry
    /// returns. Nothing runs while the fiber is being torn down.
    void CallOnFiber(QVariantList args, fut::MoveFunc<void(std::string_view err)> onError) const;

    void CallNoWait(const QVariantList &args, std::string ctx) const;
    fut::Future<QVariant> Call(QVariantList args) const;
};

using ExtraFunction = fut::MoveFunc<QVariant(Instance*, QVariantList const&)>;
using ExtraFunctionPtr = std::shared_ptr<ExtraFunction>;

}

Q_DECLARE_METATYPE(radapter::LuaFunction)
Q_DECLARE_TYPEINFO(radapter::LuaFunction, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(radapter::ExtraFunctionPtr)
Q_DECLARE_TYPEINFO(radapter::ExtraFunctionPtr, Q_RELOCATABLE_TYPE);

#endif //RADAPTER_LUA_FUNC_HPP
