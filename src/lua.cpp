#include "lua.hpp"

using namespace radapter;

std::string_view lua::ToString(lua_State *L, int idx) {
    size_t len;
    auto ptr = lua_tolstring(L, idx, &len);
    return {ptr, len};
}

std::string_view lua::ToStringEx(lua_State *L, int idx) {
    size_t len;
    auto ptr = luaL_tolstring(L, idx, &len);
    return {ptr, len};
}
