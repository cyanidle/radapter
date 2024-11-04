#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <QJsonDocument>
#include <qctrlsignalhandler.h>
#include "radapter.hpp"
#include "argparse/argparse.hpp"

int main (int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    radapter::Instance inst;
    std::vector<std::string> args;
    for (auto a: app.arguments()) {
        args.push_back(a.toStdString());
    }

    std::vector<std::string> exprs;
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
    cli.add_argument("--schema")
        .flag()
        .help("Print config schema");
    cli.add_argument("--debug")
        .flag()
        .help("Enable debugger");
    cli.add_argument("--debug-vscode")
        .flag()
        .help("Debugger vscode implementation (json-based)");
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
        std::cerr << "Invalid args: ";
        for (auto& a: args) {
            std::cerr << a << " | ";
        }
        std::cerr << std::endl;
        std::cerr << cli << std::endl;
        return 1;
    }

    if (cli["schema"] == true) {
        std::cerr << QJsonDocument::fromVariant(inst.GetSchemas()).toJson().toStdString() << std::endl;
        return 0;
    }

    auto sigs = QCtrlSignalHandler::instance();
    sigs->registerForSignal(QCtrlSignalHandler::SigInt);
    sigs->registerForSignal(QCtrlSignalHandler::SigTerm);
    app.connect(sigs, &QCtrlSignalHandler::ctrlSignal, &inst, &radapter::Instance::Shutdown);
    app.connect(&inst, &radapter::Instance::HasShutdown, &app, &QCoreApplication::quit);

    inst.RegisterGlobal("args", lua_args);

    for (auto& e: exprs) {
        inst.Eval(e);
    }

    bool debug = cli["debug"] == true;
    bool debugVscode = cli["debug-vscode"] == true;

    if (debug || debugVscode) {
        radapter::Instance::DebuggerOpts opts;
        opts.host = cli.get("debug-host");
        opts.port = cli.get<uint16_t>("debug-port");
        opts.vscode = debugVscode;
        inst.DebuggerConnect(opts);
    }

    if (auto f = cli.present("file")) {
        inst.EvalFile(radapter::fs::u8path(*f));
    }

    return app.exec();
} catch (std::exception& exc) {
    std::cerr << "Critical: " << exc.what() << std::endl;
    return 1;
}
