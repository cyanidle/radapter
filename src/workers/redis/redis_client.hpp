#pragma once

#include "radapter.hpp"

class QtRedisAdapter;
struct redisAsyncContext;

namespace radapter::redis {

struct Config {
    WithDefault<string> host = "localhost";
    WithDefault<uint16_t> port = uint16_t(6379);
    WithDefault<uint16_t> db = uint16_t(0);
    WithDefault<unsigned> reconnect_timeout = 1000u;
};
DESCRIBE(redis::Config, &_::host, &_::port, &_::db, &_::reconnect_timeout)

class Client : public QObject {
    Q_OBJECT

    Config config;
    QtRedisAdapter* adapter{};
    redisAsyncContext* ctx{};
public:
    using Callback = std::function<void(QVariant, std::exception_ptr)>;
    Client(Config conf, Worker* parent);
    void Start();
    void Execute(string cmd, Callback cb);
signals:
    void Error(QString err);
    void ConnectedChanged(bool state);
private:
    struct Impl;
    void doConnect();
    void reconnectLater();
};


}
