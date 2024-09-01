#ifndef RADAPTER_WORKER_H
#define RADAPTER_WORKER_H

#include "logs.hpp"
#include "config.hpp"

struct lua_State;

namespace radapter
{

class Instance;
class Worker : public QObject {
    Q_OBJECT
public:
    Instance* _Inst;
    int _self;
    int _subs;

    Worker(Instance* parent, const char* category = "worker");
    void Log(LogLevel lvl, fmt::string_view fmt, fmt::format_args args);
    template<typename...Args>
    void Debug(fmt::format_string<Args...> fmt, Args&&...a) {
        Log(debug, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Info(fmt::format_string<Args...> fmt, Args&&...a) {
        Log(info, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Warn(fmt::format_string<Args...> fmt, Args&&...a) {
        Log(warn, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Error(fmt::format_string<Args...> fmt, Args&&...a) {
        Log(error, fmt, fmt::make_format_args(a...));
    }
    virtual void OnMsg(QVariant const& msg) = 0;
    virtual void Shutdown();
    virtual ~Worker();
signals:
    void ShutdownDone();
    void SendMsg(QVariant const& msg);
private:
    const char *_category;
};

}

#endif //RADAPTER_WORKER_H
