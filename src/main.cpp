#include <argparse/argparse.hpp>
#include <QCoreApplication>
#include "common.hpp"
#include "logs.hpp"
#include "lua.hpp"
#include "node.hpp"
#include "incbin.h"

using namespace radapter;

INCBIN(bootstrap, BOOTSTRAP_PATH);

static int panic(lua_State* L) {
    logErr("LUA: Panic: {}", lua::ToStringEx(L, 1));
    std::exit(1);
}

int main(int argc, char *argv[]) try
{
    QCoreApplication app(argc, argv);
    argparse::ArgumentParser cli(argv[0], "0.0.0");
    auto L = luaL_newstate();
    luaL_openlibs(L);
    lua_atpanic(L, panic);
    logs::Register(L);
    Node::Register(L);
    if (auto err = luaL_loadbufferx(L, reinterpret_cast<const char*>(gbootstrapData), gbootstrapSize, "bootstrap", "b")) {
        throw Err("while loading bootstrap: {}", lua::printErr(err));
    }
    if (auto err = lua_pcall(L, 0, LUA_MULTRET, 0)) {
        throw Err("while running bootstrap: {}", lua::printErr(err));
    }
    defer cleanup([&]{
        lua_close(L);
    });
    logDebug(Json{});
    logDebug("{:p}", Json{});
    cli.add_argument("main")
        .default_value("")
        .help("main file to launch");
    cli.add_argument("--load", "-l")
        .action([&](string file){
            luaL_dofile(L, file.c_str());
        })
        .help("prepare file as enviroment");
    cli.add_argument("--run", "-r")
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
    if (auto main = cli.get("main"); !main.empty()) {
        luaL_dofile(L, main.c_str());
    }
    return app.exec();
} catch (std::exception& exc) {
    logErr("Critical: {}", exc.what());
    return 1;
}
