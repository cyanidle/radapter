#pragma once

#include "common.hpp"
#include "describe/describe.hpp"
#include <QObject>
#include <QVariant>
#include <fmt/core.h>
#include "auto_reg.hpp"

class QtRedisAdapter;
struct redisAsyncContext;

namespace radapter::redis
{

struct Settings : ClassSettings {
    string host = "127.0.0.1";
    uint16_t port = 6379;
    uint16_t db = 0;
};

DESCRIBE(Settings, &_::host, &_::port, &_::db)

class Client : public QObject
{
    Q_OBJECT
public:
    using ResultCallback = std::function<void(QVariant res, string err)>;
    void Execute(std::string cmd, ResultCallback cb);
    Client(const Settings& settings);
    ~Client();
signals:
    void Connected();
    void Error(int code);
    void Disconnected(int code);
public slots:
    void Connect();
private:
    static void connectCallback(const redisAsyncContext *context, int status);
    static void disconnectCallback(const redisAsyncContext *context, int status);
    static void privateCallback(redisAsyncContext *ctx, void *reply, void *data);

    struct Impl;
    unique_ptr<Impl> impl;
    QtRedisAdapter* adapter;
};

DESCRIBE(Client, &_::Connected, &_::Error, &_::Disconnected, &_::Connect)
DESCRIBE_ATTRS(Client, describe::FieldsDoNotInherit, Settings)
DESCRIBE_FIELD_ATTRS(Client, Connected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Error, Signal);
DESCRIBE_FIELD_ATTRS(Client, Disconnected, Signal);

}
