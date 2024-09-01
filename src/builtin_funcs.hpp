#pragma once
#include "pushpop.hpp"
#include "utils.hpp"
#include <fmt/args.h>
#include <QTimer>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>

static int format(lua_State* L);

[[maybe_unused]]
static void printStack(lua_State* L, string msg = "") {
    auto top = lua_gettop(L);
    for (auto i = 1; i <= top; ++i) {
        lua_pushcfunction(L, protect<format>);
        lua_pushvalue(L, i);
        lua_pcall(L, 1, 1, 0);
        msg += fmt::format("\n{}: {}", i, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    Instance::FromLua(L)->Debug("radapter", "Printing Stack: {}", msg);
}

[[maybe_unused]]
static int format(lua_State* L) {
    int idx = 2;
    auto top = lua_gettop(L);
    string_view fmt;
    if (top == 1) {
        fmt = "{}";
        idx--;
    } else {
        size_t len;
        auto s = lua_tolstring(L, 1, &len);
        fmt = {s, len};
    }
    fmt::dynamic_format_arg_store<fmt::format_context> args;
    args.reserve(size_t(top - idx + 1), 0);
    for (; idx <= top; ++idx) {
        auto t = lua_type(L, idx);
        switch (t) {
        case LUA_TBOOLEAN: {
            args.push_back(lua_toboolean(L, idx));
            break;
        }
        case LUA_TNUMBER: {
#ifdef RADAPTER_JIT
            args.push_back(lua_tonumber(L, idx));
#else
            if (lua_isinteger(L, idx)) {
                args.push_back(lua_tointeger(L, idx));
            } else {
                args.push_back(lua_tonumber(L, idx));
            }
#endif
            break;
        }
        case LUA_TSTRING: {
            size_t len;
            auto s = lua_tolstring(L, idx, &len);
            args.push_back(string_view{s, len});
            break;
        }
        case LUA_TTABLE: {
            lua_pushvalue(L, idx);
            auto var = toQVar(L);
            auto j = QJsonDocument::fromVariant(var);
            lua_pop(L, 1);
            args.push_back(j.toJson().toStdString());
            break;
        }
        case LUA_TFUNCTION: {
            args.push_back("<func>");
            break;
        }
        case LUA_TTHREAD: {
            args.push_back("<thread>");
            break;
        }
        case LUA_TUSERDATA: {
            args.push_back(fmt::format("<udata@0x{}>", lua_touserdata(L, -1)));
            break;
        }
        case LUA_TLIGHTUSERDATA: {
            args.push_back(fmt::format("<light_udata@0x{}>", lua_touserdata(L, -1)));
            break;
        }
        default:
            args.push_back("<nil>");
        }
    }
    auto str = fmt::vformat(fmt, args);
    lua_pushlstring(L, str.data(), str.size());
    return 1;
}

static void pushPart(lua_State* L, string_view part) {
    auto n = tryInt(part);
    if (n < 0) {
        lua_pushlstring(L, part.data(), part.size());
    } else {
        lua_pushinteger(L, n);
    }
}

// get(table, "deep:nested:key", sep = ':') -> nil/value
[[maybe_unused]]
static int get(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    string_view sep = ":";
    if (lua_gettop(L) > 2) {
        luaL_checktype(L, 3, LUA_TSTRING);
        sep = toSV(L, 3);
        if (sep.empty()) {
            luaL_error(L, "empty 'sep' parameter #3 passed to get()");
        }
    }

    lua_pushvalue(L, 1);
    auto k = toSV(L, 2);
    if (k.empty() || k == sep) {
        lua_pushvalue(L, 1);
        return 1;
    }
    size_t pos = 0;
    while(true) {
        auto ptr = k.find(sep, pos);
        if (ptr) {
            string_view part = k.substr(pos, ptr - pos);
            pushPart(L, part);
            lua_rawget(L, -2);
            if (lua_isnil(L, -1)) {
                return 1;
            }
            lua_remove(L, -2);
            if (ptr == string_view::npos) {
                return 1;
            }
        }
        pos = ptr + sep.size();
    }
}

// get(table, "deep:nested:key", val, sep = ':') -> table
[[maybe_unused]]
static int set(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checkany(L, 3);
    string_view sep = ":";
    if (lua_gettop(L) > 3) {
        luaL_checktype(L, 4, LUA_TSTRING);
        sep = toSV(L, 4);
        if (sep.empty()) {
            luaL_error(L, "empty 'sep' parameter #4 passed to set()");
        }
    }

    lua_pushvalue(L, 1);
    auto k = toSV(L, 2);
    if (k.empty() || k == sep) {
        lua_pushvalue(L, 1);
        return 1;
    }
    size_t pos = 0;
    while(true) {
        auto ptr = k.find(sep, pos);
        if (ptr) {
            string_view part = k.substr(pos, ptr - pos);
            pushPart(L, part);
            if (ptr == string_view::npos) {
                lua_pushvalue(L, 3);
                lua_rawset(L, -3);
                lua_pushvalue(L, 1);
                return 1;
            } else {
                lua_rawget(L, -2);
                auto t = lua_type(L, -1);
                if (t != LUA_TTABLE) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    pushPart(L, part);
                    lua_pushvalue(L, -2);
                    lua_rawset(L, -4);
                }
                lua_remove(L, -2);
            }
        }
        pos = ptr + sep.size();
    }
}


[[maybe_unused]]
static int wrap(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TSTRING);
    lua_pushvalue(L, 1);
    constexpr lua_CFunction doWrap = [](lua_State* L) noexcept {
        luaL_checkany(L, 1);

        lua_pushcfunction(L, set);
        lua_newtable(L);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);

        lua_call(L, 3, 1);
        return 1;
    };
    lua_pushcclosure(L, doWrap, 1);
    return 1;
}

