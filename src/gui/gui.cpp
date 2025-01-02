#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include "./builtin.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTemporaryFile>
#include <QHash>
#include <QTemporaryDir>

namespace radapter::gui 
{

struct QmlSettings {
    QString url;
    vector<QString> props;
};

DESCRIBE("gui::QmlSettings", QmlSettings, void) {
    MEMBER("url", &_::url);
    MEMBER("props", &_::props);
}

static void applyToQml(QVariant const& msg, QObject* target) {
    switch (msg.type()) {
    case QVariant::Map: {
        const auto& m = *static_cast<const QVariantMap*>(msg.constData());
        for (auto it = m.begin(); it != m.end(); ++it) {
            const auto& k = it.key();
            const auto& v = it.value();
            auto t = v.type();
            if (t == QVariant::Map || t == QVariant::List) {
                if (auto child = target->findChild<QObject*>(k, Qt::FindChildOption::FindDirectChildrenOnly)) {
                    applyToQml(v, child);
                } else if (auto nested = target->property(k.toStdString().c_str()).value<QObject*>()) {
                    applyToQml(v, nested);
                }
            } else {
                target->setProperty(k.toStdString().c_str(), v);
            }
        }
        break;
    }
    case QVariant::List: {
        const auto& l = *static_cast<const QVariantList*>(msg.constData());
        auto& children = target->children();
        auto sz = l.size();
        for (auto i = 0; i < sz; ++i) {
            auto& v = l[i];
            if (v.isValid()) {
                applyToQml(l[i], children[i]);
            }
        }
        break;
    }
    default: {
        break;
    }
    }
}

Q_GLOBAL_STATIC(QQmlEngine, g_engine)

class QMLWorker final : public radapter::Worker
{
	Q_OBJECT
private:
    QmlSettings config;
	QQmlComponent* component;
    QObject* instance;
    QTemporaryFile* temp = nullptr;
    QHash<int, int> sigsToProps;
public:
    QMLWorker(QVariantList const& args, radapter::Instance* inst) :
		Worker(inst, "qml")
    {
        auto* engine = g_engine();
        auto first = args.value(0);
        if (first.type() == QVariant::String) {
            temp = new QTemporaryFile(this);
            if (!temp->open()) {
                throw Err("Could not open temp file for script: {}", temp->errorString());
            }
            temp->write(first.toString().toStdString().c_str());
            temp->flush();
            config.url = "file:///" + temp->fileName();
        } else {
            Parse(config, first);
        }
        component = new QQmlComponent(engine, config.url);
		instance = component->create();
		if (!instance) {
			throw Err("Could not create qml view: {}", component->errorString());
		}
        instance->setParent(this);
        auto* meta = instance->metaObject();
        auto sigidx = meta->indexOfMethod("sendMsg(QVariant)");
        if (sigidx != -1 && meta->method(sigidx).methodType() == QMetaMethod::Signal) {
            connect(instance, SIGNAL(sendMsg(QVariant)), this, SLOT(handleMsgFromQml(QVariant)));
        } else {
            Info("No 'signal sendMsg(variant msg)' declared in component");
        }
        auto handler = metaObject()->indexOfMethod("handlePropChange()");
        for (auto& prop: config.props) {
            auto idx = meta->indexOfProperty(prop.toStdString().c_str());
            if (idx != - 1) {
                auto p = meta->property(idx);
                auto notif = p.notifySignalIndex();
                sigsToProps[notif] = idx;
                meta->connect(instance, notif, this, handler);
                p.read(instance); //kinda kostyl to force QML to update prop aliases
            } else {
                Warn("No property found in loaded qml component: {}", prop);
            }
        }
    }
	void OnMsg(QVariant const& msg) override {
        applyToQml(msg, instance);
    }
    static QVariant unwrap(QVariant const& var) {
        if (var.type() >= QVariant::UserType) {
            return var.value<QJSValue>().toVariant();
        } else {
            return var;
        }
    }
public slots:
    void handlePropChange() {
        auto prop = instance->metaObject()->property(sigsToProps.value(senderSignalIndex(), -1));
        QVariantMap msg {{prop.name(), unwrap(prop.read(instance))}};
        emit SendMsg(msg);
    }
    void handleMsgFromQml(QVariant const& var) {
        emit SendMsg(unwrap(var));
    }
};

}

namespace radapter::builtin {


void workers::gui(Instance* inst) 
{
    inst->RegisterWorker<radapter::gui::QMLWorker>("QML");
}

}


#include "gui.moc"
