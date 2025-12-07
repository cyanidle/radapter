#include "redis_client.hpp"
#include "fmt/compile.h"
#include "qtadapter.hpp"
#include <QTimer>
#include "redis_inc.h"

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
            adapter->ReconnectLater();
        } else {
            redisAsyncCommand(adapter->ctx, Impl::dbCallback, nullptr, "SELECT %d", adapter->config.db.value);
        }
    }

    static void disconnectCallback(const redisAsyncContext *context, int)
    {
        auto adapter = static_cast<Client*>(context->data);
        adapter->ok = false;
        emit adapter->ConnectedChanged(false);
    }

    static void privateCallback(redisAsyncContext* ctx, void *reply, void *_data) noexcept try
    {
        auto cast = static_cast<redisReply*>(reply);
        auto* data = static_cast<Data<QVariant>*>(_data);
        auto promise = Promise<QVariant>(data);
        // hack, todo: improve Future<> API
        data->promises--;
        Unref(data);
        try {
            if (!cast) throw Err("null reply");
            promise(parseReply(cast));
        } catch (...) {
            promise(std::current_exception());
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
            adapter->ok = true;
            emit adapter->ConnectedChanged(true);
        } catch (std::exception& e) {
            emit adapter->Error(e.what());
            adapter->ReconnectLater();
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

Client::~Client()
{
    if (ctx) {
        redisAsyncDisconnect(ctx);
        ctx = nullptr;
    }
}

void radapter::redis::Client::Start() {
    connect(this, &redis::Client::Error, this, [this](auto err){
        static_cast<Worker*>(parent())->Error("{}: {}", objectName(), err);
    });
    connect(this, &redis::Client::ConnectedChanged, this, [this](bool _ok){
        if (_ok) {
            static_cast<Worker*>(parent())->Info("{}: connected", objectName());
        } else {
            static_cast<Worker*>(parent())->Warn("{}: disconnected", objectName());
            ReconnectLater();
        }
    });
    doConnect();
}

bool Client::IsConnected() const
{
    return ok;
}

static string redisFormat(const string_view *argv, size_t argc) {
    string res = fmt::format(FMT_COMPILE("*{}\r\n"), argc);
    for (size_t i = 0; i < argc; ++i) {
        res += fmt::format(FMT_COMPILE("${}\r\n{}\r\n"), argv[i].size(), argv[i]);
    }
    return res;
}

Future<QVariant> Client::Execute(const string_view *argv, size_t argc)
{
    if (!ctx) {
        return fut::Rejected<QVariant>(Err("Not connected"));
    }
    if (!argc) {
        return fut::Rejected<QVariant>(Err("Empty command"));
    }
    string prep;
    try {
        prep = redisFormat(argv, argc);
    } catch (...) {
        return fut::Rejected<QVariant>(std::current_exception());
    }
    auto promise = Promise<QVariant>{};
    auto fut = promise.GetFuture();
    auto status = redisAsyncFormattedCommand(ctx, Impl::privateCallback, fut.PeekState(), prep.c_str(), prep.size());
    if (status != REDIS_OK) {
        promise(Err("Could not run command: {} => ", argv[0], ctx->errstr));
        ReconnectLater();
    } else {
        fut.PeekState()->promises++; // hack, todo: improve Future<> API
        AddRef(fut.PeekState());
    }
    return fut;
}

static void subCallback(redisAsyncContext* ctx, void *reply, void *_data) noexcept
{
    auto adapter = static_cast<Client*>(ctx->data);
    auto cast = static_cast<redisReply*>(reply);
    auto* data = static_cast<Client::Subscriber*>(_data);
    try {
        if (!cast) throw Err("null reply");
        auto r = parseReply(cast).toStringList();
        if (r.size() < 3) {
            throw Err("Invalid responce: small list?");
        }
        if (r[0] == "pmessage") {
            (*data)(Client::SubEvent{std::move(r[2]), std::move(r[3])});
        }
    } catch (std::exception& e) {
        emit adapter->Error(QString("PSUB: ") + e.what());
        adapter->ReconnectLater();
    }
}

void Client::PSubscribe(string_view glob, Subscriber _sub)
{
    if (!ctx) {
        throw Err("Not connected");
    }
    auto* sub = new Subscriber(std::move(_sub));
    auto status = redisAsyncCommand(ctx, subCallback, sub, "PSUBSCRIBE %s", string{glob}.c_str());
    if (status != REDIS_OK) {
        throw Err("Could not psubscribe to: {}", glob);
    }
}

void radapter::redis::Client::doConnect() {
    if (ctx) {
        redisAsyncFree(ctx);
        ctx = nullptr;
    }
    if (adapter) {
        delete adapter;
        adapter = nullptr;
    }
    reconPending = false;
    adapter = new QtRedisAdapter{this};
    auto options = redisOptions{};
    options.options |= REDIS_OPT_NOAUTOFREE;
    REDIS_OPTIONS_SET_TCP(&options, config.host.value.c_str(), config.port);
    ctx = redisAsyncConnectWithOptions(&options);
    ctx->data = this;
    adapter->SetContext(ctx);
    redisAsyncSetConnectCallback(ctx, Impl::connectCallback);
    redisAsyncSetDisconnectCallback(ctx, Impl::disconnectCallback);
    if (ctx->err) {
        emit Error(ctx->errstr);
        ReconnectLater();
    }
}

void Client::ReconnectLater()
{
    if (reconPending) return;
    reconPending = true;
    QTimer::singleShot(int(config.reconnect_timeout), this, &Client::doConnect);
}
