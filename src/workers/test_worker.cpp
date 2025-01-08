#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QTimer>

namespace radapter {

struct TestConfig {
    WithDefault<string> name = "TestWorker";
    WithDefault<int> delay = 1000;

};


DESCRIBE("radapter::ws::TestConfig", TestConfig, void) {
    MEMBER("delay", &_::delay);
    MEMBER("name", &_::name);
}

class TestWorker : public Worker {
    Q_OBJECT
public:
    TestConfig conf;
    unsigned current = 0;
    TestWorker(QVariantList args, Instance* parent) :
        Worker(parent, "test")
    {
        Parse(conf, args.value(0));
        setObjectName(conf.name.value.c_str());
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

    QVariant Call(QVariantList args) {
        Info("Called with {} args", args.size());
        if (auto f = args.value(0).value<LuaFunction>()) {
            f({1, 2, 3});
        }
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
