#ifndef RADAPTER_VALUE
#define RADAPTER_VALUE
#pragma once

#include "radapter/common.hpp"
#include <QVariantList>

struct lua_State;

namespace radapter {


class Instance;

struct LuaValue {
    LuaValue(lua_State* L, int idx);

    LuaValue();
    LuaValue(const LuaValue& o);
    LuaValue(LuaValue&& o) noexcept :
        _L(std::exchange(o._L, nullptr)),
        _ref(o._ref)
    {}
    LuaValue& operator=(const LuaValue& o);
    LuaValue& operator=(LuaValue&& o) noexcept {
        if (this != &o) {
            this->~LuaValue();
            _L = std::exchange(o._L, nullptr);
            _ref = o._ref;
        }
        return *this;
    }
    explicit operator bool() const noexcept;
    bool operator==(LuaValue const& o) const noexcept;
    bool IsValid() const noexcept {
        return bool(*this);
    }
    ~LuaValue();

    lua_State* _L;
    int _ref;
};


struct LuaUserData : LuaValue {
    using LuaValue::LuaValue;

    void* Data();
};

}

Q_DECLARE_METATYPE(radapter::LuaValue)
Q_DECLARE_TYPEINFO(radapter::LuaValue, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(radapter::LuaUserData)
Q_DECLARE_TYPEINFO(radapter::LuaUserData, Q_MOVABLE_TYPE);

#endif //RADAPTER_VALUE
