#pragma once
#include "radapter.hpp"
#include <QSet>
#include "builtin.hpp"

struct radapter::Instance::Impl {
    lua_State* L;
    QSet<Worker*> workers;
    LogLevel globalLevel = LogLevel::debug;
    std::map<string, LogLevel, std::less<>> perCat;
    std::map<string, ExtraSchema> schemas;
    bool shutdown = false;
    bool shutdownDone = false;
    int insideLogHandler = false;
    int luaHandler = LUA_NOREF;

    static int luaLog(lua_State* L);
    static int log_level(lua_State* L);
    static int log__call(lua_State* L); // convert __call(t, ...) -> luaLog(...)
    static int log_handler(lua_State* L);

    Impl() {
        L = luaL_newstate();
    }

    ~Impl() {
        luaL_unref(L, LUA_REGISTRYINDEX, luaHandler);
        lua_close(L);
    }

};

