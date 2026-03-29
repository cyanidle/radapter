#include "radapter/radapter.hpp"
#include "radapter/worker.hpp"
#include "radapter/config.hpp"
#include <qtimer.h>

using namespace radapter;

struct TestPluginConfig {
    WithDefault<int> delay = 1000;
};

RAD_DESCRIBE(TestPluginConfig) {
    RAD_MEMBER(delay);
}

class TestPlugin final : public radapter::Worker {
    Q_OBJECT

    TestPluginConfig config;
public:
    TestPlugin(TestPluginConfig config, Instance* inst) :
        Worker(inst, "test_plugin"),
        config(config)
    {
        auto timer = new QTimer(this);
        timer->callOnTimeout(this, [this]{
            emit SendMsg(QVariantMap{{"from_plugin", "Hello!"}});
        });
        timer->start(config.delay.value);
    }
    void OnMsg(QVariant const& msg) override {
        Info("Msg: {}", msg.toString());
        emit SendEventField("msg", "received");
    }
};

RADAPTER_PLUGIN("radapter.plugins.Test") {
    radapter->Info("global!", "Log on plugin load!");
    radapter->RegisterWorker<TestPlugin>("TestPlugin");
}

#include "test.moc"
