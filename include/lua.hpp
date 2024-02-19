#pragma once
#include "common.hpp"

namespace radapter::lua
{

inline const char* printErr(int err) {
    switch (err) {
    case LUA_OK: return "ok";
    case LUA_YIELD:	return "yield";
    case LUA_ERRRUN: return "run error";
    case LUA_ERRSYNTAX:	return "syntax error";
    case LUA_ERRMEM: return "memory";
    case LUA_ERRERR: return "err";
    default: return "<unknown>";
    }
}

template<typename...Ts>
[[noreturn]] void Error(lua_State* L, const char* fmt, const Ts&...a) {
    luaL_error(L, fmt, a...);
    ::abort();
}

template<auto func>
int Protected(lua_State* L) try {
    return func(L);
} catch (std::exception& exc) {
    luaL_error(L, "Exception: %s", exc.what());
    ::abort();
}

string_view ToString(lua_State* L, int idx) noexcept;
string_view ToStringWithConv(lua_State* L, int idx) noexcept;

template<typename T>
int cleanup(lua_State* L) noexcept {
    static_cast<T*>(lua_touserdata(L, 1))->~T();
}
template<typename T>
inline constexpr luaL_Reg gcFor = {"__gc", cleanup<T>};

}
