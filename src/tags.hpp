#pragma once
#include "radapter/radapter.hpp"
#include "radapter/function.hpp"

namespace radapter {

class TagRegistry : public QObject {
    Q_OBJECT
public:
    enum class Quality : uint8_t { Good, CommFail };
    static constexpr const char* qualityStr(Quality q) noexcept {
        return q == Quality::Good ? "good" : "comm_fail";
    }

    struct Tag {
        QVariant value;
        qint64 ts = 0;
        Quality quality = Quality::CommFail;
        QPointer<Worker> source;
        QString field;
        QVector<LuaFunction> subscribers;
    };

    LuaValue changedListeners;
    LuaValue changedObj;

    explicit TagRegistry(Instance* inst);

    void Subscribe(QString const& tagName, LuaFunction fn);
    Tag const* GetTag(QString const& tagName) const;
    void Advertise(Worker* w, QStringList const& fields);

    // get-or-create the per-tag listener list behind tags.changed["name"]
    LuaValue& PerTagListeners(QString const& tagName);

    // called from worker_notify before Lua listeners fire
    void onWorkerMsg(Worker* w, QVariant const& msg);
    void onWorkerEvent(Worker* w, QVariant const& msg);

public slots:
    void onWorkerCreated(Worker* w);

private:
    void setWorkerQuality(Worker* w, Quality q);
    void updateTag(QString const& tagName, QVariant const& value, Worker* source);
    void notifyTag(QString const& tagName, Tag const& tag);

    Instance* _inst;
    QMap<QString, Tag> _tags;
    QMap<QString, LuaValue> _perTag; // per-tag changed-listener lists
};

} // namespace radapter
