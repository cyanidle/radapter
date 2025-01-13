#ifndef RADAPTER_LOGS_HPP
#define RADAPTER_LOGS_HPP

#include <fmt/core.h>
#include <QString>
#include "describe/describe.hpp"

namespace radapter {

#define _RADAPTER_ENUM(cls, name, ...) \
enum cls name {}

#define RADAPTER_ENUM_CLASS(name, ...) _RADAPTER_ENUM(class, name, ##__VA_ARGS__)
#define RADAPTER_ENUM(name, ...) _RADAPTER_ENUM(, name, ##__VA_ARGS__)

enum LogLevel {
    debug = 0,
    info,
    warn,
    error,
    disabled,
};

DESCRIBE("radapter::LogLevel", LogLevel, void) {
    MEMBER("D", debug);
    MEMBER("I", info);
    MEMBER("W", warn);
    MEMBER("E", error);
}

namespace detail {
std::runtime_error RADAPTER_API doErr(fmt::string_view fmt, fmt::format_args args);
}

template<typename...Args>
std::runtime_error Err(fmt::format_string<Args...> fmt, Args&&...a) {
    return detail::doErr(fmt, fmt::make_format_args(a...));
}

}

template<> struct fmt::formatter<QString> : fmt::formatter<const char*>  {
    template<typename Ctx>
    auto format(QString const& s, Ctx& ctx) const {
        return fmt::formatter<const char*>::format(qUtf8Printable(s), ctx);
    }
};

#endif //RADAPTER_LOGS_HPP
