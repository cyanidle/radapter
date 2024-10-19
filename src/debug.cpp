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
}

static string_view mobdebug() {
    return {reinterpret_cast<const char*>(mobdebug_lua), mobdebug_lua_len};
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

// Qt-based 'luasocket' placeholder (required by mobdebug)
// this is stripped-down absolutely minimal API

namespace radapter {

struct SocketClient {
    radapter::Instance* inst;
    std::unique_ptr<QTcpSocket> sock;

    void send(string_view msg);
    string receive(lua_State* L);
    void close();
};

struct SocketServer {
    radapter::Instance* inst;
    std::unique_ptr<QTcpServer> server;

    SocketServer(lua_State* L, string host, uint16_t port);
    SocketClient accept();
};

DESCRIBE(radapter::SocketClient, &_::send, &_::receive, &_::close)
DESCRIBE(radapter::SocketServer, &_::accept)

SocketServer::SocketServer(lua_State * L, string _host, uint16_t port)
{
    inst = Instance::FromLua(L);
    server.reset(new QTcpServer(inst));
    QHostAddress host = _host == "*" ? QHostAddress::Any : QHostAddress(QString::fromStdString(_host));
    if (!server->listen(host, port)) {
        throw Err("Could not listen: {}", server->errorString().toStdString());
    }
}

SocketClient SocketServer::accept()
{
    bool shutdown = false;
    std::unique_ptr<QTcpSocket> sock = nullptr;
    auto conn = QObject::connect(server.get(), &QTcpServer::newConnection, [&]{
        sock.reset(server->nextPendingConnection());
    });
    auto shut = QObject::connect(inst, &Instance::ShutdownRequest, [&]{
        shutdown = true;
    });
    while (!sock && !shutdown) {
        server->thread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    }
    QObject::disconnect(conn);
    QObject::disconnect(shut);
    if (shutdown) {
        throw Err("Shutdown");
    }
    sock->setParent(inst);
    return {inst, std::move(sock)};
}

void SocketClient::send(string_view msg)
{
    sock->write(msg.data(), int64_t(msg.size()));
}

string SocketClient::receive(lua_State * L)
{
    if (lua_isinteger(L, 2)) {
        auto l = lua_tointeger(L, 2);
        if (l < 0) {
            throw Err("size < 0");
        }
        string buff;
        buff.resize(size_t(l));
        sock->read(buff.data(), l);
        return buff;
    } else {
        size_t l;
        auto p = luaL_checklstring(L, 2, &l);
        string_view pat = {p, l};
        if (pat == "*l") {
            while (!sock->canReadLine()) {
                sock->thread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
            }
            auto l = sock->readLine();
            return {l.data(), size_t(l.size())};
        } else {
            throw Err("Invalid pattern: {}", pat);
        }
    }
}

void SocketClient::close()
{
    sock->close();
}

static int load_rad_socket(lua_State* L) {
    lua_createtable(L, 0, 1); // socket
    glua::PushCtor<SocketServer(lua_State*, string, uint16_t)>(L);
    lua_setfield(L, -2, "bind");
    return 1;
}

static int load_mobdebug(lua_State* L) {
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    auto status = luaL_loadbufferx(L, mobdebug().data(), mobdebug().size(), "mobdebug", "t");
    if (status != LUA_OK) {
        throw Err("Could not compile mobdebug: {}", toSV(L));
    }
    status = lua_pcall(L, 0, 1, msgh);
    if (status != LUA_OK) {
        throw Err("Could not load mobdebug: {}", toSV(L));
    }
    return 1;
}

}

void radapter::Instance::DebuggerListen(string host, uint16_t port)
{
    auto L = LuaState();
    lua_pushcfunction(L, traceback);
    auto msgh = lua_gettop(L);
    stop_gc gc{L};
    luaL_requiref(L, "socket", glua::protect<load_rad_socket>, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "mobdebug", glua::protect<load_mobdebug>, 1);
    Info("debugger", "Listening on {}:{}", host, port);
    lua_getfield(L, -1, "listen");
    lua_pushlstring(L, host.data(), host.size());
    lua_pushinteger(L, port);
    auto status = lua_pcall(L, 2, 0, msgh);
    if (status != LUA_OK) {
        throw Err("debugger: Could not listen: {}", toSV(L));
    }
    lua_pop(L, 1);
}
