#pragma once

#include "common.hpp"
#include "describe.hpp"
#include <QObject>
#include <QVariant>
#include <fmt/core.h>

class QtRedisAdapter;
struct redisAsyncContext;

namespace radapter::redis
{

struct Settings {
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

}
