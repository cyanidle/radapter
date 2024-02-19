#include "lua.hpp"
#include "logs.hpp"

using namespace radapter;
using namespace radapter::lua;

std::string_view lua::ToString(lua_State *L, int idx) noexcept {
    size_t len;
    auto ptr = lua_tolstring(L, idx, &len);
    return {ptr, len};
}

std::string_view lua::ToStringWithConv(lua_State *L, int idx) noexcept {
    size_t len;
    auto ptr = luaL_tolstring(L, idx, &len);
    return {ptr, len};
}

static int jsonmt = LUA_REFNIL;
static std::vector<bool> visited;

