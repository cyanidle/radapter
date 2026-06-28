#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QJsonDocument>
#include <QFile>
#include <efsw/efsw.hpp>
#include <qctrlsignalhandler.h>
#include "radapter/radapter.hpp"
#include "argparse/argparse.hpp"

#ifdef RADAPTER_GUI
#include <QGuiApplication>
#include <QApplication>
#endif

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace efsw;
using std::vector;
using std::string;

using Strings = vector<string>;

// captured at the very start of main() before any QApplication can strip Qt
// arguments, so a re-exec relaunches with exactly what the user passed.
static std::vector<std::string> g_argv;

// true from the start of a reload until the fresh instance is up. Guards against
// pre-reload side effects (e.g. a build writing into a watched dir) retriggering
// a reload that is already in flight.
static bool g_reloading = false;

static bool g_exited = false;

// Replace the running process image with a fresh launch of the (possibly
// rebuilt) executable. POSIX only; returns only on failure.
static void reexec() {
#ifdef Q_OS_UNIX
    auto exe = QCoreApplication::applicationFilePath().toStdString();
    std::vector<char*> a;
    for (auto& s : g_argv) a.push_back(const_cast<char*>(s.c_str()));
    a.push_back(nullptr);
    std::cerr << "# Re-exec: " << exe << std::endl;
    execv(exe.c_str(), a.data());
    std::perror("# Re-exec failed");
#endif
}

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
    bool shutdownOnLastClosed;
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

#ifdef RADAPTER_GUI
        if (auto* gapp = qobject_cast<QGuiApplication*>(qApp); gapp && config->shutdownOnLastClosed) {
            connect(gapp, &QGuiApplication::lastWindowClosed, inst, [inst=inst]{
                inst->Shutdown();
            });
        }
#endif

        QObject::connect(inst, &radapter::Instance::ShutdownDone, this, &QObject::deleteLater);

        QObject::connect(inst, &radapter::Instance::ShutdownDone, []{
            if (!g_reloading)
            {
                g_exited = true;
                qApp->quit();
            }
        });

        QObject::connect(config->sigs, &QCtrlSignalHandler::sigInt, this, [this]{
            sigint = true;
            shutdown();
        });
        QObject::connect(config->sigs, &QCtrlSignalHandler::sigTerm, this, [this]{
            sigterm = true;
            shutdown();
        });

#ifdef RADAPTER_GUI
        if (qobject_cast<QGuiApplication*>(qApp)) {
            inst->EnableGui();
        }
#endif

        // --gui-record: start recording before user code runs (so windows created
        // during eval are captured) and save on any exit path.
        if constexpr (radapter::GUI) {
            if (auto recPath = config->cli.present<std::string>("gui-record")) {
                auto path = QString::fromStdString(*recPath);
                radapter::gui::StartRecording(inst);
                std::cerr << "# --gui-record: recording to " << *recPath << std::endl;

                auto saveRec = [this, path, saved = std::make_shared<bool>(false)] {
                    if (*saved) return;
                    *saved = true;
                    try {
                        auto evs = radapter::gui::StopRecording(inst);
                        auto data = QJsonDocument::fromVariant(evs).toJson(QJsonDocument::Indented);
                        QFile f(path);
                        if (!f.open(QIODevice::WriteOnly))
                            radapter::Raise("QML_Tester: cannot write '{}'", path);
                        f.write(data);
                    } catch (std::exception& e) {
                        std::cerr << "# --gui-record: save error: " << e.what() << std::endl;
                    }
                };
                QObject::connect(inst, &radapter::Instance::ShutdownRequest, saveRec);
                QObject::connect(qApp, &QCoreApplication::aboutToQuit, saveRec);
            }
        }

        if (config->cli["tags"] == true) {
            inst->EnableTags();
        }

        if (config->cli.is_used("schema")) {
            QStringList only;
            for (auto& n: config->cli.get<std::vector<std::string>>("schema")) {
                only << QString::fromStdString(n);
            }
            std::cerr << QJsonDocument::fromVariant(inst->GetSchemas(only)).toJson().toStdString() << std::endl;
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

        // --gui-replay: once the event loop is running and windows exist, replay the
        // recorded events then exit. The single-shot fires after eval, inside the loop.
        if constexpr (radapter::GUI) {
            if (auto replayPath = config->cli.present<std::string>("gui-replay")) {
                auto path = QString::fromStdString(*replayPath);
                QTimer::singleShot(1000, qApp, [this, path] {
                    try {
                        radapter::gui::ReplayFile(path, 1.0);
                    } catch (std::exception& e) {
                        std::cerr << "# --gui-replay error: " << e.what() << std::endl;
                    }
                    inst->Shutdown();
                });
            }
        }

        if (config->listener) {
            QObject::connect(config->listener, &Listener::FileChanged, this, [this] {
                if (g_reloading)
                    return;
                g_reloading = true;
                if (!reload())
                    g_reloading = false;
            });
        }
    }

    void shutdown() {
        inst->Shutdown(sigterm ? 500 : 5000);
        return;
    }

    bool reload() {
        if (auto cmd = config->cli.present("pre-reload")) {
            std::cerr << "# Pre-reload: " << *cmd << std::endl;
            if (int rc = std::system(cmd->c_str())) {
                std::cerr << "# Pre-reload command failed (exit " << rc
                          << "), keeping current instance" << std::endl;
                return false;
            }
        }
        bool execMode = config->cli["reload-exec"] == true;
        std::cerr << (execMode ? "# Restart..." : "# Hot reload...") << std::endl;
        QObject::connect(inst, &QObject::destroyed, [config = config, execMode]{
            if (execMode)
                reexec();   // on success the image is replaced and never returns
            try {
                new AppState{config};
                config->listener->elapsed.restart();
                std::cerr << "# Hot reload done." << std::endl;
            } catch (std::exception& e) {
                std::cerr << "# Hot reload error: " << e.what() << std::endl;
                qApp->exit(1);
            }
            // release the guard only after queued build side-effect events drain
            QTimer::singleShot(0, qApp, []{ g_reloading = false; });
        });
        inst->Shutdown();
        return true;
    }

    ~AppState() {}
};

