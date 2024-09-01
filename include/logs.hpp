#ifndef RADAPTER_LOGS_HPP
#define RADAPTER_LOGS_HPP

#include <fmt/core.h>
#include <QString>
#include "describe/describe.hpp"

namespace radapter {

enum LogLevel {
    debug = 0,
    info,
    warn,
    error,
    disabled,
};

DESCRIBE(radapter::LogLevel, debug, info, warn, error, disabled)

namespace detail {
std::runtime_error doErr(fmt::string_view fmt, fmt::format_args args);
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
