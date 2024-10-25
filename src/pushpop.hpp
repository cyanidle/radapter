#pragma once
#include <QVariant>
#include <QList>
#include <QPointer>
#include "radapter.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

using namespace radapter;

[[maybe_unused]]
static string_view toSV(lua_State* L, int idx = -1) noexcept {
    size_t len;
    auto s = lua_tolstring(L, idx, &len);
    return {s, len};
}

[[maybe_unused]]
inline int traceback(lua_State* L) noexcept {
    auto msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg, 1);
    return 1;
}

template<typename Fn>
static void iterateTable(lua_State* L, Fn&& f) {
    if (lua_type(L, -1) != LUA_TTABLE) {
        throw Err("iterateTable(): Table expected");
    }
    lua_pushnil(L);
    while(lua_next(L, -2)) {
        auto was = lua_gettop(L);
        bool contin = true;
        if constexpr (std::is_invocable_r_v<void, Fn>) {
            f();
        } else {
            contin = f();
        }
        auto now = lua_gettop(L);
        if (was != now) {
            throw Err("After calling an iterator function =>"
                      " stack is not of the same size (was: {} != now: {})",
                      was, now);
        }
        if (!contin) {
            lua_pop(L, 2);
            return;
        }
        lua_pop(L, 1);
    }
}

template<auto f>
static int protect(lua_State* L) noexcept {
    try {
        return f(L);
    } catch (std::exception& e) {
        lua_pushstring(L, e.what());
    }
    lua_error(L);
    std::abort();
}

template<typename T>
static int __gc(lua_State* L) {
    static_cast<T*>(lua_touserdata(L, -1))->~T();
    return 0;
}

static QVariant toQVar(lua_State* L);

[[maybe_unused]]
static QVariantList toArgs(lua_State* L, int start = 1) {
    QVariantList args;
    auto top = lua_gettop(L);
    for (auto i = start; i <= top; ++i) {
        lua_pushvalue(L, i);
        args.push_back(toQVar(L));
        lua_pop(L, 1);
    }
    return args;
}

#ifdef RADAPTER_JIT
#define lua_udata(L, ...) lua_newuserdata(L, (__VA_ARGS__))
#else
#define lua_udata(L, ...) lua_newuserdatauv(L, (__VA_ARGS__), 0)
#endif

template<typename T, typename...Args>
inline T* pushGced(lua_State* L, Args&&...a) {

    auto mem = lua_udata(L, sizeof(T));
    auto res = new (mem) T{std::forward<Args>(a)...};
    lua_createtable(L, 0, 1);
    luaL_Reg metameta[]{
        {"__gc", __gc<T>},
        {nullptr, nullptr}
    };
    luaL_setfuncs(L, metameta, 0);
    lua_setmetatable(L, -2);
    return res;
}

static void pushQStr(lua_State* L, QString const& str) {
    auto std = str.toStdString();
    lua_pushlstring(L, std.data(), std.size());
}

using qptr = QPointer<QObject>;
constexpr auto qtptr_name = "QObject*";

[[maybe_unused]]
inline void push(lua_State* L, QVariant const& val) {
    auto t = val.type();
    switch (int(t)) {
    case QVariant::Type::Map: {
        lua_checkstack(L, 3); //key, val, table
        auto map = val.toMap();
        lua_createtable(L, 0, map.size());
        for (auto it = map.keyValueBegin(); it != map.keyValueEnd(); ++it) {
            pushQStr(L, it->first);
            push(L, it->second);
            lua_settable(L, -3);
        }
        break;
    }
    case QVariant::Type::List: {
        lua_checkstack(L, 2); //val, table
        auto arr = val.toList();
        lua_createtable(L, arr.size(), 0);
        lua_Integer idx = 1;
        for (auto& v: arr) {
            push(L, v);
            lua_rawseti(L, -2, idx++);
        }
        break;
    }
    case QVariant::Type::Bool: {
        lua_pushboolean(L, val.toBool());
        break;
    }
    case QVariant::Type::Char: {
        pushQStr(L, QString(val.toChar()));
        break;
    }
    case QVariant::Type::Int: {
        lua_pushinteger(L, val.toInt());
        break;
    }
    case QVariant::Type::UInt: {
        lua_pushinteger(L, val.toUInt());
        break;
    }
    case QVariant::Type::LongLong: {
        lua_pushinteger(L, val.toLongLong());
        break;
    }
    case QVariant::Type::ULongLong: {
        auto v = val.toULongLong();
        if (v > (std::numeric_limits<lua_Integer>::max)()) {
            lua_pushnumber(L, double(v));
        } else {
            lua_pushinteger(L, lua_Integer(v));
        }
        break;
    }
    case QVariant::Type(QMetaType::Float): {
        lua_pushnumber(L, double(val.toFloat()));
        break;
    }
    case QVariant::Type::Double: {
        lua_pushnumber(L, val.toDouble());
        break;
    }
    case QVariant::Type::String: {
        pushQStr(L, val.toString());
        break;
    }
    case QVariant::Type::StringList: {
        lua_checkstack(L, 1); //val
        auto arr = val.toStringList();
        lua_createtable(L, arr.size(), 0);
        lua_Integer i = 1;
        for (auto& v: arr) {
            pushQStr(L, v);
            lua_rawseti(L, -2, i++);
        }
        break;
    }
    default:
        if (auto f = val.value<LuaFunction>()) {
            lua_rawgeti(f._L, LUA_REGISTRYINDEX, f._ref);
        } else if (auto q = val.value<QObject*>()) {
            auto ud = lua_udata(L, sizeof(qptr));
            new (ud) qptr{q};
            if (luaL_newmetatable(L, qtptr_name)) {
                luaL_Reg funcs[] = {
                    {"__gc", __gc<qptr>},
                    {nullptr, nullptr},
                };
                luaL_setfuncs(L, funcs, 0);
            }
            lua_setmetatable(L, -2);
        } else {
            lua_pushnil(L);
        }
        break;
    }
}

