#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include "./builtin.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>

namespace radapter::gui 
{

struct QmlSettings {
	QString url;
};

DESCRIBE("gui::QmlSettings", QmlSettings) {
	MEMBER("url", &_::url);
}

class QmlWorker final : public radapter::Worker
{
	Q_OBJECT
private:
	QmlSettings config;
	QQmlEngine engine;
	QQmlComponent* component;
	QObject* instance;
public:
	QmlWorker(QVariantList const& args, radapter::Instance* inst) :
		Worker(inst, "qml")
	{
		Parse(config, args.value(0));
		component = new QQmlComponent(&engine, config.url);
		instance = component->create();
		if (!instance) {
			throw Err("Could not create qml view: {}", component->errorString());
		}
		instance->setParent(this);
	}
	void OnMsg(QVariant const& msg) override {

	}
};

}

namespace radapter::builtin {


void workers::gui(Instance* inst) 
{
	inst->RegisterWorker<radapter::gui::QmlWorker>("Qml");
}

}


#include "gui.moc"