#include "logs.hpp"
#include "fmt/chrono.h"
#include "fmt/args.h"
#include <chrono>
#include <fmt/color.h>
#include <fmt/compile.h>
#include "common.hpp"
#include "lua.hpp"
#include <array>

using namespace radapter;

using LevelPair = std::pair<string_view, fmt::color>;

static constexpr std::array levels = {
    LevelPair{"dbg", fmt::color::blue},
    LevelPair{"info",  fmt::color::green},
    LevelPair{"warn",  fmt::color::orange},
    LevelPair{"err", fmt::color::red},
};

void radapter::logs::DoLog(Level lvl, fmt::string_view format, fmt::format_args args)
{
    using namespace std::chrono;
    using clock = std::chrono::system_clock;
    if (int(lvl) < 0 || int(lvl) > 3) {
        throw Err("invalid log level");
    }
    auto& pair = levels[int(lvl)];
    auto now = clock::now();
    fmt::print(
        stderr,
        FMT_COMPILE("{:%m-%d}T{:%H:%M:%S}|{:<4}| {}\n"),
        fmt::localtime(clock::to_time_t(now)),
        duration_cast<milliseconds>(now.time_since_epoch()),
        fmt::styled(pair.first, fmt::fg(pair.second)),
        fmt::vformat(format, args)
    );
}

static int logImpl(lua_State* L) {
    auto lvl = logs::Level(lua_tointeger(L, lua_upvalueindex(1)));
    auto n = lua_gettop(L);
    if (n == 0) {
        throw Err("at least 1 argument expected");
    }
    fmt::dynamic_format_arg_store<fmt::format_context> args;
    string_view fmt;
    if (lua_type(L, 1) == LUA_TSTRING) {
        fmt = lua::ToString(L, 1);
    } else {
        fmt = "{}";
        lua_pushvalue(L, 1);
    }
    args.reserve(n, 0);
    for (auto idx = 2; idx < n + 2; ++idx) {
        switch (lua_type(L, idx)) {
        case LUA_TBOOLEAN: {
            args.push_back(bool(lua_toboolean(L, idx)));
            break;
        }
        case LUA_TSTRING: {
            args.push_back(lua::LogString{L, idx});
            break;
        }
        case LUA_TNIL: {
            args.push_back("null");
            break;
        }
        case LUA_TNUMBER: {
            if (lua_isinteger(L, idx)) {
                args.push_back(lua_tointeger(L, idx));
            } else {
                args.push_back(lua_tonumber(L, idx));
            }
            break;
        }
        case LUA_TTABLE: {
            args.push_back(lua::LogTable{L, idx});
            break;
        }
        default: {
            args.push_back(lua::ToStringWithConv(L, idx));
            break;
        }
        }
    }
    logs::DoLog(lvl, fmt, args);
    return 0;
}

static void pushLogger(lua_State* L, logs::Level lvl, string_view name) {
    lua_pushlstring(L, name.data(), name.size());
    lua_pushinteger(L, int(lvl));
    lua_pushcclosure(L, lua::Protected<logImpl>, 1);
    lua_rawset(L, -3);
}

void logs::Register(lua_State *L)
{
    lua_createtable(L, 0, 4);
    pushLogger(L, Level::Debug, "debug");
    pushLogger(L, Level::Info, "info");
    pushLogger(L, Level::Warn, "warn");
    pushLogger(L, Level::Error, "error");
    lua_setglobal(L, "log");
}
