#include "value.hpp"

#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/reader.h"
#include "rapidjson/error/en.h"

using namespace radapter::lua;
using namespace rapidjson;

using Encoding = UTF8<>;

struct LuaHandler {
    typedef typename Encoding::Ch Ch;
    lua_State *L;
    int reserved = 10;
    int times = 1;
    enum Mode {
        Val, Arr, Obj
    } mode = Val;
    int arrCount = 1;

    bool pushed() {
        switch (mode) {
        case Arr: {
            lua_rawseti(L, -2, arrCount++);
            break;
        }
        case Obj: {
            lua_rawset(L, -3);
            break;
        }
        default: break;
        }
        return true;
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
        if (!--reserved) {
            if (!lua_checkstack(L, 10)) {
                return false;
            }
            reserved = 10;
        }
        lua_pushinteger(L, mode);
        lua_createtable(L, 0, 0);
        return true;
    }
    bool EndObject(SizeType) {
        reserved++;
        mode = Mode(lua_tointeger(L, -1));
        lua_rotate(L, -2, 1);
        lua_pop(L, 1);
        return true;
    }
    bool StartArray() {
        if (!--reserved) {
            if (!lua_checkstack(L, 10)) {
                return false;
            }
            reserved = 10;
        }
        arrCount = 1;
        lua_pushinteger(L, mode);
        lua_createtable(L, 0, 0);
        return true;
    }
    bool EndArray(SizeType n) {
        reserved++;
        mode = Mode(lua_tointeger(L, -2));
        lua_rotate(L, -2, 1);
        lua_pop(L, 1);
        return pushed();
    }
};


bool Value::ParseJson(lua_State *L, const string& json)
{
    StringStream str{json.c_str()};
    alignas(void*) char buff[4096];
    MemoryPoolAllocator<> alloc{buff, sizeof(buff)};
    GenericReader<UTF8<>, UTF8<>, MemoryPoolAllocator<>> reader{&alloc};
    LuaHandler handler{L};
    auto was = lua_gettop(L);
    auto st = reader.Parse<kParseIterativeFlag>(str, handler);
    if (was) {
        lua_settop(L, was - 1);
    }
    if (!st) {
        lua_settop(L, was);
        lua_pushfstring(L, "Parse Error at %I => %s", st.Offset(), GetParseError_En(st.Code()));
        return false;
    } else {
        return true;
    }
}

std::string Value::DumpJson(bool pretty)
{

}
