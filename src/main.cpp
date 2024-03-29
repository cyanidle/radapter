#include <argparse/argparse.hpp>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <set>
#include "common.hpp"
#include "logs.hpp"
#include "lua.hpp"
#include "compiled_bootstrap.hpp"
#include "redis/client.hpp"

using namespace radapter;

static int panic(lua_State* L) {
    logErr("LUA: Panic: {}", lua::ToStringWithConv(L, 1));
    std::exit(1);
}

static int traceFunc(lua_State* L) {
    auto str = lua::ToStringWithConv(L, 1);
    // todo: stack trace
    lua_pushlstring(L, str.data(), str.size());
    return 1;
}

static std::set<QTimer*> timers;

static int setTimeout(lua_State* L) {
    auto millis = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto ref = lua::Ref(L, 2);
    auto tmr = new QTimer();
    tmr->callOnTimeout([ref, L]{
        lua::TracerFunc.push();
        ref.push();
        lua_pcall(L, 0, LUA_MULTRET, -2);
    });
    tmr->start(millis);
    timers.insert(tmr);
    lua_pushlightuserdata(L, tmr);
    return 1;
}

static int clearTimeout(lua_State* L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    auto tmr = static_cast<QTimer*>(lua_touserdata(L, 1));
    if (auto it = timers.find(tmr); it == timers.end()) {
        throw Err("attempt to delete invalid timer");
    } else {
        timers.erase(it);
        delete tmr;
    }
    return 0;
}

static int parseJson(lua_State* L) {
    luaL_checkany(L, 1);
    size_t sz;
    auto str = luaL_checklstring(L, 1, &sz);
    try {
        lua::ParseJson(L, {str, sz});
    } catch (std::exception& exc) {
        luaL_error(L, "parse error: %s", exc.what());
    }
    return 1;
}

static int dumpJson(lua_State* L) {
    luaL_checkany(L, 1);
    auto n = lua_gettop(L);
    bool pretty = false;
    if (n >= 2) {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        pretty = lua_toboolean(L, 2);
    }
    try {
        lua::DumpJson(L, 1, pretty);
    } catch (std::exception& exc) {
        luaL_error(L, "dump error: %s", exc.what());
    }
    return 1;
}

static luaL_Reg builtins[] = {
    {"each", lua::Protected<setTimeout>},
    {"stop", lua::Protected<clearTimeout>},
    {"parse", parseJson},
    {"dump", dumpJson},
    {"printStack", lua::DumpStack},
    {NULL, NULL}
};

static void runBuffer(lua_State* L, string_view code, const char* name, const char* mode) {
    lua::TracerFunc.push();
    if (auto err = luaL_loadbufferx(L, code.data(), code.size(), name, mode)) {
        throw Err("while loading {}: {}", name, lua::printErr(err));
    }
    if (auto err = lua_pcall(L, 0, LUA_MULTRET, -2)) {
        throw Err("while running {}: {} => {}", name, lua::printErr(err), lua::ToString(L, -1));
    }
    lua_pop(L, 1);
}

static void interactive(lua_State* L) {
    auto thr = QThread::currentThread();
    while (!thr->isInterruptionRequested()) {
        std::string line;
        std::getline(std::cin, line);
        QMetaObject::invokeMethod(qApp, [L, line=std::move(line)]{
            try {
                runBuffer(L, line, "<command line>", "t");
            } catch (std::exception& e) {
                logErr("Error: {}", e.what());
            }
        },
        Qt::QueuedConnection);
    }
}

struct Lol {
    int a;
    int b;
};
DESCRIBE(Lol, &_::a, &_::b)

int main(int argc, char *argv[]) try
{
    QCoreApplication app(argc, argv);
    argparse::ArgumentParser cli(argv[0], "0.0.0");
    auto L = luaL_newstate();
    meta::defer close([=]{
        lua_close(L);
    });
    luaL_openlibs(L);
    lua_atpanic(L, panic);
    lua_pushcfunction(L, traceFunc);
    lua::TracerFunc = {L, -1};
    lua_pop(L, 1);
    logs::Register(L);
    lua_pushglobaltable(L);
    luaL_setfuncs(L, builtins, 0);
    lua_pop(L, 1);
    auto boot = compiled_bootstrap();
    runBuffer(L, boot, "<bootstrap>", "bt");
    MakeClass<redis::Client>(L);
    cli.add_argument("run")
        .action([&](const string& file){
            if (auto err = luaL_dofile(L, file.c_str())) {
                logErr("while running {}: \n\t{} => {}", file, lua::printErr(err), lua::ToString(L, -1));
                std::exit(1);
            }
        })
        .nargs(argparse::nargs_pattern::any)
        .help("main file to launch");
    cli.add_argument("--eval", "-e")
        .action([&](string script){
            luaL_loadstring(L, script.c_str());
        })
        .help("execute lua from cli");
    cli.add_argument("--interactive", "-i")
        .implicit_value(true)
        .default_value(false)
        .help("interactive (cli) mode");
    try {
        cli.parse_known_args(argc, argv);
    } catch (std::exception& exc) {
        logErr(exc.what());
        std::cerr << cli;
        return 1;
    }
    if (cli["i"] == true) {
        QThread::create([L]{interactive(L);})->start();
    }
    return app.exec();
} catch (std::exception& exc) {
    logErr("Critical: {}", exc.what());
    return 1;
}
