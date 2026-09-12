#ifndef RADAPTER_LUA_FUNC_HPP
#define RADAPTER_LUA_FUNC_HPP

#include "radapter/value.hpp"
#include <functional>
#include <memory>
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
