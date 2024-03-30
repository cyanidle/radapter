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

class Client : public QObject
{
    Q_OBJECT
public:
    struct Settings : ClassSettings, describe::Attrs<MissingAllowed> {
        string host = "127.0.0.1";
        uint16_t port = 6379;
        uint16_t db = 0;
        uint32_t timeout = 5000;
    };

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
         &_::Connected,
         &_::Error,
         &_::Disconnected,
         &_::Disconnect,
         &_::Connect,
         &_::Execute)
DESCRIBE(Client::Settings, &_::host, &_::port, &_::db, &_::timeout)
DESCRIBE_ATTRS(Client, describe::FieldsDoNotInherit, Client::Settings)
DESCRIBE_FIELD_ATTRS(Client, Connected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Error, Signal);
DESCRIBE_FIELD_ATTRS(Client, Disconnected, Signal);
DESCRIBE_FIELD_ATTRS(Client, Execute, NativeMethod);

}
