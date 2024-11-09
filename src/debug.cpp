#include "radapter.hpp"
#include "builtin.hpp"
#include "builtin_funcs.hpp"
#include "glua/glua.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <qfileinfo.h>
#include <qthread.h>
#include <QAbstractEventDispatcher>

extern "C" {
int luaopen_socket_core(lua_State *L);
}


void radapter::Instance::DebuggerConnect(DebuggerOpts opts)
{
    auto L = LuaState();
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    lua_gc(L, LUA_GCSTOP, 0);
    defer restart([L]{
        lua_gc(L, LUA_GCRESTART, 0);
    });
    radapter::compat::prequiref(L, "socket.core", luaopen_socket_core, 0);
    lua_pop(L, 1);
    LoadEmbeddedFile("socket");
    if (opts.vscode) {
        Warn("debugger", "Using vscode-compatible mobdebug");
        LoadEmbeddedFile("dkjson");
        LoadEmbeddedFile("mobdebug.vscode", LoadEmbedNoPop);
    } else {
        LoadEmbeddedFile("mobdebug", LoadEmbedNoPop);
    }
    Info("debugger", "Connecting to debug server on {}:{}", opts.host, opts.port);
    lua_getfield(L, -1, "coro");
    lua_pcall(L, 0, 0, 0);
    lua_getfield(L, -1, "start");
    lua_pushlstring(L, opts.host.data(), opts.host.size());
    lua_pushinteger(L, opts.port);
    auto status = lua_pcall(L, 2, 1, msgh);
    if (status != LUA_OK) {
        throw Err("debugger: Could not start: {}", toSV(L));
    }
    if (!lua_toboolean(L, -1)) {
        throw Err("debugger: Not available");
    }
    lua_pop(L, 1);
}
