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

static string load_builtin(QString name) {
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        throw Err("Could not open builtin file: {}", name);
    }
    string s(size_t(f.size()), ' ');
    f.read(s.data(), f.size());
    return s;
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
    luaL_requiref(L, "socket.core", luaopen_socket_core, 0);
    lua_pop(L, 1);
    load_mod(L, "socket", load_builtin(":/scripts/socket.lua"));
    return 1;
}

static int load_mobdebug(lua_State* L) {
    load_mod(L, "mobdebug", load_builtin(":/scripts/mobdebug.lua"));
    return 1;
}

static int load_dkjson(lua_State* L) {
    load_mod(L, "dkjson", load_builtin(":/scripts/dkjson.lua"));
    return 1;
}

static int load_vscode_mobdebug(lua_State* L) {
    load_mod(L, "mobdebug", load_builtin(":/scripts/mobdebug.vscode.lua"));
    return 1;
}

void radapter::Instance::DebuggerConnect(DebuggerOpts opts)
{
    auto L = LuaState();
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    stop_gc gc{L};
    luaL_requiref(L, "socket", glua::protect<load_socket>, 0);
    lua_pop(L, 1);
    if (opts.vscode) {
        Warn("debugger", "Using vscode-compatible mobdebug");
        luaL_requiref(L, "dkjson", glua::protect<load_dkjson>, 0);
        luaL_requiref(L, "mobdebug", glua::protect<load_vscode_mobdebug>, 0);
    } else {
        luaL_requiref(L, "mobdebug", glua::protect<load_mobdebug>, 0);
    }
    Info("debugger", "Connecting to debug server on {}:{}", opts.host, opts.port);
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
