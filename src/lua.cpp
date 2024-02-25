#include "lua.hpp"
#include "logs.hpp"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

using namespace radapter::lua;
using namespace rapidjson;
using namespace radapter;

using Encoding = UTF8<>;
namespace {

struct LuaHandler {
    typedef typename Encoding::Ch Ch;
    lua_State *L;
    int reserved = 10;
    enum Mode {
        Val, Arr, Obj
    } mode = Val;
    lua_Integer pushedCount = 0;
    bool pushed() {
        switch (mode) {
        case Arr:
            lua_rawseti(L, -2, ++pushedCount);
            break;
        case Obj:
            if (++pushedCount == 2) {
                lua_rawset(L, -3);
                pushedCount = 0;
            }
            break;
        default: break;
        }
        return true;
    }
    bool saveState() {
        if (reserved <= 3) {
            if (_Unlikely(!lua_checkstack(L, 12))) {
                return false;
            }
            reserved = 12;
        }
        reserved -= 3;
        lua_pushinteger(L, pushedCount);
        lua_pushinteger(L, mode);
        return true;
    }
    void popState() {
        lua_rotate(L, -3, 1);
        mode = Mode(lua_tointeger(L, -1));
        pushedCount = lua_tointeger(L, -2);
        lua_pop(L, 2);
        reserved += 3;
    }
    bool Null() {
        lua_pushnil(L);
        return pushed();
    }
    bool Bool(bool v) {
        lua_pushboolean(L, v);
        return pushed();
    }
    bool Int(int i) {
        lua_pushinteger(L, i);
        return pushed();
    }
    bool RawNumber(const Ch* s, SizeType sz, bool) {
        return false;
    }
    bool Uint(unsigned u) {
        lua_pushinteger(L, u);
        return pushed();
    }
    bool Int64(int64_t i) {
        lua_pushinteger(L, i);
        return pushed();
    }
    bool Uint64(uint64_t u) {
        if (u > std::numeric_limits<lua_Integer>::max()) {
            lua_pushnumber(L, double(u));
        } else {
            lua_pushinteger(L, int64_t(u));
        }
        return pushed();
    }
    bool Double(double n) {
        lua_pushnumber(L, n);
        return pushed();
    }
    bool String(const Ch* s, SizeType sz, bool) {
        lua_pushlstring(L, s, sz);
        return pushed();
    }
    bool Key(const Ch* str, SizeType len, bool copy) {
        return String(str, len, copy);
    }
    bool StartObject() {
        if (_Unlikely(!saveState())) {
            return false;
        }
        pushedCount = 0;
        mode = Obj;
        lua_createtable(L, 0, 0);
        return true;
    }
    bool EndObject(SizeType) {
        popState();
        return pushed();
    }
    bool StartArray() {
        if (_Unlikely(!saveState())) {
            return false;
        }
        pushedCount = 0;
        mode = Arr;
        lua_createtable(L, 0, 0);
        return true;
    }
    bool EndArray(SizeType n) {
        popState();
        return pushed();
    }
};

struct StringViewStream : StringStream
{
    StringViewStream(string_view sv) :
        StringStream(sv.data()),
        end(src_ + sv.size())
    {}
    Ch Peek() { return _Unlikely(src_ == end) ? 0 : *src_; }
    Ch Take() { return _Unlikely(src_ == end) ? 0 : *src_++; }
    const Ch* end;
};

template<typename Handler>
static void visit(lua_State* L, Handler& h) {
    auto t = lua_type(L, -1);
    switch (t) {
    case LUA_TBOOLEAN: {
        h.Bool(bool(lua_toboolean(L, -1)));
        return;
    }
    case LUA_TNUMBER: {
        if (lua_isnumber(L, -1)) {
            h.Int64(lua_tointeger(L, -1));
            return;
        } else {
            h.Double(lua_tonumber(L, -1));
            return;
        }
    }
    case LUA_TSTRING: {
        auto str = ToString(L, -1);
        if (str.size() == 0) {
            h.String("", 0, true);
        } else {
            h.String(str.data(), str.size(), true);
        }
        return;
    }
    case LUA_TTABLE: {
        if (auto len = IsArray(L, -1)) {
            h.StartArray();
            for (lua_Integer i = 1; i <= len; ++i) {
                lua_rawgeti(L, -1, i);
                visit(L, h);
                lua_pop(L, 1);
            }
            h.EndArray(len);
        } else {
            h.StartObject();
            lua_Integer obj = 0;
            lua_pushnil(L);
            while(lua_next(L, -2) != 0) {
                obj++;
                if (lua_type(L, -2) != LUA_TSTRING) {
                    throw Err("{} => key was not a string: rather: {}",
                              lua::ToStringWithConv(L, -2), luaL_typename(L, -2));
                }
                auto key = ToString(L, -2);
                h.Key(key.data(), key.size(), true);
                try {
                    visit(L, h);
                } catch (std::exception& exc) {
                    throw Err("{}.{}", key, exc.what());
                }
                lua_pop(L, 1);
            }
            h.EndObject(obj);
        }
        break;
    }
    default: {
        h.Null();
    }
    }
}

} //<anon>

