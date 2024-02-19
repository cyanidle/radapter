#pragma once

#include "common.hpp"
#include "fmt/core.h"

namespace radapter::logs
{

enum class Level : uint {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
};

void DoLog(Level lvl, fmt::string_view format, fmt::format_args args);

template<typename ...T>
void PrepareLog(Level lvl, fmt::format_string<T...> fmt, T&&...args) {
    DoLog(lvl, fmt, fmt::make_format_args(args...));
}

template<typename T>
void PrepareLog(Level lvl, const T &arg) {
    PrepareLog(lvl, "{}", arg);
}

void Register(lua_State* L);

}

namespace radapter {

template<typename...Args>
std::runtime_error Err(fmt::format_string<Args...> fmt, const Args&... args) {
    return std::runtime_error{fmt::format(fmt, args...)};
}

}

#define _IMPL_DO_LOG(lvl, ...) ::radapter::logs::PrepareLog(lvl, __VA_ARGS__)
#define logDebug(...) _IMPL_DO_LOG(::radapter::logs::Level::Debug, __VA_ARGS__)
#define logInfo(...) _IMPL_DO_LOG(::radapter::logs::Level::Info, __VA_ARGS__)
#define logWarn(...) _IMPL_DO_LOG(::radapter::logs::Level::Warn, __VA_ARGS__)
#define logErr(...)  _IMPL_DO_LOG(::radapter::logs::Level::Error, __VA_ARGS__)

template<> struct fmt::formatter<jv::JsonView> {
    template<typename ParseContext>
    auto parse(ParseContext& ctx) {
        auto fmt_end = std::find(ctx.begin(), ctx.end(), '}');
        if (fmt_end != ctx.begin()) {
            char repr = *ctx.begin();
            if (repr == 'p') {
                pretty = true;
            } else if (repr == 'c') {
                pretty = false;
            } else {
                throw radapter::Err("invalid repr for json: {}", repr);
            }
            ctx.advance_to(std::next(ctx.begin()));
        }
        return ctx.begin();
    }
    template<typename FormatContext>
    auto format(jv::JsonView j, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", j.Dump(pretty));
    }
    bool pretty = false;
};

template<> struct fmt::formatter<jv::Json> : fmt::formatter<jv::JsonView> {
    template<typename FormatContext>
    auto format(jv::Json const& j, FormatContext& ctx) const {
        return fmt::formatter<jv::JsonView>::format(j, ctx);
    }
};
