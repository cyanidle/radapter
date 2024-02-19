#include <argparse/argparse.hpp>
#include <QCoreApplication>
#include "common.hpp"
#include "logs.hpp"
#include "lua.hpp"
#include "incbin.h"

using namespace radapter;

INCBIN(bootstrap, BOOTSTRAP_PATH);

static int panic(lua_State* L) {
    logErr("LUA: Panic: {}", lua::ToStringWithConv(L, 1));
    std::exit(1);
}

static void runBootstrap(lua_State* L) {
    auto src = reinterpret_cast<const char*>(gbootstrapData);
    auto sz = gbootstrapSize;
    if (auto err = luaL_loadbufferx(L, src, sz, "bootstrap", "b")) {
        throw Err("while loading bootstrap: {}", lua::printErr(err));
    }
    if (auto err = lua_pcall(L, 0, LUA_MULTRET, 0)) {
        throw Err("while running bootstrap: {}", lua::printErr(err));
    }
}

struct CloseLater {
    lua_State* L;
    ~CloseLater() {lua_close(L);}
};

int main(int argc, char *argv[]) try
{
    QCoreApplication app(argc, argv);
    argparse::ArgumentParser cli(argv[0], "0.0.0");
    auto L = luaL_newstate();
    CloseLater close{L};
    luaL_openlibs(L);
    lua_atpanic(L, panic);
    logs::Register(L);
    runBootstrap(L);
    cli.add_argument("run")
        .action([&](const string& file){
            luaL_dofile(L, file.c_str());
        })
        .nargs(argparse::nargs_pattern::any)
        .help("main file to launch");
    cli.add_argument("--eval", "-e")
        .action([&](string script){
            luaL_loadstring(L, script.c_str());
        })
        .help("execute lua from cli");
    try {
        cli.parse_known_args(argc, argv);
    } catch (std::exception& exc) {
        logErr(exc.what());
        std::cerr << cli;
        return 1;
    }
    return app.exec();
} catch (std::exception& exc) {
    logErr("Critical: {}", exc.what());
    return 1;
}
