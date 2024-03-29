#pragma once
#include "common.hpp"
#include "fmt/base.h"
#include "fmt/format.h"

namespace radapter::lua
{

template<typename Fn>
void IterateTable(lua_State* L, int idx, Fn&& f) {
    lua_pushvalue(L, idx);
    lua_pushnil(L);
    while(lua_next(L, -2)) {
        f();
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void checkType(lua_State* L, int t, int idx = -1);

struct Ref {
    lua_State* L = {};
    int ref = LUA_NOREF;
    Ref() noexcept = default;
    Ref(lua_State* L, int idx) noexcept : L(L) {
        lua_pushvalue(L, idx);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    Ref(const Ref& o) noexcept : L(o.L) {
        o.push();
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    Ref& operator=(const Ref& o) {
        if (this != &o) {
            this->~Ref();
            L = o.L;
            o.push();
            ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        return *this;
    }
    void push() const noexcept {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    }
    ~Ref() {
        if (ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }
    }
};

inline lua::Ref TracerFunc;

inline const char* printErr(int err) {
    switch (err) {
    case LUA_OK: return "ok";
    case LUA_YIELD:	return "yield";
    case LUA_ERRRUN: return "run error";
    case LUA_ERRSYNTAX:	return "syntax error";
    case LUA_ERRMEM: return "memory error";
    case LUA_ERRERR: return "unknown error";
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

lua_Integer IsArray(lua_State* L, int idx);

string_view ToString(lua_State* L, int idx) noexcept;
string_view ToStringWithConv(lua_State* L, int idx) noexcept;

template<typename T>
int cleanup(lua_State* L) noexcept {
    static_cast<T*>(lua_touserdata(L, 1))->~T();
    return 0;
}

template<typename T>
inline constexpr luaL_Reg gcFor = {"__gc", cleanup<T>};

void ParseJson(lua_State* L, string_view json);
void DumpJson(lua_State* L, int idx, bool pretty = false);
int DumpStack(lua_State* L) noexcept;

struct LogString {
    lua_State* L = nullptr;
    int idx = -1;
};
struct LogTable {
    lua_State* L = nullptr;
    int idx = -1;
};

}

template<> struct fmt::formatter<radapter::lua::LogString> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const radapter::lua::LogString& s, FormatContext& ctx) const {
        return fmt::formatter<std::string_view>::format(radapter::lua::ToString(s.L, s.idx), ctx);
    }
};

template<> struct fmt::formatter<radapter::lua::LogTable> {
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        auto fmt_end = std::find(ctx.begin(), ctx.end(), '}');
        if (fmt_end != ctx.begin()) {
            char representation = *ctx.begin();
            if (representation == 'c')
                pretty = false;
            else if (representation == 'p')
                pretty = true;
            else
                throw fmt::format_error("invalid");
        }
        return std::next(ctx.begin());
    }
    template<typename FormatContext>
    auto format(const radapter::lua::LogTable& s, FormatContext& ctx) const {
        radapter::lua::DumpJson(s.L, s.idx, pretty);
        auto str = radapter::lua::ToString(s.L, -1);
        return fmt::format_to(ctx.out(), "{}", str);
    }
    bool pretty = false;
};
