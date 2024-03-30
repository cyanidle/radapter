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

struct Settings : ClassSettings, describe::Attrs<MissingAllowed> {
    string Host = "127.0.0.1";
    uint16_t Port = 6379;
    uint16_t Db = 0;
    uint32_t Timeout = 5000;
};

class Client : public QObject
{
    Q_OBJECT
public:

    int Execute(lua_State* L);
    Client(Settings settings);
    ~Client();
signals:
    void Connected();
    void Error(string msg, int code);
    void Disconnected(int code);
public slots:
    void Connect();
    void Disconnect();
private:
    static void connectCallback(const redisAsyncContext *context, int status);
    static void disconnectCallback(const redisAsyncContext *context, int status);
    static void privateCallback(redisAsyncContext *ctx, void *reply, void *data);

    struct Impl;
    unique_ptr<Impl> d;
};

DESCRIBE(redis::Client,
    &_::Connected, &_::Error, &_::Disconnected,
    &_::Disconnect, &_::Connect, &_::Execute)
DESCRIBE(redis::Settings, &_::Host, &_::Port, &_::Db, &_::Timeout)
DESCRIBE_ATTRS(Client, Settings)
DESCRIBE_FIELD_ATTRS(Client, Connected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Error, Signal);
DESCRIBE_FIELD_ATTRS(Client, Disconnected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Execute, NativeMethod);

}
