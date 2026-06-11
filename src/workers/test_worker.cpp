#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QTimer>

namespace radapter {

struct TestConfig : WorkerConfig {
    WithDefault<int> delay = 1000;
};


RAD_DESCRIBE(TestConfig) {
    PARENT(WorkerConfig);
    MEMBER("delay", &_::delay);
}

class TestWorker : public Worker {
    Q_OBJECT
public:
    TestConfig conf;
    unsigned current = 0;
    TestWorker(TestConfig config, Instance* parent) :
        Worker(parent, config, "test")
    {
        conf = std::move(config);
        auto timer = new QTimer(this);
        timer->callOnTimeout(this, [this]{
            emit SendMsg(current++);
        });
        timer->start(conf.delay.value);
    }

    void OnMsg(QVariant const& msg) override {
        auto sender = CurrentSender();
        auto* w = sender.value<Worker*>();
        if (w) {
            assert(w == this);
        }
        Info("Msg => '{}'", msg.toString());
    }

    QVariant Call(std::optional<LuaFunction> fn) {
        if (fn) fn->Call({1, 2, 3});
        return {};
    }
};

void builtin::workers::test(Instance* inst) {
    inst->RegisterWorker<TestWorker>("TestWorker", {
        {"Call", AsExtraMethod<&TestWorker::Call>},
    });
    inst->RegisterSchema("TestWorker", SchemaFor<TestConfig>);
}

}


#include "test_worker.moc"
