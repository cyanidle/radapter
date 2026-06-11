#ifndef RADAPTER_WORKER_H
#define RADAPTER_WORKER_H

#include "logs.hpp"
#include "radapter/config.hpp"
#include <QtPlugin>
#include <qobject.h>

struct lua_State;

namespace radapter
{

struct WorkerImpl;
class Instance;
class Worker;


using ExtraMethod = QVariant(*)(Worker*, QVariantList const&);
using ExtraMethods = QMap<QString, ExtraMethod>;

struct RADAPTER_API WorkerConfig {
    optional<QString> name;
    optional<QString> category;

    bool generated_name = false;
};

RAD_DESCRIBE(WorkerConfig) {
    RAD_MEMBER(name);
    RAD_MEMBER(category);
}

template<typename C>
C& EnsureName(C& conf, QString def) {
    if (!conf.name || conf.name->isEmpty()) {
        conf.name = std::move(def);
        conf.generated_name = true;
    }
    return conf;
}

class RADAPTER_API Worker : public QObject {
    Q_OBJECT
public:
    Instance* _Inst;
    WorkerImpl* _Impl;
    string _Category;
    string _LogCat;
    string _Origin; // "file:line" of the creating Lua call, or "<CPP>"
    int _luaSelfRef = -1; // Lua registry ref to this worker's userdata (LUA_NOREF); set by push_worker

    Worker(Instance* parent, const char* category);
    Worker(Instance* parent, WorkerConfig const& conf, const char* category);

    QString Name() const {
        return objectName();
    }

    // Inform the tag registry about known output fields (call from ctor after fields are known)
    void AdvertiseFields(QStringList const& fields);

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
    virtual void Destroy();
    virtual ~Worker();
protected:
    QVariant CurrentSender();
signals:
    void ShutdownDone();
    void SendMsg(QVariant const& msg);
    void SendMsgField(QString key, QVariant const& msg);
    void SendEvent(QVariant const& msg);
    void SendEventField(QString key, QVariant const& data);
};

struct RADAPTER_API WorkerPlugin {
    virtual ~WorkerPlugin() = default;
    virtual void Initialize(Instance* target, QVariantList args) = 0;
};

}

#define RadapterWorkerPlugin_iid "radapter.plugins.Worker/1.1"
Q_DECLARE_INTERFACE(radapter::WorkerPlugin, RadapterWorkerPlugin_iid)

#define RADAPTER_PLUGIN(plugin, iid) \
    class plugin##_RadPluginImpl final : public QObject, public radapter::WorkerPlugin \
    {  \
        Q_OBJECT  \
        Q_PLUGIN_METADATA(IID iid) \
        Q_INTERFACES(radapter::WorkerPlugin)  \
    public:  \
        void Initialize(radapter::Instance* radapter, QVariantList args) override;\
    }; \
void plugin##_RadPluginImpl::Initialize(radapter::Instance* radapter, QVariantList args)

#endif //RADAPTER_WORKER_H
