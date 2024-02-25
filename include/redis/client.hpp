#pragma once

#include "common.hpp"
#include <QObject>
#include <QVariant>
#include <fmt/core.h>

class QtRedisAdapter;
struct redisAsyncContext;
namespace radapter::redis
{

class Client : public QObject
{
    Q_OBJECT
public:

    struct Exception : public std::exception {
        Exception(std::string reason = "Unknown") : reason(reason) {}
        std::string reason;
        const char* what() const noexcept override {
            return reason.c_str();
        }
    };

    using ResultCallback = std::function<void(QVariant)>;

    struct Settings {
        string host = "127.0.0.1";
        uint16_t port = 6379;
        uint16_t db = 0;
    };


    template<typename Fmt, typename...Args>
    ThenHelper Execute(Fmt&& fmt, Args&&...args) {
        return Execute(fmt::format(std::forward<Fmt>(fmt), std::forward<Args>(args)...));
    }
    ThenHelper Execute(std::string cmd) {
        return ThenHelper{cmd, *this};
    }
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
    QtRedisAdapter* adapter {};
};

}

Q_DECLARE_METATYPE(radapter::redis::Client::Exception)
