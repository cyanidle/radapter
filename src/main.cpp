#include <argparse/argparse.hpp>
#include <QCoreApplication>
#include <QTimer>
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

static void runPrecompiled(lua_State* L, const void* data, size_t sz) {
    auto src = reinterpret_cast<const char*>(data);
    if (auto err = luaL_loadbufferx(L, src, sz, "bootstrap", "b")) {
        throw Err("while loading bootstrap: {}", lua::printErr(err));
    }
    if (auto err = lua_pcall(L, 0, LUA_MULTRET, 0)) {
        throw Err("while running bootstrap: {} => {}", lua::printErr(err), lua::ToString(L, -1));
    }
}

static int traceFunc(lua_State* L) {
    auto str = lua::ToStringWithConv(L, 1);
    // todo: stack trace
    lua_pushlstring(L, str.data(), str.size());
    return 1;
}

static int tracerRef = 0;

static int setTimeout(lua_State* L) {
    auto millis = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto tmr = new QTimer();
    tmr->callOnTimeout([ref, L]{
        lua_rawgeti(L, LUA_REGISTRYINDEX, tracerRef);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_pcall(L, 0, LUA_MULTRET, -2);
    });
    QObject::connect(tmr, &QObject::destroyed, [ref, L]{
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    });
    tmr->start(millis);
    lua_pushlightuserdata(L, tmr);
    return 1;
}

static int clearTimeout(lua_State* L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    delete static_cast<QTimer*>(lua_touserdata(L, 1));
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
    {"setTimeout", lua::Protected<setTimeout>},
    {"clearTimeout", lua::Protected<clearTimeout>},
    {"parseJson", parseJson},
    {"dumpJson", dumpJson},
    {"logStack", lua::DumpStack},
    {NULL, NULL}
};

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
    lua_pushcfunction(L, traceFunc);
    tracerRef = luaL_ref(L, LUA_REGISTRYINDEX);
    logs::Register(L);
    lua_pushglobaltable(L);
    luaL_setfuncs(L, builtins, 0);
    runPrecompiled(L, gbootstrapData, gbootstrapSize);
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
