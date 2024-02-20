#pragma once

#include "common.hpp"
#include "logs.hpp"

namespace radapter::lua
{

// capture value on top of stack into a reference
struct Value
{
    int ref = 0;
    int type = 0;
    lua_State* L;
    Value(lua_State* L) : L(L) {
        type = lua_type(L, -1);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    void Push() noexcept {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    }
    Value operator[](int idx) {
        lua_pushinteger(L, idx);
        Push();
        checkTable();
        lua_gettable(L, -1);
        return {L};
    }
    Value operator[](string_view str) {
        lua_pushlstring(L, str.data(), str.size());
        Push();
        checkTable();
        lua_gettable(L, -1);
        return {L};
    }
    ~Value() {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
private:
    void checkTable() {
        if (type != LUA_TTABLE)
            throw Err("value is not a table, was: {}", luaL_typename(L, -1));
    }
};


}
