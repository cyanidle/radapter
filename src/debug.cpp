#include "radapter.hpp"
#include "builtin.hpp"
#include "builtin_funcs.hpp"
#include "glua/glua.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <qthread.h>
#include <QAbstractEventDispatcher>

extern "C" {
#include "mobdebug.h"
#include "luasocket.h"
int luaopen_socket_core(lua_State *L);
}

static string_view mobdebug() {
    return {reinterpret_cast<const char*>(mobdebug_lua), mobdebug_lua_len};
}
static string_view luasocket() {
    return {reinterpret_cast<const char*>(socket_lua), socket_lua_len};
}

namespace {
struct stop_gc {
    lua_State* L;
    stop_gc(lua_State* L): L(L) {
        lua_gc(L, LUA_GCSTOP, 0);
    }
    ~stop_gc() {
        lua_gc(L, LUA_GCRESTART, 0);
    }
};
}


static void load_mod(lua_State* L, const char* name, string_view src) {
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto status = luaL_loadbufferx(L, src.data(), src.size(), name, "t");
    if (status != LUA_OK) {
        throw Err("Could not compile {}: {}", name, toSV(L));
    }
    status = lua_pcall(L, 0, 1, msgh);
    if (status != LUA_OK) {
        throw Err("Could not load {}: {}", name, toSV(L));
    }
}

static int load_socket(lua_State* L) {
    load_mod(L, "socket", luasocket());
    return 1;
}

static int load_mobdebug(lua_State* L) {
    load_mod(L, "mobdebug", mobdebug());
    return 1;
}

void radapter::Instance::DebuggerConnect(string host, uint16_t port)
{
    auto L = LuaState();
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    stop_gc gc{L};
    luaL_requiref(L, "socket.core", luaopen_socket_core, 0);
    lua_pop(L, 1);
    luaL_requiref(L, "socket", glua::protect<load_socket>, 0);
    lua_pop(L, 1);
    luaL_requiref(L, "mobdebug", glua::protect<load_mobdebug>, 0);
    Info("debugger", "Connecting to debug server on {}:{}", host, port);
    lua_getfield(L, -1, "start");
    lua_pushlstring(L, host.data(), host.size());
    lua_pushinteger(L, port);
    auto status = lua_pcall(L, 2, 1, msgh);
    if (status != LUA_OK) {
        throw Err("debugger: Could not start: {}", toSV(L));
    }
    if (!lua_toboolean(L, -1)) {
        throw Err("debugger: Not available");
    }
    lua_pop(L, 1);
}
