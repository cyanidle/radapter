#pragma once
#include "describe/describe.hpp"
#include "logs.hpp"
#include "lua.hpp"

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
                throw Err("{}.{}", exc.what());
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
    } else if (false) {
        // containers
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
