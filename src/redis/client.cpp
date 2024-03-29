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
    if (d->ctx) {
        redisAsyncDisconnect(d->ctx);
    }
}

static QVariant parseReply(redisReply* reply)
{
    if (!reply) {
        return {};
    }
    auto array = QVariantList{};
    switch (reply->type) {
    case REDIS_REPLY_STRING:
        return QString(reply->str);
    case REDIS_REPLY_ARRAY:
        for (size_t i = 0; i < reply->elements; ++i) {
            array.append(parseReply(reply->element[i]));
        }
        return array;
    case REDIS_REPLY_INTEGER:
        return reply->integer;
    case REDIS_REPLY_NIL:
        return {};
    case REDIS_REPLY_STATUS:
        return QString(reply->str);
    case REDIS_REPLY_ERROR:
        return {};
    case REDIS_REPLY_DOUBLE:
        return reply->dval;
    case REDIS_REPLY_BOOL:
        return bool(reply->integer);
    case REDIS_REPLY_MAP:
        throw std::runtime_error("REDIS_REPLY_MAP Unsupported");
    case REDIS_REPLY_SET:
        for (size_t i = 0; i < reply->elements; ++i) {
            array.append(parseReply(reply->element[i]));
        }
        return array;
    case REDIS_REPLY_ATTR:
        throw std::runtime_error("REDIS_REPLY_ATTR Unsupported");
    case REDIS_REPLY_PUSH:
        throw std::runtime_error("REDIS_REPLY_PUSH Unsupported");
    case REDIS_REPLY_BIGNUM:
        return QString(reply->str);
    case REDIS_REPLY_VERB:
        return QString(reply->str);
    default:
        return {};
    }
}

void Client::Connect()
{
    if (d->ctx) {
        redisAsyncDisconnect(d->ctx);
    }
    if (d->adapter) {
        delete d->adapter;
    }
    d->adapter = new QtRedisAdapter{this};
    auto options = redisOptions{};
    REDIS_OPTIONS_SET_TCP(&options, d->settings.host.c_str(), d->settings.port);
    d->ctx = redisAsyncConnectWithOptions(&options);
    d->ctx->data = this;
    d->adapter->SetContext(d->ctx);
    timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    redisAsyncSetTimeout(d->ctx, timeout);
    redisAsyncSetConnectCallback(d->ctx, connectCallback);
    redisAsyncSetDisconnectCallback(d->ctx, disconnectCallback);
    if (d->ctx->err) {
        emit Error(d->ctx->errstr, d->ctx->err);
    }
}

void Client::connectCallback(const redisAsyncContext *context, int status)
{
    auto adapter = static_cast<Client*>(context->data);
    if (status != REDIS_OK) {
        emit adapter->Error("Connect Error", status);
    } else {
        emit adapter->Connected();
    }
}

void Client::disconnectCallback(const redisAsyncContext *context, int status)
{
    auto adapter = static_cast<Client*>(context->data);
    emit adapter->Disconnected(status);
}

void Client::privateCallback(redisAsyncContext*, void *reply, void *data)
{
    auto cb = static_cast<ResultCallback*>(data);
    auto cast = static_cast<redisReply*>(reply);
    if (cb && *cb) {
        if (cast) {
            try {
                (*cb)(parseReply(cast), {});
            } catch (std::exception& exc) {
                (*cb)({}, exc.what());
            }
        } else {
            (*cb)({}, "Error in callback");
        }
    }
    delete cb;
}

void Client::Execute(string cmd, ResultCallback _cb)
{
    if (!d->ctx) {
        _cb({}, "Connect not called");
    } else {
        auto cb = new ResultCallback{std::move(_cb)};
        auto status = redisAsyncCommand(d->ctx, privateCallback, cb, cmd.c_str());
        if (status != REDIS_OK) {
            (*cb)({}, "Send Error");
            delete cb;
        }
    }

}

Client::Client(Settings settings) : d(new Impl{std::move(settings)}) {}
