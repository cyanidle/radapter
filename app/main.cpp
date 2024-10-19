#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <qctrlsignalhandler.h>
#include <qjsondocument.h>
#include "radapter.hpp"
#include "argparse/argparse.hpp"

int main (int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    radapter::Instance inst;
    std::vector<std::string> args;
    for (auto a: app.arguments()) {
        args.push_back(a.toStdString());
    }

    QVariantMap luaArgs;
    std::vector<std::string> exprs;
    std::vector<std::string> files;
    auto ver = fmt::format(
        "{}.{}{} ({}+{})",
        radapter::VerMajor,
        radapter::VerMinor,
        radapter::JIT ? "-jit" : "",
        radapter::BuildId,
        radapter::BuildDate);
    argparse::ArgumentParser cli(argv[0], ver);
    cli.add_argument("file")
        .nargs(argparse::nargs_pattern::any)
        .store_into(files)
        .help("Execute files");
    cli.add_argument("-a", "--arg")
        .append()
        .action([&](std::string_view kv){
            auto pos = kv.find_first_of('=');
            if (pos == std::string_view::npos || pos == kv.size() - 1) {
                throw radapter::Err("Invalid --arg format ({}) => should be key=val", kv);
            }
            auto k = kv.substr(0, pos);
            auto v = kv.substr(pos + 1);
            luaArgs[QString::fromUtf8(k.data(), int(k.size()))] = QString::fromUtf8(v.data(), int(v.size()));
        })
        .help("Add keys to global 'args' with syntax -a key=value");
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
    cli.add_argument("--debug-host")
        .default_value("*")
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

    if (cli["schema"] == true) {
        std::cerr << QJsonDocument::fromVariant(inst.GetSchemas()).toJson().toStdString() << std::endl;
        return 0;
    }

    auto sigs = QCtrlSignalHandler::instance();
    sigs->registerForSignal(QCtrlSignalHandler::SigInt);
    sigs->registerForSignal(QCtrlSignalHandler::SigTerm);
    app.connect(sigs, &QCtrlSignalHandler::ctrlSignal, &inst, &radapter::Instance::Shutdown);
    app.connect(&inst, &radapter::Instance::HasShutdown, &app, &QCoreApplication::quit);

    inst.RegisterGlobal("args", luaArgs);

    for (auto& e: exprs) {
        inst.Eval(e);
    }
    for (auto& f: files) {
        inst.EvalFile(f);
    }

    if (cli["debug"] == true) {
        inst.DebuggerListen(cli.get("debug-host"), cli.get<uint16_t>("debug-port"));
    }

    return app.exec();
} catch (std::exception& exc) {
    std::cerr << "Critical: " << exc.what() << std::endl;
    return 1;
}
