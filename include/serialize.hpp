#pragma once
#include "describe/describe.hpp"
#include "meta/meta.hpp"
#include "logs.hpp"
#include "lua.hpp"

namespace radapter {

struct MissingOk {};

template<typename T>
void Deserialize(lua_State* L, T& out, int idx = -1)
{
    lua_pushvalue(L, idx);
    if constexpr (describe::is_described_v<T>) {
        constexpr auto desc = describe::Get<T>();
        lua::checkType(L, LUA_TTABLE, -1);
        desc.for_each_field([&](auto f){
            lua_pushlstring(L, f.name.data(), f.name.size());
            lua_rawget(L, -2);
            if constexpr (describe::has_attr_v<MissingOk, decltype(f)>) {
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    return;
                }
            }
            try {
                Deserialize(L, f.get(out));
            } catch (std::exception& exc) {
                throw Err("{}.{}", f.name, exc.what());
            }
            lua_pop(L, 1);
        });
    } else if constexpr (std::is_same_v<T, bool>) {
        lua::checkType(L, LUA_TBOOLEAN, -1);
        out = lua_toboolean(L, -1);
    } else if constexpr (std::is_floating_point_v<T>) {
        lua::checkType(L, LUA_TNUMBER, -1);
        out = lua_tonumber(L, -1);
    } else if constexpr (std::is_constructible_v<string_view, T>) {
        lua::checkType(L, LUA_TSTRING, -1);
        out = lua::ToString(L, -1);
    } else if constexpr (std::is_integral_v<T>) {
        if (!lua_isinteger(L, -1)) {
            throw Err("number should be an integer");
        }
        out = lua_tointeger(L, -1);
    } else if constexpr (meta::is_assoc_container_v<T>) {
        using kT = typename T::key_type;
        static_assert(std::is_constructible_v<string_view, T>, "non-string map keys unsupported");
        lua::IterateTable(L, -1, [&]{
            lua::checkType(L, LUA_TSTRING, -2);
            auto key = lua::ToString(L, -2);
            try {
                deserialize(L, out[key]);
            } catch (std::exception& exc) {
                throw Err("{}.{}", key, exc.what());
            }
        });
    } else if constexpr (meta::is_index_container_v<T>) {
        lua::checkType(L, LUA_TTABLE, -1);
        if (auto len = lua::IsArray(L, -1)) {
            out.resize(len);
            for (auto i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                try {
                    deserialize(L, out[i]);
                } catch (std::exception& exc) {
                    throw Err("[{}].{}", i, exc.what());
                }
                lua_pop(L, 1);
            }
        } else {
            throw Err("value is not an array");
        }
    } else {
        static_assert(std::is_void_v<T>, "Unsupported type");
    }
    lua_pop(L, 1);
}

template<typename T>
T Deserialize(lua_State* L, int idx = -1) {
    T out;
    Deserialize(L, out, idx);
    return out;
}

template<typename T>
void Serialize(lua_State* L, T const& val) {
    if constexpr (describe::is_described_v<T>) {
        constexpr auto desc = describe::Get<T>();
        lua_createtable(L, 0, desc.fields_count);
        desc.for_each_field([&](auto f){
            lua_pushlstring(L, f.name.data(), f.name.size());
            Serialize(L, f.get(val));
            lua_rawset(L, -3);
        });
    } else if constexpr (std::is_same_v<T, bool>) {
        lua_pushboolean(L, val);
    } else if constexpr (std::is_floating_point_v<T>) {
        lua_pushnumber(L, val);
    } else if constexpr (std::is_constructible_v<string_view, T>) {
        auto str = string_view{val};
        lua_pushlstring(L, str.data(), str.size());
    } else if constexpr (std::is_integral_v<T>) {
        lua_pushinteger(L, val);
    } else if constexpr (meta::is_assoc_container_v<T>) {
        lua_createtable(L, val.size(), 0);
        size_t idx = 0;
        for (const auto& v: val) {
            Serialize(L, v);
            lua_rawseti(L, -1, idx++);
        }
    } else if constexpr (meta::is_index_container_v<T>) {
        lua_createtable(L, 0, val.size());
        for (const auto& [k, v]: val) {
            Serialize(L, k);
            Serialize(L, v);
            lua_rawset(L, -3);
        }
    } else {
        static_assert(std::is_void_v<T>, "Unsupported type");
    }
}

}