[[maybe_unused]]
static int unwrap(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TSTRING);
    lua_pushvalue(L, 1);
    constexpr lua_CFunction doUnwrap = [](lua_State* L) noexcept {
        luaL_checkany(L, 1);

        lua_pushcfunction(L, get);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, lua_upvalueindex(1));

        lua_call(L, 2, 1);
        return 1;
    };
    lua_pushcclosure(L, doUnwrap, 1);
    return 1;
}

static int stopTimer(lua_State* L) noexcept {
    auto t = static_cast<QTimer*>(luaL_checkudata(L, 1, "_each_timer"));
    t->stop();
    luaL_unref(L, LUA_REGISTRYINDEX, t->objectName().toInt());
    t->setObjectName("-2");
    return 0;
}

[[maybe_unused]]
static int timer(lua_State* L, bool oneshot) {
    luaL_checktype(L, 1, LUA_TNUMBER);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto period = lua_tointeger(L, 1);
    if (period < 0) {
        luaL_error(L, "each(): Negative period passed: %I", period);
    }
    auto ud = lua_udata(L, sizeof(QTimer));
    auto inst = Instance::FromLua(L);
    auto t = new (ud) QTimer(inst);
    if (luaL_newmetatable(L, "_each_timer")) {
        luaL_Reg funcs[] = {
            {"Stop", stopTimer},
            {"__gc", __gc<QTimer>},
            {nullptr, nullptr},
        };
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        luaL_setfuncs(L, funcs, 0);
    }
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    auto ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto func = LuaFunction(L, 2);
    t->setObjectName(QString::number(ref));
    t->setSingleShot(oneshot);
    t->callOnTimeout([t, f = std::move(func)]() mutable {
        try {
            f({});
        } catch (std::exception& e) {
            static_cast<Instance*>(t->parent())->Error("timers", "Error calling timer: {}", e.what());
        }
        if (t->isSingleShot()) {
            luaL_unref(f._L, LUA_REGISTRYINDEX, t->objectName().toInt());
            t->setObjectName("-2");
            f = LuaFunction{};
        }
    });
    t->start(int(lua_tointeger(L, 1)));
    return 1;
}

[[maybe_unused]]
static int each(lua_State* L) {
    return timer(L, false);
}

[[maybe_unused]]
static int after(lua_State* L) {
    return timer(L, true);
}

[[maybe_unused]] // pipe(a, b, c) == a:pipe(b):pipe(c)
static int pipeAll(lua_State* L) noexcept {
    auto top = lua_gettop(L);
    if (top < 2) {
        luaL_error(L, "pipe(): expected at least 2 params");
    }
    lua_getfield(L, 1, "pipe");
    lua_pushvalue(L, 1);
    for (auto i = 2; i <= top; ++i) {
        lua_pushvalue(L, i);
        lua_call(L, 2, 1);
        if (i != top) {
            lua_getfield(L, -1, "pipe");
            lua_insert(L, -2); //swap top and pre-top
        }
    }
    return 1;
}
