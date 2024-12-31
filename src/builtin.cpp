#include "radapter/radapter.hpp"
#include "utils.hpp"
#include <fmt/args.h>
#include <QTimer>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include "QPointer"
#include "builtin.hpp"
#include "glua/glua.hpp"

using namespace radapter;
using namespace glua;

void builtin::help::PrintStack(lua_State* L, string msg) {
    auto top = lua_gettop(L);
    for (auto i = 1; i <= top; ++i) {
        lua_pushcfunction(L, protect<api::Format>);
        lua_pushvalue(L, i);
        lua_pcall(L, 1, 1, 0);
        msg += fmt::format("\n{}: {}", i, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    Instance::FromLua(L)->Debug("radapter", "Printing Stack: {}", msg);
}

int builtin::api::Format(lua_State* L) {
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
            auto var = builtin::help::toQVar(L);
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
int builtin::api::Get(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    string_view sep = ":";
    if (lua_gettop(L) > 2) {
        luaL_checktype(L, 3, LUA_TSTRING);
        sep = builtin::help::toSV(L, 3);
        if (sep.empty()) {
            luaL_error(L, "empty 'sep' parameter #3 passed to get()");
        }
    }

    lua_pushvalue(L, 1);
    auto k = builtin::help::toSV(L, 2);
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
int builtin::api::Set(lua_State* L) noexcept {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checkany(L, 3);
    string_view sep = ":";
    if (lua_gettop(L) > 3) {
        luaL_checktype(L, 4, LUA_TSTRING);
        sep = help::toSV(L, 4);
        if (sep.empty()) {
            luaL_error(L, "empty 'sep' parameter #4 passed to set()");
        }
    }

    lua_pushvalue(L, 1);
    auto k = help::toSV(L, 2);
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
            {"__gc", dtor_for<QTimer>},
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
            t->setObjectName(QString::number(LUA_NOREF));
            f = LuaFunction{};
        }
    });
    t->start(int(lua_tointeger(L, 1)));
    return 1;
}

int builtin::api::Each(lua_State* L) {
    return timer(L, false);
}

int builtin::api::After(lua_State* L) {
    return timer(L, true);
}

struct TempFileObject {
    std::shared_ptr<QTemporaryFile> _file;
    std::string _url;

    TempFileObject() {
        _file = std::make_shared<QTemporaryFile>();
        if (!_file->open()) {
            throw Err("Could not open temp file: {}", _file->errorString());
        }
        _url = "file:///" + _file->fileName().toStdString();
    }

    std::string_view url() {
        return _url;
    }
};

DESCRIBE("TempFileObject", TempFileObject) {
    MEMBER("url", &_::url);
}

int radapter::builtin::api::TempFile(lua_State* L)
{   
    size_t sz;
    auto str = luaL_checklstring(L, 1, &sz);
    TempFileObject temp;
    temp._file->write(str, sz);
    glua::Push(L, temp);
    return 1;
}

string_view builtin::help::toSV(lua_State* L, int idx) noexcept {
    size_t len;
    auto s = lua_tolstring(L, idx, &len);
    return {s, len};
}

int builtin::help::traceback(lua_State* L) noexcept {
    auto msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg, 1);
    return 1;
}

template<typename Fn>
inline void iterateTable(lua_State* L, Fn&& f) {
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

QVariantList builtin::help::toArgs(lua_State* L, int start = 1) {
    QVariantList args;
    auto top = lua_gettop(L);
    for (auto i = start; i <= top; ++i) {
        lua_pushvalue(L, i);
        args.push_back(toQVar(L));
        lua_pop(L, 1);
    }
    return args;
}

static void pushQStr(lua_State* L, QString const& str) {
    auto std = str.toStdString();
    lua_pushlstring(L, std.data(), std.size());
}

using QObjPtr = QPointer<QObject>;
DESCRIBE("_radapter::QObjPtr", QObjPtr, void) {}


struct ExtraHelper {
    ExtraFunction func;
};
DESCRIBE("radapter::ExtraHelper", ExtraHelper, void) {}

static int wrapFunc(lua_State* L) {
    auto top = lua_gettop(L);
    auto args = QVariantList();
    args.reserve(top);
    for (auto i = 1; i <= top; ++i) {
        lua_pushvalue(L, i);
        args.push_back(builtin::help::toQVar(L));
        lua_pop(L, 1);
    }
    glua::Push(L, glua::CheckUData<ExtraHelper>(L, lua_upvalueindex(1)).func(Instance::FromLua(L), std::move(args)));
    return 1;
}

void glua::Push(lua_State* L, QVariant const& val) {
    auto t = val.type();
    switch (int(t)) {
    case QVariant::Type::Map: {
        lua_checkstack(L, 3); //key, val, table
        auto map = val.toMap();
        lua_createtable(L, 0, map.size());
        for (auto it = map.keyValueBegin(); it != map.keyValueEnd(); ++it) {
            pushQStr(L, it->first);
            Push(L, it->second);
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
            Push(L, v);
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
        } else if (auto cf = val.value<ExtraFunction>()) {
            glua::Push(L, ExtraHelper{std::move(cf)});
            lua_pushcclosure(L, glua::protect<wrapFunc>, 1);
        } else if (auto q = val.value<QObject*>()) {
            glua::Push(L, QPointer<QObject>{q});
        } else {
            lua_pushnil(L);
        }
        break;
    }
}

inline unsigned isArray(lua_State* L) noexcept {
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

QString builtin::help::toQStr(lua_State* L, int idx) {
    size_t len;
    auto s = lua_tolstring(L, idx, &len);
    return QString::fromUtf8(s, int(len));
}

QVariant builtin::help::toQVar(lua_State* L, int idx) {
    switch (lua_type(L, idx)) {
    case LUA_TTABLE: {
        if (!lua_checkstack(L, 3)) return {}; // nil + key + val
        if (luaL_getmetafield(L, idx, "__call")) {
            lua_pop(L, 1);
            return QVariant::fromValue(LuaFunction(L, idx));
        }
        if (auto len = isArray(L)) {
            QVariantList arr;
            arr.reserve(int(len));
            for (int i = 1; unsigned(i) <= len; ++i) {
                lua_rawgeti(L, idx, i);
                arr.push_back(toQVar(L));
                lua_pop(L, 1);
            }
            return arr;
        } else {
            QVariantMap map;
            lua_pushvalue(L, idx);
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
            lua_pop(L, 1);
            return map;
        }
    }
    case LUA_TSTRING: {
        return toQStr(L, idx);
    }
    case LUA_TBOOLEAN: {
        return bool(lua_toboolean(L, idx));
    }
    case LUA_TNUMBER: {
#ifdef RADAPTER_JIT
        return lua_tonumber(L, idx);
#else
        if (lua_isinteger(L, idx)) {
            return lua_tointeger(L, idx);
        } else {
            return lua_tonumber(L, idx);
        }
#endif
    }
    case LUA_TFUNCTION: {
        return QVariant::fromValue(LuaFunction(L, idx));
    }
    case LUA_TUSERDATA: {
        if (auto q = glua::TestUData<QPointer<QObject>>(L, idx)) {
            return QVariant::fromValue(q->data());
        } else if (luaL_getmetafield(L, idx, "__call")) {
            lua_pop(L, 1);
            return QVariant::fromValue(LuaFunction(L, idx));
        } else {
            return {};
        }
    }
    default:
        return {};
    }
}