static unsigned isArray(lua_State* L) noexcept {
    lua_Integer hits = 0;
    lua_Integer max = 0;
    lua_pushnil(L);
    while(lua_next(L, -2)) {
        ++hits;
        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2); //pop key, value
            return 0;
        }
        auto k = lua_tointeger(L, -2);
        if (k < 1) {
            lua_pop(L, 2); //pop key, value
            return 0;
        }
        if (k > max) {
            max = k;
        }
        lua_pop(L, 1); //pop value
    }
    if (hits < max) {
        return 0;
    }
    return unsigned(hits);
}

static QString toQStr(lua_State* L, int idx = -1) {
    size_t len;
    auto s = lua_tolstring(L, idx, &len);
    return QString::fromUtf8(s, int(len));
}

static QVariant toQVar(lua_State* L) {
    switch (lua_type(L, -1)) {
    case LUA_TTABLE: {
        if (!lua_checkstack(L, 3)) return {}; // nil + key + val
        if (luaL_getmetafield(L, -1, "__call")) {
            lua_pop(L, 1);
            return QVariant::fromValue(LuaFunction(L, -1));
        }
        if (auto len = isArray(L)) {
            QVariantList arr;
            arr.reserve(int(len));
            for (int i = 1; unsigned(i) <= len; ++i) {
                lua_rawgeti(L, -1, i);
                arr.push_back(toQVar(L));
                lua_pop(L, 1);
            }
            return arr;
        } else {
            QVariantMap map;
            iterateTable(L, [&]{
                auto key = -2;
                QString k;
                switch (lua_type(L, key)) {
                case LUA_TSTRING: {
                    k = toQStr(L, key);
                    break;
                }
                case LUA_TNUMBER: {
#ifdef RADAPTER_JIT
                    k = QString::number(lua_tonumber(L, key));
#else
                    if (lua_isinteger(L, key)) {
                        k = QString::number(lua_tointeger(L, key));
                    } else {
                        k = QString::number(lua_tonumber(L, key));
                    }
#endif
                    break;
                }
                default: return;
                }
                auto v = toQVar(L);
                map.insert(k, std::move(v));
            });
            return map;
        }
    }
    case LUA_TSTRING: {
        return toQStr(L);
    }
    case LUA_TBOOLEAN: {
        return bool(lua_toboolean(L, -1));
    }
    case LUA_TNUMBER: {
#ifdef RADAPTER_JIT
        return lua_tonumber(L, -1);
#else
        if (lua_isinteger(L, -1)) {
            return lua_tointeger(L, -1);
        } else {
            return lua_tonumber(L, -1);
        }
#endif
    }
    case LUA_TFUNCTION: {
        return QVariant::fromValue(LuaFunction(L, -1));
    }
    case LUA_TUSERDATA: {
        if (auto q = luaL_testudata(L, -1, qtptr_name)) {
            return QVariant::fromValue(static_cast<qptr*>(q)->data());
        } else if (luaL_getmetafield(L, -1, "__call")) {
            lua_pop(L, 1);
            return QVariant::fromValue(LuaFunction(L, -1));
        } else {
            return {};
        }
    }
    default:
        return {};
    }
}
