#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <QProcess>
#include <QCoreApplication>
#include <QMetaEnum>

namespace radapter {

struct ProcessConfig : WorkerConfig {
    QString program;
    WithDefault<vector<QString>> arguments {};
    optional<QString> working_dir;
    WithDefault<bool> autostart = true;
    WithDefault<bool> merge_stderr = false;   // fold stderr into the stdout data channel
};

RAD_DESCRIBE(ProcessConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(program);
    RAD_MEMBER(arguments);
    RAD_MEMBER(working_dir);
    RAD_MEMBER(autostart);
    RAD_MEMBER(merge_stderr);
}

// Wraps a child process (QProcess). stdout is the data channel (pipe(proc, fn));
// stderr and lifecycle land on the event channel (proc.events): { stderr }, { started },
// { finished=true, exit_code } on normal exit / { finished=true, signal=true } when killed
// by a signal, { error }. Inbound messages (strings/bytes) are written to stdin.
// The child is terminated when the worker is destroyed, so it never outlives the instance.
class ProcessWorker : public Worker {
    Q_OBJECT

    ProcessConfig config;
    QProcess* proc;
public:
    ProcessWorker(ProcessConfig conf, Instance* inst) :
        Worker(inst, EnsureName(conf, conf.program), "process")
    {
        config = std::move(conf);
        if (config.program.isEmpty()) {
            Raise("Process: 'program' must not be empty");
        }
        proc = new QProcess(this);
        if (config.working_dir) {
            proc->setWorkingDirectory(*config.working_dir);
        }
        if (config.merge_stderr.value) {
            proc->setProcessChannelMode(QProcess::MergedChannels);
        }

        connect(proc, &QProcess::readyReadStandardOutput, this, [this]{
            emit SendMsgField("stdout", proc->readAllStandardOutput());
        });
        connect(proc, &QProcess::readyReadStandardError, this, [this]{
            emit SendEventField("stderr", proc->readAllStandardError());
        });
        connect(proc, &QProcess::started, this, [this]{
            Info("started (pid {})", proc->processId());
            emit SendEventField("started", qlonglong(proc->processId()));
        });
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus status){
            QVariantMap ev{ {"finished", true} };
            if (status == QProcess::CrashExit) {
                // QProcess has no public API for the signal number; surface that the
                // process was terminated by a signal rather than exiting normally.
                ev["signal"] = true;
                Info("finished (terminated by signal)");
            } else {
                ev["exit_code"] = code;
                Info("finished (exit code {})", code);
            }
            emit SendEvent(ev);
        });
        connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err){
            auto name = QMetaEnum::fromType<QProcess::ProcessError>().valueToKey(err);
            Warn("process error: {}", name);
            emit SendEventField("error", QString::fromLatin1(name));
        });

        if (config.autostart.value) {
            launch();
        }
    }

    void launch() {
        if (proc->state() != QProcess::NotRunning) {
            Warn("already running");
            return;
        }
        QStringList qargs;
        for (auto& a: config.arguments.value) qargs << a;
        proc->start(config.program, qargs);
    }

    void OnMsg(QVariant const& msg) override {
        if (proc->state() != QProcess::Running) {
            Warn("stdin write ignored: process not running");
            return;
        }
        if (msg.type() == QVariant::ByteArray) {
            proc->write(msg.toByteArray());
        } else if (msg.type() == QVariant::String) {
            proc->write(msg.toString().toUtf8());
        } else {
            Warn("only strings/bytes can be written to stdin");
        }
    }

    QVariant Start(QVariantList const&)      { launch(); return {}; }
    QVariant Terminate(QVariantList const&)  { proc->terminate(); return {}; }
    QVariant Kill(QVariantList const&)       { proc->kill(); return {}; }
    QVariant CloseStdin(QVariantList const&) { proc->closeWriteChannel(); return {}; }
    QVariant Pid(QVariantList const&)        { return qlonglong(proc->processId()); }

    QVariant Write(QVariantList const& a) {
        if (a.isEmpty()) Raise("Write(data): missing data argument");
        auto& v = a[0];
        auto bytes = v.type() == QVariant::ByteArray ? v.toByteArray() : v.toString().toUtf8();
        return qlonglong(proc->write(bytes));
    }

    QVariant State(QVariantList const&) {
        switch (proc->state()) {
        case QProcess::NotRunning: return QStringLiteral("not_running");
        case QProcess::Starting:   return QStringLiteral("starting");
        case QProcess::Running:    return QStringLiteral("running");
        }
        return {};
    }

    void Destroy() override {
        if (proc->state() != QProcess::NotRunning) {
            proc->terminate();
            if (!proc->waitForFinished(500)) {
                proc->kill();
                proc->waitForFinished(200);
            }
        }
        Worker::Destroy();
    }
};

void builtin::workers::process(Instance* inst) {
    inst->RegisterWorker<ProcessWorker>("Process", {
        {"Start",      AsExtraMethod<&ProcessWorker::Start>},
        {"Write",      AsExtraMethod<&ProcessWorker::Write>},
        {"Terminate",  AsExtraMethod<&ProcessWorker::Terminate>},
        {"Kill",       AsExtraMethod<&ProcessWorker::Kill>},
        {"CloseStdin", AsExtraMethod<&ProcessWorker::CloseStdin>},
        {"Pid",        AsExtraMethod<&ProcessWorker::Pid>},
        {"State",      AsExtraMethod<&ProcessWorker::State>},
    });
    inst->RegisterSchema<ProcessConfig>("Process");

    // running-process info via Qt APIs — notably the path to spawn a sibling adapter
    inst->RegisterFunc("app_info", [](Instance*, QVariantList const&) -> QVariant {
        return QVariantMap{
            {"executable", QCoreApplication::applicationFilePath()},
            {"dir",        QCoreApplication::applicationDirPath()},
            {"name",       QCoreApplication::applicationName()},
            {"pid",        qlonglong(QCoreApplication::applicationPid())},
            {"qt_version", QString::fromLatin1(qVersion())},
        };
    });
}

}

#include "process.moc"