int main (int argc, char **argv) try {
    g_argv.assign(argv, argv + argc);
    std::unique_ptr<QCoreApplication> app;
    bool shutdownOnLastClosed = false;
#ifdef RADAPTER_GUI
    for (auto it = argv; it != argv + argc; ++it) {
        if (strncmp(*it, "--gui", 5) == 0) {
            auto gapp = new QApplication(argc, argv);
            gapp->setQuitOnLastWindowClosed(false);
            app.reset(gapp);
            break;
        }
    }
#endif
    if (!app) {
        app.reset(new QCoreApplication(argc, argv));
    }
    // QtQuick.Dialogs' fallback file dialog persists its state via QSettings, which warns
    // unless the application identity is set — give it one so the GUI runs noise-free
    QCoreApplication::setOrganizationName("radapter");
    QCoreApplication::setOrganizationDomain("radapter.local");
    QCoreApplication::setApplicationName("radapter");
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
            .help("Enable native gui mode (quits when the last GUI window is closed)");
        cli.add_argument("--gui-no-auto-quit")
            .flag()
            .help("Like --gui, but keep running after the last GUI window is closed");
        cli.add_argument("--gui-record")
            .help("Implies --gui. Record all GUI interactions to a JSON file for later replay");
        cli.add_argument("--gui-replay")
            .help("Implies --gui. Replay a recorded JSON file of GUI interactions, then exit");
    }
    cli.add_argument("--watch-dir", "-w")
        .append()
        .store_into(watch_dirs)
        .help("Reload on modified files in <dir>");
    cli.add_argument("--pre-reload")
        .help("Shell command to run before each hot reload (e.g. a rebuild); "
              "the reload is skipped if it exits non-zero");
    cli.add_argument("--reload-exec", "-r")
        .flag()
        .help("On hot reload, re-exec the (rebuilt) binary instead of rebuilding "
              "the instance in-process (POSIX only); picks up changes baked into "
              "the executable such as embedded QML/scripts");
    cli.add_argument("--schema")
        .nargs(argparse::nargs_pattern::any)
        .default_value(std::vector<std::string>{})
        .help("Print config schema (optionally only for the given worker names)");
    cli.add_argument("--tags")
        .flag()
        .help("Enable the tag registry (tags.subscribe/get/source/changed)");
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
#ifdef RADAPTER_GUI
        // auto-quit on last window close is the default for any --gui* mode;
        // --gui-no-auto-quit opts out
        if (cli["gui-no-auto-quit"] != true) {
            if (auto* guiApp = qobject_cast<QGuiApplication*>(app.get())) {
                shutdownOnLastClosed = true;
            }
        }
#endif

    auto sigs = QCtrlSignalHandler::instance();
    sigs->registerForSignal(QCtrlSignalHandler::SigInt);
    sigs->registerForSignal(QCtrlSignalHandler::SigTerm);

    std::optional<Listener> listener;
    std::optional<FileWatcher> watcher;

    if (watch_dirs.size()) {
        watcher.emplace();
        listener.emplace();
        for (auto& dir: watch_dirs) {
            auto id = watcher->addWatch(dir, &*listener, true);
            if (id < 0) {
                std::cerr << "# WARN: could not watch: " << dir << ": " << Errors::Log::getLastErrorLog() << std::endl;
            } else {
                std::cerr << "# Watching directory: " << dir << std::endl;
            }
        }
    }

    AppConfig cfg{cli, lua_args, exprs, sigs, listener ? &*listener : nullptr, shutdownOnLastClosed};

    new AppState{&cfg};

    QObject::connect(qApp, &QCoreApplication::aboutToQuit, []{
        std::cerr << "# About to quit" << std::endl;
    });

    if (watcher) {
        watcher->watch();
    }

    return g_exited ? 0 : app->exec();
} catch (std::exception& exc) {
    std::cerr << "Critical: " << exc.what() << std::endl;
    return 1;
}

#include "main.moc"
