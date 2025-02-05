#ifndef RADAPTER_VALUE
#define RADAPTER_VALUE
#pragma once

#include "radapter/common.hpp"
#include <QVariantList>

struct lua_State;

namespace radapter {


class Instance;

enum ConsumeTopTag {ConsumeTop};

struct RADAPTER_API LuaValue {
    LuaValue(lua_State* L, int idx);
    LuaValue(lua_State* L, ConsumeTopTag);

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


struct RADAPTER_API LuaUserData : LuaValue {
    using LuaValue::LuaValue;

    using Dtor = void(*)(void*);

    template<typename T>
    LuaUserData Create(lua_State* L, const char* tname, T value) noexcept {
        auto* mem = init(L, sizeof(T));
        new (mem) T{std::move(value)};
        return initDone(L, tname, [](void* d){ static_cast<T*>(d)->~T(); });
    }

    void* Data(const char* tname);
    void* UnsafeData();

private:
    static void* init(lua_State* L, size_t sz);
    static LuaUserData initDone(lua_State* L, const char* tname, Dtor dtor);
};

}

Q_DECLARE_METATYPE(radapter::LuaValue)
Q_DECLARE_TYPEINFO(radapter::LuaValue, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(radapter::LuaUserData)
Q_DECLARE_TYPEINFO(radapter::LuaUserData, Q_MOVABLE_TYPE);

#endif //RADAPTER_VALUE
