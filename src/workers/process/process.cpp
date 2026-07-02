#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "../binary_worker.hpp"
#include <QProcess>
#include <QCoreApplication>
#include <QMetaEnum>
#include <QJsonDocument>
#include <signal.h>

namespace radapter {

struct ProcessConfig : WorkerConfig {
    QString program;
    WithDefault<vector<QString>> arguments {};
    optional<QString> working_dir;
    WithDefault<bool> autostart = true;
    WithDefault<bool> merge_stderr = false;
    std::optional<BinaryConfig> binary;
};

RAD_DESCRIBE(ProcessConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(program);
    RAD_MEMBER(arguments);
    RAD_MEMBER(working_dir);
    RAD_MEMBER(autostart);
    RAD_MEMBER(merge_stderr);
    RAD_MEMBER(binary);
}

// Wraps a child process (QProcess). stdout lands on the data channel: in text
// mode as {stdout = <string>}, in binary mode as arbitrary objects decoded by
// the BinaryWorker framing. stderr and lifecycle land on the event channel.
// Inbound messages are written to stdin (text mode: strings/bytes; binary
// mode: arbitrary objects via msgpack framing through BinaryWorker::OnMsg).
class ProcessWorker : public BinaryWorker {
    Q_OBJECT

    ProcessConfig config;
    QProcess* proc;
public:
    ProcessWorker(ProcessConfig conf, Instance* inst) :
        BinaryWorker(conf.binary.value_or(BinaryConfig{}), inst, "process")
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

        if (config.binary) {
            connect(proc, &QProcess::readyReadStandardOutput, this, [this]{
                auto data = proc->readAllStandardOutput();
                ReceiveBinary(data);
            });
        } else {
            connect(proc, &QProcess::readyReadStandardOutput, this, [this]{
                emit SendMsgField("stdout", QString::fromUtf8(proc->readAllStandardOutput()));
            });
        }

        connect(proc, &QProcess::readyReadStandardError, this, [this]{
            emit SendEventField("stderr", QString::fromUtf8(proc->readAllStandardError()));
        });
        connect(proc, &QProcess::started, this, [this]{
            Info("started (pid {})", proc->processId());
            emit SendEventField("started", qlonglong(proc->processId()));
        });
        connect(proc, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus status){
            QVariantMap ev{ {"finished", true} };
            if (status == QProcess::CrashExit) {
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
        if (config.binary) {
            BinaryWorker::OnMsg(msg);
            return;
        }
        // Text mode: write to stdin
        if (proc->state() != QProcess::Running) {
            Warn("stdin write ignored: process not running");
            return;
        }
        if (msg.metaType().id() == QMetaType::QByteArray) {
            proc->write(msg.toByteArray());
        } else if (msg.metaType().id() == QMetaType::QString) {
            proc->write(msg.toString().toUtf8());
        } else {
            Warn("only strings/bytes can be written to stdin");
        }
    }

    QVariant Start(QVariantList const&)      { launch(); return {}; }
    QVariant Terminate(QVariantList const&)  { proc->terminate(); return {}; }
    QVariant Kill(QVariantList const&)       { proc->kill(); return {}; }
    QVariant Signal(QVariantList const& a) {
        if (a.isEmpty()) Raise("Signal(name_or_num): missing argument");
        auto& arg = a[0];
        int sig = 0;
        if (arg.metaType().id() == QMetaType::Int || arg.metaType().id() == QMetaType::LongLong) {
            sig = arg.toInt();
        } else {
            auto name = arg.toString().toUpper().toLatin1();
            if (name.startsWith("SIG")) name = name.mid(3);
            bool ok = false;
            sig = name.toInt(&ok);
            if (!ok) {
                static QHash<QByteArray, int> names{
#ifdef SIGINT
                    {"INT", SIGINT},
#endif
#ifdef SIGHUP
                    {"HUP", SIGHUP},
#endif
#ifdef SIGTERM
                    {"TERM", SIGTERM},
#endif
#ifdef SIGKILL
                    {"KILL", SIGKILL},
#endif
#ifdef SIGQUIT
                    {"QUIT", SIGQUIT},
#endif
#ifdef SIGUSR1
                    {"USR1", SIGUSR1},
#endif
#ifdef SIGUSR2
                    {"USR2", SIGUSR2},
#endif
                };
                if (names.contains(name)) {
                    sig = names[name];
                } else {
                    Raise("Signal: unknown signal name '{}'", arg.toString());
                }
            }
        }
        auto pid = proc->processId();
        if (pid <= 0) Raise("Signal: process not running");
        if (sig == SIGTERM) { proc->terminate(); return {}; }
        if (sig == SIGKILL) { proc->kill(); return {}; }
#ifdef Q_OS_UNIX
        if (::kill(static_cast<pid_t>(pid), sig) != 0)
            Raise("Signal: kill({}, {}) failed: {}", pid, sig, strerror(errno));
#else
        Warn("Signal: arbitrary signal delivery not supported on this platform");
#endif
        return {};
    }
    QVariant CloseStdin(QVariantList const&) { proc->closeWriteChannel(); return {}; }
    QVariant Pid(QVariantList const&)        { return qlonglong(proc->processId()); }

    QVariant Write(QVariantList const& a) {
        if (a.isEmpty()) Raise("Write(data): missing data argument");
        auto& v = a[0];
        auto bytes = v.metaType().id() == QMetaType::QByteArray ? v.toByteArray() : v.toString().toUtf8();
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

    void SendBinary(string_view buffer) override {
        if (proc->state() == QProcess::Running)
            proc->write(buffer.data(), qint64(buffer.size()));
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
        {"Signal",     AsExtraMethod<&ProcessWorker::Signal>},
        {"CloseStdin", AsExtraMethod<&ProcessWorker::CloseStdin>},
        {"Pid",        AsExtraMethod<&ProcessWorker::Pid>},
        {"State",      AsExtraMethod<&ProcessWorker::State>},
    });
    inst->RegisterSchema<ProcessConfig>("Process");

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
