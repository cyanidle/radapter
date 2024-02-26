#pragma once
#include "describe/describe.hpp"
#include "logs.hpp"
#include "lua.hpp"
#include "utilcpp/meta.hpp"

namespace radapter {

template<typename T, typename = void>
void deserialize(lua_State* L, T& out)
{
    if constexpr (describe::has_v<T>) {
        constexpr auto desc = describe::Get<T>();
        lua::checkType(L, LUA_TTABLE, -1);
        desc.for_each_field([&](auto f){
            lua_pushlstring(L, f.name.data(), f.name.size());
            lua_gettable(L, -1);
            try {
                deserialize(L, out.*f.value);
            } catch (std::exception& exc) {
                throw Err("{}.{}", f.name, exc.what());
            }
            lua_pop(L, 1);
        });
    } else if constexpr (std::is_same_v<T, bool>) {
        lua::checkType(L, LUA_TBOOLEAN, -1);
        out = lua_toboolean(L, -1);
    }  else if constexpr (std::is_floating_point_v<T>) {
        lua::checkType(L, LUA_TNUMBER, -1);
        out = lua_tonumber(L, -1);
    } else if constexpr (std::is_same_v<T, std::string>) {
        lua::checkType(L, LUA_TSTRING, -1);
        out = lua::ToString(L, -1);
    } else if constexpr (std::is_integral_v<T>) {
        lua::checkType(L, LUA_TNUMBER, -1);
        if (!lua_isinteger(L, -1)) {
            throw Err("number should be an integer");
        }
        out = lua_tointeger(L, -1);
    } else if constexpr (util::is_assoc_container_v<T>) {
        using kT = typename T::key_type;
        static_assert(util::is_string_like_v<kT>, "non-string map keys unsupported");
        using vT = typename T::mapped_type;
        lua::IterateTable(L, -1, [&]{
            lua::checkType(L, LUA_TSTRING, -2);
            auto key = lua::ToString(L, -2);
            try {
                deserialize(L, out[key]);
            } catch (std::exception& exc) {
                throw Err("{}.{}", key, exc.what());
            }
        });
    } else if constexpr (util::is_container_v<T>) {
        using vT = typename T::value_type;
        lua::checkType(L, LUA_TTABLE, -1);
        if (auto len = lua::IsArray(L, -1)) {
            out.resize(len);
            for (auto i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                deserialize(L, out[i]);
                lua_pop(L, 1);
            }
        } else {
            throw Err("value is not an array");
        }
    } else {
        static_assert(std::is_void_v<T>, "Unsupported type");
    }
}

template<typename T, typename Conf>
int configurable(lua_State* L) {
    Conf conf;
    deserialize(L, conf);
}

}
