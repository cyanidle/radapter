#pragma once
#include "common.hpp"

namespace radapter::lua
{

inline const char* printErr(int err) {
    switch (err) {
    case LUA_OK: return "ok";
    case LUA_YIELD:	return "yield";
    case LUA_ERRRUN: return "run";
    case LUA_ERRSYNTAX:	return "syntax error";
    case LUA_ERRMEM: return "memory";
    case LUA_ERRERR: return "err";
    default: return "<unknown>";
    }
}

template<auto func>
int Protected(lua_State* L) try {
    return func(L);
} catch (std::exception& exc) {
    luaL_error(L, "Exception: %s", exc.what());
    ::abort();
}

string_view ToString(lua_State* L, int idx);
string_view ToStringEx(lua_State* L, int idx);

Json FromTable(lua_State* L, int idx);
void RegisterJson(lua_State* L);
void RegisterJsonPtr(lua_State* L);


}
