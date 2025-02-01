#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QJsonDocument>
#include <efsw/efsw.hpp>
#include <qctrlsignalhandler.h>
#include "radapter/radapter.hpp"
#include "argparse/argparse.hpp"

#ifdef RADAPTER_GUI
#include <QGuiApplication>
#endif

using namespace efsw;
using std::vector;
using std::string;

using Strings = vector<string>;

class Listener final : public QObject, public efsw::FileWatchListener {
    Q_OBJECT
public:
    QElapsedTimer elapsed;
    void handleFileAction(
        WatchID, const std::string&,
        const std::string&, Action act,
        std::string) final
    {
        if (act == Action::Modified && elapsed.hasExpired(500)) {
            elapsed.restart();
            QMetaObject::invokeMethod(this, [this]{
                emit FileChanged();
            });
        }
    }
    Listener() {
        elapsed.start();
    }
signals:
    void FileChanged();
};

struct AppConfig {
    argparse::ArgumentParser& cli;
    QStringList& lua_args;
    Strings& exprs;
    QCtrlSignalHandler* sigs;
    Listener* listener;
};

class AppState : public QObject {
    Q_OBJECT
public:
    radapter::Instance* inst;
    const AppConfig* config;
    bool sigint = false;
    bool sigterm = false;

    AppState(const AppConfig* _config) : config(_config) {
        inst = new radapter::Instance(this);

        QObject::connect(inst, &radapter::Instance::ShutdownDone, this, &QObject::deleteLater);

        QObject::connect(config->sigs, &QCtrlSignalHandler::sigInt, this, [this]{
            sigint = true;
            shutdown();
        });
        QObject::connect(config->sigs, &QCtrlSignalHandler::sigTerm, this, [this]{
            sigterm = true;
            shutdown();
        });

        if (radapter::GUI && config->cli["gui"] == true) {
            inst->EnableGui();
        }

        if (config->cli["schema"] == true) {
            std::cerr << QJsonDocument::fromVariant(inst->GetSchemas()).toJson().toStdString() << std::endl;
            std::exit(0);
        }

        inst->RegisterGlobal("args", config->lua_args);

        for (auto& e: config->exprs) {
            inst->Eval(e);
        }

        bool debug = config->cli["debug"] == true;
        bool debugVscode = config->cli["debug-vscode"] == true;

        if (debug || debugVscode) {
            radapter::Instance::DebuggerOpts opts;
            opts.host = config->cli.get("debug-host");
            opts.port = config->cli.get<uint16_t>("debug-port");
            opts.vscode = debugVscode;
            inst->DebuggerConnect(opts);
        }

        if (auto f = config->cli.present("file")) {
            inst->EvalFile(radapter::fs::u8path(*f));
        }
        if (config->listener) {
            QObject::connect(config->listener, &Listener::FileChanged, this, [this, done = false]() mutable {
                if (!done) reload();
                done = true;
            });
        }
    }

    void shutdown() {
        QObject::connect(inst, &radapter::Instance::ShutdownDone, qApp, &QCoreApplication::quit);
        inst->Shutdown(sigterm ? 500 : 5000);
        return;
    }

    void reload() {
        std::cerr << "# Hot reload..." << std::endl;
        QObject::connect(inst, &QObject::destroyed, [config = config]{
            try {
                new AppState{config};
                std::cerr << "# Hot reload done." << std::endl;
            } catch (std::exception& e) {
                std::cerr << "# Hot reload error: " << e.what() << std::endl;
                qApp->exit(1);
            }
        });
        inst->Shutdown();
    }

    ~AppState() {}
};

int main (int argc, char **argv) try {
    std::unique_ptr<QCoreApplication> app;
#ifdef RADAPTER_GUI
    for (auto it = argv; it != argv + argc; ++it) {
        if (strcmp(*it, "--gui") == 0) {
            auto gapp = new QGuiApplication(argc, argv);
            gapp->setQuitOnLastWindowClosed(false);
            app.reset(gapp);
            break;
        }
    }
#endif
    if (!app) {
        app.reset(new QCoreApplication(argc, argv));
    }
    std::vector<std::string> args;
    for (auto a: app->arguments()) {
        args.push_back(a.toStdString());
    }

    std::vector<std::string> exprs;
    std::vector<std::string> watch_dirs;
    QStringList lua_args;
    auto ver = fmt::format(
        "{}.{}{} ({}+{})",
        radapter::VerMajor,
        radapter::VerMinor,
        radapter::JIT ? "-jit" : "",
        radapter::BuildId,
        radapter::BuildDate);
    argparse::ArgumentParser cli(argv[0], ver);
    cli.add_argument("file")
        .nargs(argparse::nargs_pattern::optional)
        .help("Execute file");
    cli.add_argument("args")
        .nargs(argparse::nargs_pattern::any)
        .action([&](std::string_view str){
            lua_args.push_back(QString::fromUtf8(str.data(), int(str.size())));
        })
        .help("Args array to be passed to script");
    cli.add_argument("-e", "--eval")
        .append()
        .store_into(exprs)
        .help("Execute expressions from cli");
    if constexpr (radapter::GUI) {
        cli.add_argument("--gui")
            .flag()
            .help("Enable native gui mode");
    }
    cli.add_argument("--watch-dir")
        .append()
        .store_into(watch_dirs)
        .help("Reload on modified files in <dir>");
    cli.add_argument("--schema")
        .flag()
        .help("Print config schema");
    cli.add_argument("--debug")
        .flag()
        .help("Enable debugger Mobdebug");
    cli.add_argument("--debug-vscode")
        .flag()
        .help("Debugger vscode implementation (json-based Mobdebug extension)");
    cli.add_argument("--debug-host")
        .default_value("127.0.0.1")
        .help("Debugger listen host");
    cli.add_argument("--debug-port")
        .scan<'u', uint16_t>()
        .default_value(uint16_t{8172})
        .help("Debugger listen port");
    try {
        cli.parse_args(args);
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << cli << std::endl;
        return 1;
    }
    auto sigs = QCtrlSignalHandler::instance();
    sigs->registerForSignal(QCtrlSignalHandler::SigInt);
    sigs->registerForSignal(QCtrlSignalHandler::SigTerm);

    std::optional<Listener> listener;
    std::optional<FileWatcher> watcher;

    if (watch_dirs.size()) {
        watcher.emplace();
        listener.emplace();
        for (auto& dir: watch_dirs) {
            auto id = watcher->addWatch(dir, &*listener);
            if (id < 0) {
                std::cerr << "# WARN: could not watch: " << dir << ": " << Errors::Log::getLastErrorLog() << std::endl;
            } else {
                std::cerr << "# Watching directory: " << dir << std::endl;
            }
        }
    }

    AppConfig cfg{cli, lua_args, exprs, sigs, listener ? &*listener : nullptr};

    new AppState{&cfg};

    QObject::connect(qApp, &QCoreApplication::aboutToQuit, []{
        std::cerr << "# About to quit" << std::endl;
    });

    if (watcher) {
        watcher->watch();
    }

    return app->exec();
} catch (std::exception& exc) {
    std::cerr << "Critical: " << exc.what() << std::endl;
    return 1;
}

#include "main.moc"
