#pragma once

#include "common.hpp"
#include "describe/describe.hpp"
#include <QObject>
#include <QVariant>
#include <fmt/core.h>
#include "auto_reg.hpp"

struct redisAsyncContext;

namespace radapter::redis
{

struct Settings : ClassSettings {
    string host = "127.0.0.1";
    uint16_t port = 6379;
    uint16_t db = 0;
};

DESCRIBE(redis::Settings, &_::host, &_::port, &_::db)
DESCRIBE_FIELD_ATTRS(Settings, host, MissingOk);
DESCRIBE_FIELD_ATTRS(Settings, port, MissingOk);
DESCRIBE_FIELD_ATTRS(Settings, db, MissingOk);

class Client : public QObject
{
    Q_OBJECT
public:
    using ResultCallback = std::function<void(QVariant res, string err)>;
    void Execute(std::string cmd, ResultCallback cb);
    Client(Settings settings);
    ~Client();
signals:
    void Connected();
    void Error(string msg, int code);
    void Disconnected(int code);
public slots:
    void Connect();
private:
    static void connectCallback(const redisAsyncContext *context, int status);
    static void disconnectCallback(const redisAsyncContext *context, int status);
    static void privateCallback(redisAsyncContext *ctx, void *reply, void *data);

    struct Impl;
    unique_ptr<Impl> d;
};

DESCRIBE(redis::Client, &_::Connected, &_::Error, &_::Disconnected, &_::Connect)
DESCRIBE_ATTRS(Client, describe::FieldsDoNotInherit, Settings)
DESCRIBE_FIELD_ATTRS(Client, Connected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Error, Signal);
DESCRIBE_FIELD_ATTRS(Client, Disconnected, Signal);

}
