#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <qfileinfo.h>
#include <qthread.h>
#include <QAbstractEventDispatcher>
#include "instance_impl.hpp"

void radapter::Instance::DebuggerConnect(DebuggerOpts opts)
{
    auto L = LuaState();
    lua_pushcfunction(L, builtin::traceback);
    auto msgh = lua_gettop(L);
    lua_gc(L, LUA_GCSTOP, 0);
    defer restart([L]{
        lua_gc(L, LUA_GCRESTART, 0);
    });
    if (opts.vscode) {
        Warn("debugger", "Using vscode-compatible mobdebug");
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
        Raise("debugger: Could not start: {}", builtin::help::toSV(L));
    }
    if (!lua_toboolean(L, -1)) {
        Raise("debugger: Not available");
    }
    lua_pop(L, 1);
    d->debuggerActive = opts.vscode ? 2 : 1;
    d->newThreadCreated(L);
}


void radapter::Instance::Impl::newThreadCreated(lua_State* T)
{
    if (!debuggerActive)
        return;
    self->LoadEmbeddedFile(debuggerActive == 1 ? "mobdebug" : "mobdebug.vscode", LoadEmbedNoPop);
    lua_xmove(L, T, 1);
    lua_getfield(T, -1, "on");
    lua_pcall(T, 0, 0, 0);
    lua_pop(T, 1);
}