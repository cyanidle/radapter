#include "redis/client.hpp"
#include <QPointer>
#include "redis/adapter.hpp"

using namespace radapter::redis;

struct Client::Impl
{
    Settings settings{};
    redisAsyncContext* ctx{};
    QtRedisAdapter* adapter = {};
};

Client::~Client()
{
    Disconnect();
}

static void parseReply(lua_State* L, redisReply* reply)
{
    if (!reply) {
        return;
    }
    switch (reply->type) {
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_BIGNUM:
    case REDIS_REPLY_VERB:
        lua_pushlstring(L, reply->str, reply->len);
        break;
    case REDIS_REPLY_ATTR:
    case REDIS_REPLY_SET:
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_PUSH:
        lua_createtable(L, reply->elements, 0);
        for (size_t i = 0; i < reply->elements; ++i) {
            parseReply(L, reply->element[i]);
            lua_rawseti(L, -2, i);
        }
        break;
    case REDIS_REPLY_INTEGER:
        lua_pushinteger(L, reply->integer);
        break;
    case REDIS_REPLY_DOUBLE:
        lua_pushnumber(L, reply->dval);
        break;
    case REDIS_REPLY_BOOL:
        lua_pushboolean(L, bool(reply->integer));
        break;
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_NIL:
    default:
        lua_pushnil(L);
    }
}

void Client::Connect()
{
    Disconnect();
    d->adapter = new QtRedisAdapter{this};
    auto options = redisOptions{};
    REDIS_OPTIONS_SET_TCP(&options, d->settings.host.c_str(), d->settings.port);
    d->ctx = redisAsyncConnectWithOptions(&options);
    d->ctx->data = this;
    d->adapter->SetContext(d->ctx);
    timeval timeout;
    timeout.tv_sec = d->settings.timeout / 1000;
    timeout.tv_usec = (d->settings.timeout % 1000) * 1000;
    redisAsyncSetTimeout(d->ctx, timeout);
    redisAsyncSetConnectCallback(d->ctx, connectCallback);
    redisAsyncSetDisconnectCallback(d->ctx, disconnectCallback);
    if (d->ctx->err) {
        emit Error(d->ctx->errstr, d->ctx->err);
    }
}

void Client::Disconnect()
{
    if (d->ctx) {
        redisAsyncDisconnect(d->ctx);
    }
    if (d->adapter) {
        delete d->adapter;
        d->adapter = nullptr;
    }
}

void Client::connectCallback(const redisAsyncContext *ctx, int status)
{
    auto adapter = static_cast<Client*>(ctx->data);
    if (status != REDIS_OK) {
        emit adapter->Error(ctx->errstr, status);
    } else {
        emit adapter->Connected();
    }
}

void Client::disconnectCallback(const redisAsyncContext *context, int status)
{
    auto adapter = static_cast<Client*>(context->data);
    emit adapter->Disconnected(status);
}

void Client::privateCallback(redisAsyncContext* ctx, void *reply, void *data)
{
    auto cb = static_cast<lua::Ref*>(data);
    auto cast = static_cast<redisReply*>(reply);
    if (cb) {
        if (cast) {
            try {
                parseReply(cb->L, cast);
                auto pos = lua_absindex(cb->L, -1);
                cb->push();
                lua::PCall(cb->L, lua::StackRef{pos}, nullptr);
            } catch (std::exception& exc) {
                cb->push();
                lua::PCall(cb->L, nullptr, exc.what());
            }
        } else {
            cb->push();
            lua::PCall(cb->L, nullptr, ctx->errstr);
        }
        delete cb;
    }
}

static int defaultCb(lua_State* L) {
    auto cmd = lua_tostring(L, lua_upvalueindex(1));
    if (!lua_isnil(L, 2)) {
        logErr("Error during ({}) => {}", cmd, radapter::lua::ToString(L, 2));
    }
    return 0;
}

int Client::Execute(lua_State *L)
{
    auto cmd = luaL_checkstring(L, 1);
    if (lua_gettop(L) == 1) {
        lua_pushvalue(L, 1);
        lua_pushcclosure(L, defaultCb, 1);
    }
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!d->ctx) {
        lua_pushvalue(L, 2);
        lua::PCall(L, nullptr, "Connect not called");
        return 0;
    }
    auto cb = new lua::Ref{L, 2};
    auto status = redisAsyncCommand(d->ctx, privateCallback, cb, cmd);
    if (status != REDIS_OK) {
        cb->push();
        lua::PCall(L, nullptr, d->ctx->errstr);
        delete cb;
    }
    return 0;
}

Client::Client(Settings settings) : d(new Impl{std::move(settings)}) {
    connect(this, &Client::Error, &Client::Disconnect);
    connect(this, &Client::Connected, this, [this]{
        redisAsyncCommand(d->ctx, nullptr, nullptr, "SELECT %d", d->settings.db);
    });
}
