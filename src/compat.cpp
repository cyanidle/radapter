#include "builtin.hpp"
#include "radapter/radapter.hpp"
#include "glua/glua.hpp"

using namespace radapter;

int radapter::compat::lua_absindex (lua_State *L, int i) {
    if (i < 0 && i > LUA_REGISTRYINDEX)
        i += lua_gettop(L) + 1;
    return i;
}

int radapter::compat::luaL_getsubtable (lua_State *L, int i, const char *name) {
    int abs_i = compat::lua_absindex(L, i);
    luaL_checkstack(L, 3, "not enough stack slots");
    lua_pushstring(L, name);
    lua_gettable(L, abs_i);
    if (lua_istable(L, -1))
        return 1;
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushstring(L, name);
    lua_pushvalue(L, -2);
    lua_settable(L, abs_i);
    return 0;
}

void radapter::compat::luaL_requiref (lua_State *L, const char *modname, lua_CFunction openf, int glb) {
    luaL_checkstack(L, 3, "not enough stack slots available");
    compat::luaL_getsubtable(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, modname);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_pushcfunction(L, openf);
        lua_pushstring(L, modname);
        lua_call(L, 1, 1);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, modname);
    }
    if (glb) {
        lua_pushvalue(L, -1);
        lua_setglobal(L, modname);
    }
    lua_replace(L, -2);
}

static int do_prequiref(lua_State* L) {
    compat::luaL_requiref(L, lua_tostring(L, 1), lua_tocfunction(L, 2), int(lua_tointeger(L, 3)));
    return 1;
}

void compat::prequiref(lua_State *L, const char *modname, lua_CFunction openf, int glb)
{
    if (!lua_checkstack(L, 5)) throw Err("prequire: no stack left");
    lua_pushcfunction(L, builtin::help::traceback);
    auto trace = lua_gettop(L);
    lua_pushcfunction(L, do_prequiref);
    lua_pushstring(L, modname);
    lua_pushcfunction(L, openf);
    lua_pushinteger(L, glb);
    auto res = lua_pcall(L, 3, 1, trace);
    lua_insert(L, -2); //remove traceback
    lua_pop(L, 1);
    if (res != LUA_OK) throw Err("prequire: {}", lua_tostring(L, -1));
}
