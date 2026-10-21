// Benchmark-only plugin: a `Bench` worker that exposes the Lua <-> QVariant
// message boundary with no side effects. Messages it emits reach Lua listeners
// as lazy views (the engine's normal notification path), so the lazy benchmarks
// measure the real implementation.

#include "radapter/radapter.hpp"
#include "radapter/worker.hpp"
#include <QVariant>
#include <lua.hpp>

using namespace radapter;

namespace {

struct BenchConfig : WorkerConfig {};
RAD_DESCRIBE(BenchConfig) {
    PARENT(WorkerConfig);
}

QVariant buildDeep(int depth, int width) {
    if (depth <= 0) {
        QVariantList list;
        list.reserve(width);
        for (int i = 0; i < width; ++i) list.append(i);
        return list;
    }
    QVariantMap m;
    m.insert(QStringLiteral("a"), buildDeep(depth - 1, width));
    for (int i = 1; i < width; ++i) {
        m.insert(QStringLiteral("k") + QString::number(i), i);
    }
    return m;
}

int argInt(QVariantList const& args, int idx, int def) {
    if (args.size() <= idx) return def;
    return int(args[idx].toLongLong());
}

class BenchWorker final : public Worker {
    Q_OBJECT
    QVariant retained;
public:
    BenchWorker(BenchConfig conf, Instance* inst) :
        Worker(inst, conf, "bench")
    {}

    void OnMsg(QVariant const&) override {}

    QVariant Noop(QVariantList const&) { return {}; }

    QVariant Sink(QVariantList const&) { return {}; }

    QVariant Roundtrip(QVariantList const& args) { return args.value(0); }

    QVariant Echo(QVariantList const& args) {
        if (!args.isEmpty()) {
            emit SendMsg(args.first());
        }
        return {};
    }

    QVariant Deep(QVariantList const& args) {
        return buildDeep(argInt(args, 0, 1), argInt(args, 1, 1));
    }

    QVariant Prepare(QVariantList const& args) {
        retained = buildDeep(argInt(args, 0, 1), argInt(args, 1, 1));
        return {};
    }

    QVariant DeepCached(QVariantList const&) { return retained; }

    QVariant EchoCached(QVariantList const&) {
        emit SendMsg(retained);
        return {};
    }
};

} // namespace

RADAPTER_PLUGIN(Bench, "radapter.bench.plugin") {
    radapter->RegisterWorker<BenchWorker>("Bench", {
        {"Noop", AsExtraMethod<&BenchWorker::Noop>},
        {"Sink", AsExtraMethod<&BenchWorker::Sink>},
        {"Roundtrip", AsExtraMethod<&BenchWorker::Roundtrip>},
        {"Echo", AsExtraMethod<&BenchWorker::Echo>},
        {"Deep", AsExtraMethod<&BenchWorker::Deep>},
        {"Prepare", AsExtraMethod<&BenchWorker::Prepare>},
        {"DeepCached", AsExtraMethod<&BenchWorker::DeepCached>},
        {"EchoCached", AsExtraMethod<&BenchWorker::EchoCached>},
    });
    radapter->RegisterSchema("Bench", SchemaFor<BenchConfig>);
}

#include "bench_plugin.moc"
