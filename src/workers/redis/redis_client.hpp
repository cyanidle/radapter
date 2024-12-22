#pragma once

#include "future/future.hpp"
#include "radapter/radapter.hpp"
#include <forward_list>

class QtRedisAdapter;
struct redisAsyncContext;

namespace radapter::redis {

struct Config {
    WithDefault<string> host = "localhost";
    WithDefault<uint16_t> port = uint16_t(6379);
    WithDefault<uint16_t> db = uint16_t(0);
    WithDefault<unsigned> reconnect_timeout = 1000u;
};

DESCRIBE("redis::Config", Config, void) {
    MEMBER("host", &_::host);
    MEMBER("port", &_::port);
    MEMBER("db", &_::db);
    MEMBER("reconnect_timeout", &_::reconnect_timeout);
}

struct RedisCmd {
    RedisCmd() = default;
    RedisCmd(string_view cmd) {
        Arg(cmd);
    }
    size_t Size() {
        return args.size();
    }
    void Arg(string_view arg) {
        args.push_back(arg);
    }
    // needed if string does not outlive this RedisCmd
    void Temp(string&& arg) {
        args.push_back(string_view{temp_args.emplace_front(std::move(arg))});
    }
private:
    std::forward_list<std::string> temp_args;
    std::vector<std::string_view> args;
    friend class Client;
};

using namespace fut;

class Client : public QObject {
    Q_OBJECT

    Config config;
    bool ok = false;
    bool reconPending = false;
    QtRedisAdapter* adapter{};
    redisAsyncContext* ctx{};
public:
    Client(Config conf, Worker* parent);
    ~Client() override;
    void Start();
    bool IsConnected() const;
    void ReconnectLater();
    fut::Future<QVariant> Execute(const string_view* argv, size_t argc);
    fut::Future<QVariant> Execute(std::initializer_list<string_view> args) {
        return Execute(&*args.begin(), args.size());
    }
    struct SubEvent {
        QString channel;
        QString message;
    };
    using Subscriber = std::function<void(SubEvent)>;
    void PSubscribe(string_view glob, Subscriber sub);
    fut::Future<QVariant> Execute(RedisCmd const& args) {
        return Execute(args.args.data(), args.args.size());
    }
signals:
    void Error(QString err);
    void ConnectedChanged(bool state);
private:
    struct Impl;
    void doConnect();
};

}
