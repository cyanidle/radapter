#ifndef RADAPTER_LUA_FUNC_HPP
#define RADAPTER_LUA_FUNC_HPP

#include "radapter/common.hpp"
#include <functional>
#include <QVariantList>

struct lua_State;

namespace radapter
{

class Instance;

struct LuaFunction {
    LuaFunction();
    LuaFunction(const LuaFunction& o);
    LuaFunction(LuaFunction&& o) noexcept :
        _L(std::exchange(o._L, nullptr)),
        _ref(o._ref)
    {}
    LuaFunction& operator=(const LuaFunction& o);
    LuaFunction& operator=(LuaFunction&& o) noexcept {
        if (this != &o) {
            this->~LuaFunction();
            _L = std::exchange(o._L, nullptr);
            _ref = o._ref;
        }
        return *this;
    }
    LuaFunction(lua_State* L, int idx);
    explicit operator bool() const noexcept;
    bool operator==(LuaFunction const& o) const noexcept;
    bool IsValid() const noexcept {
        return bool(*this);
    }
    QVariant Call(const QVariantList &args) const;
    QVariant operator()(QVariantList const& args) const {
        return Call(args);
    }
    ~LuaFunction();

    lua_State* _L;
    int _ref;
};

using ExtraFunction = std::function<QVariant(Instance*, QVariantList const&)>;

}

Q_DECLARE_METATYPE(radapter::LuaFunction)
Q_DECLARE_TYPEINFO(radapter::LuaFunction, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(radapter::ExtraFunction)
Q_DECLARE_TYPEINFO(radapter::ExtraFunction, Q_MOVABLE_TYPE);

#endif //RADAPTER_LUA_FUNC_HPP
