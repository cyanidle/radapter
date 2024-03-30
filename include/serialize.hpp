#pragma once
#include "describe/describe.hpp"
#include "meta/meta.hpp"
#include "logs.hpp"
#include "lua.hpp"

namespace radapter {

struct MissingAllowed {};

struct TraceFrame {
    constexpr TraceFrame() noexcept = default;
    constexpr TraceFrame(string_view key, const TraceFrame& parent) :
        prev(&parent), size(key.size()), str(key.data())
    {}
    constexpr TraceFrame(size_t idx, const TraceFrame& parent) :
        prev(&parent), idx(idx)
    {}
    template<typename F>
    void Walk(F&& f) const {
        auto nx = prepWalk();
        while(nx) {
            if (nx->size) f(string_view{nx->str, nx->size});
            else f(nx->idx);
            nx = nx->next;
        }
    }
    string PrintTrace() const;
private:
    const TraceFrame* prepWalk() const;

    const TraceFrame* prev = {};
    mutable const TraceFrame* next = {};
    size_t size = {};
    union {
        const char* str;
        size_t idx = {};
    };
};

inline void CheckType(lua_State* L, int t, const TraceFrame& frame) {
    if (auto was = lua_type(L, -1); was != t) {
        throw Err("{}: expected '{}', got '{}'",
                  frame.PrintTrace(), lua_typename(L, t), lua_typename(L, was));
    }
}

template<typename T>
void Deserialize(lua_State* L, T& out, int idx = -1, TraceFrame const& frame = {})
{
    lua_pushvalue(L, idx);
    if constexpr (describe::is_described_v<T>) {
        constexpr auto desc = describe::Get<T>();
        auto t = lua_type(L, -1);
        if (t != LUA_TNIL && t != LUA_TTABLE) {
            throw Err("{}: Table expected, got: '{}'",
                      frame.PrintTrace(), lua_typename(L, t));
        }
        desc.for_each_field([&](auto f){
            if (t == LUA_TNIL) {
                lua_pushnil(L);
            } else {
                lua_pushlstring(L, f.name.data(), f.name.size());
                lua_rawget(L, -2);
            }
            constexpr bool canSkip = describe::has_attr_v<MissingAllowed, decltype(f)>;
            if (canSkip && lua_isnil(L, -1)) {
                lua_pop(L, 1);
                return;
            }
            TraceFrame next{f.name, frame};
            Deserialize(L, f.get(out), -1, next);
            lua_pop(L, 1);
        });
    } else if constexpr (std::is_same_v<T, bool>) {
        CheckType(L, LUA_TBOOLEAN, frame);
        out = lua_toboolean(L, -1);
    } else if constexpr (std::is_floating_point_v<T>) {
        CheckType(L, LUA_TNUMBER, frame);
        out = lua_tonumber(L, -1);
    } else if constexpr (std::is_constructible_v<string_view, T>) {
        CheckType(L, LUA_TSTRING, frame);
        out = lua::ToString(L, -1);
    } else if constexpr (std::is_integral_v<T>) {
        if (!lua_isinteger(L, -1)) {
            throw Err("{}: number should be an integer",
                      frame.PrintTrace());
        }
        out = lua_tointeger(L, -1);
    } else if constexpr (meta::is_assoc_container_v<T>) {
        using kT = typename T::key_type;
        static_assert(std::is_constructible_v<string_view, T>, "non-string map keys unsupported");
        lua::IterateTable(L, -1, [&]{
            CheckType(L, LUA_TSTRING, frame);
            auto key = lua::ToString(L, -2);
            TraceFrame next{key, frame};
            Deserialize(L, out[key], -1, next);
        });
    } else if constexpr (meta::is_index_container_v<T>) {
        CheckType(L, LUA_TTABLE, frame);
        if (auto len = lua::IsArray(L, -1)) {
            out.resize(len);
            for (auto i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                TraceFrame next{size_t(i), frame};
                Deserialize(L, out[i], -1, next);
                lua_pop(L, 1);
            }
        } else {
            throw Err("{}: value is not an array",
                      frame.PrintTrace());
        }
    } else {
        static_assert(std::is_void_v<T>, "Unsupported type");
    }
    lua_pop(L, 1);
}

template<typename T>
T Deserialize(lua_State* L, int idx = -1, TraceFrame const& frame = {}) {
    T out;
    Deserialize(L, out, idx, frame);
    return out;
}

inline void Serialize(lua_State* L, std::nullptr_t) {
    lua_pushnil(L);
}

inline void Serialize(lua_State* L, lua::StackRef s) {
    lua_pushvalue(L, s.ref);
}

inline void Serialize(lua_State* L, lua::Ref s) {
    s.push();
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

namespace lua {

template<typename...Args>
int PCall(lua_State* L, const Args&...args) {
    constexpr int count = sizeof...(Args);
    PushTracer(L);
    lua_pushvalue(L, -2);
    (Serialize(L, args), ...);
    return lua_pcall(L, count, LUA_MULTRET, -count-2);
}

}
}