void lua::ParseJson(lua_State *L, string_view json)
{
    StringViewStream str{json};
    alignas(void*) char buff[4096];
    MemoryPoolAllocator<> alloc{buff, sizeof(buff)};
    GenericReader<UTF8<>, UTF8<>, MemoryPoolAllocator<>> reader{&alloc};
    LuaHandler handler{L};
    auto st = reader.Parse<kParseIterativeFlag>(str, handler);
    if (!st) {
        throw Err("Parse Error at {} => {}", st.Offset(), GetParseError_En(st.Code()));
    }
}

void lua::DumpJson(lua_State *L, int idx, bool pretty)
{
    lua_pushvalue(L, idx);
    StringBuffer out;
    alignas(void*) char stackBuff[4096];
    MemoryPoolAllocator<> alloc(stackBuff, sizeof(stackBuff));
    if (pretty) {
        PrettyWriter<StringBuffer, UTF8<>, UTF8<>, MemoryPoolAllocator<>> writer(out, &alloc);
        visit(L, writer);
    } else {
        Writer<StringBuffer, UTF8<>, UTF8<>, MemoryPoolAllocator<>> writer(out, &alloc);
        visit(L, writer);
    }
    lua_pop(L, 1);
    lua_pushlstring(L, out.GetString(), out.GetLength());
}

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


lua_Integer lua::IsArray(lua_State *L, int idx)
{
    if (lua_type(L, idx) != LUA_TTABLE) {
        throw Err("table expected");
    }
    lua_pushvalue(L, idx);
    lua_Integer hits = 0;
    lua_Integer max = 0;
    lua_pushnil(L);
    while(lua_next(L, -2)) {
        ++hits;
        if (!lua_isinteger(L, -2)) {
            lua_pop(L, 3);
            return 0;
        }
        auto k = lua_tointeger(L, -2);
        if (k < 1) {
            lua_pop(L, 3);
            return 0;
        }
        if (k > max) {
            max = k;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (hits < max) {
        return 0;
    }
    return hits;
}

int lua::DumpStack(lua_State *L) noexcept
{
    std::string stack;
    auto top = lua_gettop(L);
    for(int i = top; i >= 1; i--) {
        string_view str;
        switch (lua_type(L, i)) {
        case LUA_TSTRING:
        case LUA_TBOOLEAN:
        case LUA_TNUMBER:
            stack += fmt::format("\t{:>3}({:>3}): {}\n", i, i - top - 1, ToStringWithConv(L, i));
            lua_pop(L, 1);
            break;
        default:
            stack += fmt::format("\t{:>3}({:>3}): <{}>\n", i, i - top - 1, luaL_typename(L, i));
            break;
        }
    }
    logDebug("# stack dump: \n{}", stack);
    return 0;
}

void lua::checkType(lua_State *L, int t, int idx) {
    if (auto was = lua_type(L, idx); was != t) {
        throw Err("Invalid type at {}: expected: {} => got: {}", idx, lua_typename(L, t), lua_typename(L, was));
    }
}
