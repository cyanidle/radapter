#include "redis_client.hpp"
#include "qtadapter.hpp"
#include <QTimer>
#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "async.h"
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif

using namespace radapter;
using namespace radapter::redis;

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
        throw std::runtime_error(reply->str);
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

struct Client::Impl {

    static void connectCallback(const redisAsyncContext *context, int status)
    {
        auto adapter = static_cast<Client*>(context->data);
        if (status != REDIS_OK) {
            emit adapter->Error(context->errstr);
        } else {
            redisAsyncCommand(adapter->ctx, Impl::dbCallback, nullptr, "SELECT %d", adapter->config.db.value);
        }
    }

    static void disconnectCallback(const redisAsyncContext *context, int)
    {
        auto adapter = static_cast<Client*>(context->data);
        emit adapter->ConnectedChanged(false);
    }

    static void privateCallback(redisAsyncContext* ctx, void *reply, void *data) noexcept try
    {
        auto cast = static_cast<redisReply*>(reply);
        auto cb = std::unique_ptr<Callback>(static_cast<Callback*>(data));
        try {
            if (!cast) throw Err("null reply");
            (*cb)(parseReply(cast), {});
        } catch (std::exception& e) {
            (*cb)({}, std::current_exception());
        }
    } catch (std::exception& e) {
        static_cast<Worker*>(static_cast<QObject*>(ctx->data)->parent())->Error("Exception in redis callback: {}", e.what());
    }

    static void dbCallback(redisAsyncContext* ctx, void *reply, void*)
    {
        auto cast = static_cast<redisReply*>(reply);
        auto adapter = static_cast<Client*>(ctx->data);
        try {
            if (!cast) throw Err("null reply");
            auto resp = parseReply(cast);
            if (resp != "OK") throw Err("Could not change db to {}", adapter->config.db.value);
            emit adapter->ConnectedChanged(true);
        } catch (std::exception& e) {
            emit adapter->Error(e.what());
            adapter->reconnectLater();
        }
    }

};

radapter::redis::Client::Client(Config _conf, Worker *parent) :
    QObject(parent),
    config(std::move(_conf))
{
    setObjectName(QString("Client(%1:%2)")
                      .arg(config.host.value.c_str())
                      .arg(config.port.value));
}

void radapter::redis::Client::Start() {
    connect(this, &redis::Client::Error, this, [this](auto err){
        static_cast<Worker*>(parent())->Error("{}: {}", objectName(), err);
    });
    connect(this, &redis::Client::ConnectedChanged, this, [this](bool ok){
        if (ok) {
            static_cast<Worker*>(parent())->Info("{}: connected", objectName());
        } else {
            static_cast<Worker*>(parent())->Warn("{}: disconnected", objectName());
            reconnectLater();
        }
    });
    doConnect();
}

static void escape(string& buff, char sym) {
    auto count = size_t(std::count(buff.begin(), buff.end(), sym));
    if (!count) return;
    buff.resize(buff.size() + count);
    auto from = buff.size() - 1 - count;
    auto to = buff.size() - 1;
    while (from) {
        auto ch = buff[from--];
        if (ch == sym) {
            buff[to--] = sym;
            buff[to--] = sym;
        } else {
            buff[to--] = ch;
        }
    }
}

void Client::Execute(string cmd, Callback cb)
{
    if (!ctx) {
        cb({}, std::make_exception_ptr(Err("Not connected")));
        return;
    }
    if (cmd.empty()) {
        cb({}, std::make_exception_ptr(Err("Empty command")));
        return;
    }
    escape(cmd, '%');
    auto c = new Callback{std::move(cb)};
    auto status = redisAsyncCommand(ctx, Impl::privateCallback, c, cmd.c_str());
    if (status != REDIS_OK) {
        (*c)({}, std::make_exception_ptr(Err("Could not run command")));
        delete c;
    }
}

void radapter::redis::Client::doConnect() {
    if (ctx) {
        redisAsyncDisconnect(ctx);
    }
    if (adapter) {
        delete adapter;
    }
    adapter = new QtRedisAdapter{this};
    auto options = redisOptions{};
    REDIS_OPTIONS_SET_TCP(&options, config.host.value.c_str(), config.port);
    ctx = redisAsyncConnectWithOptions(&options);
    ctx->data = this;
    adapter->SetContext(ctx);
    redisAsyncSetConnectCallback(ctx, Impl::connectCallback);
    redisAsyncSetDisconnectCallback(ctx, Impl::disconnectCallback);
    if (ctx->err) {
        emit Error(ctx->errstr);
        reconnectLater();
    }
}

void Client::reconnectLater()
{
    QTimer::singleShot(int(config.reconnect_timeout), this, &Client::doConnect);
}
