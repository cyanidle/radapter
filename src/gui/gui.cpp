#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include "./builtin.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTemporaryFile>

namespace radapter::gui 
{

struct QmlSettings {
    QString url;
};

DESCRIBE("gui::QmlSettings", QmlSettings, void) {
    MEMBER("url", &_::url);
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

class QMLWorker final : public radapter::Worker
{
	Q_OBJECT
private:
	QmlSettings config;
	QQmlEngine engine;
	QQmlComponent* component;
    QObject* instance;
    QTemporaryFile* temp = nullptr;
public:
    QMLWorker(QVariantList const& args, radapter::Instance* inst) :
		Worker(inst, "qml")
    {
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
        component = new QQmlComponent(&engine, config.url);
		instance = component->create();
		if (!instance) {
			throw Err("Could not create qml view: {}", component->errorString());
		}
        instance->setParent(this);
        auto* meta = instance->metaObject();
        auto count = meta->methodCount();
        for (auto i = 0; i < count; ++i) {
            auto method = meta->method(i);
            if (method.methodType() != QMetaMethod::Signal) continue;
            auto name = method.name();
            if (name.compare("sendMsg", Qt::CaseInsensitive) == 0)
            {
                auto selfIdx = metaObject()->indexOfMethod("handleMsgFromQml(QVariant)");
                meta->connect(instance, i, this, selfIdx);
                break;
            }
        }
	}
    QVariant url(QVariantList) {
        return config.url;
    }
	void OnMsg(QVariant const& msg) override {
        applyToQml(msg, instance);
	}
public slots:
    void handleMsgFromQml(QVariant const& var) {
        if (var.type() >= QVariant::UserType) {
            emit SendMsg(var.value<QJSValue>().toVariant());
        } else {
            emit SendMsg(var);
        }
    }
};

}

namespace radapter::builtin {


void workers::gui(Instance* inst) 
{
    inst->RegisterWorker<radapter::gui::QMLWorker>("QML", {
        {"url", AsExtraMethod<&radapter::gui::QMLWorker::url>},
    });
}

}


#include "gui.moc"
