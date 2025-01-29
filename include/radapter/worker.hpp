#ifndef RADAPTER_WORKER_H
#define RADAPTER_WORKER_H

#include "logs.hpp"
#include "radapter/config.hpp"
#include <QtPlugin>

struct lua_State;

namespace radapter
{

struct WorkerImpl;
class Instance;
class Worker;


using ExtraMethod = QVariant(*)(Worker*, QVariantList const&);
using ExtraMethods = QMap<QString, ExtraMethod>;

class RADAPTER_API Worker : public QObject {
    Q_OBJECT
public:
    Instance* _Inst;
    WorkerImpl* _Impl;

    Worker(Instance* parent, const char* category);
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
    lua_State* LuaState() const;

    virtual void OnMsg(QVariant const& msg) = 0;
    virtual void Shutdown();
    virtual ~Worker();
protected:
    QVariant CurrentSender();
signals:
    void ShutdownDone();
    void SendMsg(QVariant const& msg);
private:
    const char *_category;
};

struct RADAPTER_API WorkerPlugin {
    virtual ~WorkerPlugin() = default;
    virtual void RegisterGlobals(Instance* target) {(void)target;}
    virtual const char* ClassName() = 0;
    virtual Worker* Create(QVariantList const& args, Instance* inst) = 0;
    virtual const ExtraMethods* GetExtraMethods() { return nullptr; }
};

namespace impl {
void RADAPTER_API push_worker(Instance* inst, const char* clsname, Worker* w, ExtraMethods const& methods);
}

}


#define RadapterWorkerPlugin_iid "radapter.plugins.Worker/1.0"
Q_DECLARE_INTERFACE(radapter::WorkerPlugin, RadapterWorkerPlugin_iid)

#define RADAPTER_PLUGIN(worker, iid) \
    class worker##_RadPluginImpl final : public QObject, public radapter::WorkerPlugin \
    {  \
        Q_OBJECT  \
        Q_PLUGIN_METADATA(IID iid) \
        Q_INTERFACES(radapter::WorkerPlugin)  \
    public:  \
        const char* ClassName() override { return #worker; }  \
        Worker* Create(QVariantList const& args, Instance* inst) override { return new worker(args, inst);} \
        void RegisterGlobals(Instance* radapter) override;\
    }; \
void worker##_RadPluginImpl::RegisterGlobals(Instance* radapter)

#endif //RADAPTER_WORKER_H
