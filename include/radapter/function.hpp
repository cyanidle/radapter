#ifndef RADAPTER_LUA_FUNC_HPP
#define RADAPTER_LUA_FUNC_HPP

#include "radapter/value.hpp"
#include <functional>


namespace radapter
{

struct RADAPTER_API LuaFunction : LuaValue {
    using LuaValue::LuaValue;

    enum TracebackMode {
        NoTraceback = 0,
        Traceback = 1,
    };

    QVariant Call(const QVariantList &args, TracebackMode mode = Traceback) const;
};

using ExtraFunction = std::function<QVariant(Instance*, QVariantList const&)>;

}

Q_DECLARE_METATYPE(radapter::LuaFunction)
Q_DECLARE_TYPEINFO(radapter::LuaFunction, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(radapter::ExtraFunction)
Q_DECLARE_TYPEINFO(radapter::ExtraFunction, Q_MOVABLE_TYPE);

#endif //RADAPTER_LUA_FUNC_HPP
