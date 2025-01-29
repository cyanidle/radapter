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
    TestPlugin(QVariantList const& args, Instance* inst) :
        Worker(inst, "test_plugin")
    {
        Parse(config, args.value(0));
        auto timer = new QTimer(this);
        timer->callOnTimeout(this, [this]{
            emit SendMsg(QVariantMap{{"from_plugin", "Hello!"}});
        });
        timer->start(config.delay.value);
    }
    void OnMsg(QVariant const& msg) override {
        Info("Msg: {}", msg.toString());
    }
};

RADAPTER_PLUGIN(TestPlugin, "radapter.plugins.Test") {
    radapter->Info("global!", "Log on plugin load!");
}

#include "test.moc"
