#include <argparse/argparse.hpp>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QSemaphore>
#include <set>
#include "common.hpp"
#include "logs.hpp"
#include "lua.hpp"
#include "redis/client.hpp"
#include "compiled_bootstrap.hpp"
#include "compiled_mobdebug.hpp"
#include "compiled_socket.hpp"

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

template<bool singleShot>
static int setInterval(lua_State* L) {
    auto millis = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto ref = lua::Ref(L, 2);
    auto tmr = new QTimer();
    tmr->callOnTimeout([ref]{
        ref.push();
        lua::PCall(ref.L);
    });
    tmr->setSingleShot(singleShot);
    tmr->start(millis);
    timers.insert(tmr);
    lua_pushlightuserdata(L, tmr);
    return 1;
}

static int clearInterval(lua_State* L) {
    if (lua_isnil(L, 1)) {
        return 0;
    }
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
    {"setTimeout", lua::Protected<setInterval<true>>},
    {"clearTimeout", lua::Protected<clearInterval>},
    {"setInterval", lua::Protected<setInterval<false>>},
    {"clearInterval", lua::Protected<clearInterval>},
    {"parse", parseJson},
    {"dump", dumpJson},
    {"printStack", lua::DumpStack},
    {NULL, NULL}
};

static void runBuffer(lua_State* L, string_view code, const char* name, const char* mode) {
    if (auto err = luaL_loadbufferx(L, code.data(), code.size(), name, mode)) {
        throw Err("while loading {}: {}", name, lua::PrintErr(err));
    }
    if (auto err = lua::PCall(L)) {
        throw Err("while running {}: {} => {}", name, lua::PrintErr(err), lua::ToString(L, -1));
    }
}

static void interactive(lua_State* L) {
    auto thr = QThread::currentThread();
    auto sem = std::make_shared<QSemaphore>(1);
    while (!thr->isInterruptionRequested()) {
        sem->acquire();
        std::cout << "> ";
        std::string line;
        std::getline(std::cin, line);
        QMetaObject::invokeMethod(qApp, [sem, L, line=std::move(line)]{
            try {
                runBuffer(L, line, "<command line>", "t");
            } catch (std::exception& e) {
                logErr("{}", e.what());
            }
            sem->release();
        },
        Qt::QueuedConnection);
    }
}

template<typename T>
void MakeAndSet(lua_State* L, string_view ns, string_view name) {
    constexpr auto desc = describe::Get<T>();
    lua_createtable(L, 0, 1);
    Serialize(L, name);
    MakeClass<T>(L);
    lua_rawset(L, -3);
    lua_setglobal(L, string{ns}.c_str());
}

int main(int argc, char *argv[]) try
{
    QCoreApplication app(argc, argv);
    argparse::ArgumentParser cli(argv[0], "0.0.0");
    auto L = luaL_newstate();
    meta::defer close([=]{
        lua_close(L);
    });
    std::vector<string> files;
    std::vector<string> evals;
    cli.add_argument("run")
        .action([&](auto f){
            files.push_back(f);
        })
        .nargs(argparse::nargs_pattern::any)
        .help("files to launch");
    cli.add_argument("--eval", "-e")
        .action([&](string script){
            evals.push_back(script);
        })
        .help("execute lua from cli");
    cli.add_argument("--interactive", "-i")
        .flag()
        .help("interactive (cli) mode");
    cli.add_argument("--debug", "-d")
        .flag()
        .help("enable (debug) mode");
    cli.add_argument("--debug-host")
        .default_value("localhost")
        .help("debug listener host");
    cli.add_argument("--debug-port")
        .scan<'u', uint16_t>()
        .default_value(uint16_t(8172))
        .help("debug listener port");
    try {
        cli.parse_known_args(argc, argv);
    } catch (std::exception& exc) {
        logErr(exc.what());
        std::cerr << cli;
        return 1;
    }
    lua_createtable(L, 0, 0);
    auto isDebug = cli["d"] == true;
    if (isDebug) {
        Serialize(L, "debug_enabled");
        Serialize(L, true);
        lua_rawset(L, -3);
    }
    Serialize(L, "debug_port");
    Serialize(L, cli.get<uint16_t>("debug-port"));
    lua_rawset(L, -3);
    Serialize(L, "debug_host");
    Serialize(L, cli.get("debug-host"));
    lua_rawset(L, -3);
    lua_setglobal(L, "radapter");
    luaL_openlibs(L);
    lua_atpanic(L, panic);
    lua_pushcfunction(L, traceFunc);
    lua::SetTracer(L, -1);
    lua_pop(L, 1);
    logs::Register(L);
    lua_pushglobaltable(L);
    luaL_setfuncs(L, builtins, 0);
    if (isDebug) {
        runBuffer(L, compiled_socket(), "<socket>", "bt");
        lua_setglobal(L, "socket");
        runBuffer(L, compiled_mobdebug(), "<mobdebug>", "bt");
        lua_setglobal(L, "mobdebug");
    }
    runBuffer(L, compiled_bootstrap(), "<bootstrap>", "bt");
    lua_pop(L, 1);
    MakeAndSet<redis::Client>(L, "redis", "Client");
    for (auto& e: evals) {
        luaL_dostring(L, e.c_str());
    }
    evals = {};
    for (auto& f: files) {
        luaL_dofile(L, f.c_str());
    }
    files = {};
    if (cli["i"] == true) {
        QThread::create([L]{interactive(L);})->start();
    }
    return app.exec();
} catch (std::exception& exc) {
    logErr("Critical: {}", exc.what());
    return 1;
}
