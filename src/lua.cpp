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

Json lua::IntoJson(lua_State *L, int idx)
{

}

static int newJson(lua_State* L) {

}

static int getItem(lua_State* L) {

}

static int getSz(lua_State* L) {

}

static luaL_Reg jsonLib[] = {
    {"new", newJson},
    {"Size", getSz},
    {"__len", getSz},
    {"Get", getItem},
    {"__index", getItem},
    {NULL, NULL}
};

void lua::RegisterJson(lua_State *L)
{
    luaL_newmetatable(L, "Json");

    lua_pushliteral(L, "json");
    luaL_newlib(L, jsonLib);
    lua_setglobal(L, "json");
}

void lua::RegisterJsonPtr(lua_State *L)
{

}
