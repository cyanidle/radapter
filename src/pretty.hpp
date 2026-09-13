#pragma once
#include <optional>
#include <string>
#include <string_view>
#include "radapter/common.hpp"
#include <QtGlobal>

namespace radapter {

// Whether stderr should carry ANSI escapes: CLICOLOR_FORCE wins, then NO_COLOR
// disables, otherwise a terminal is required. Resolved once per process.
RADAPTER_API bool ColorizeLogs();

// If `msg` carries a Lua stack traceback, lays it out for a human: source paths are
// shortened relative to the cwd, frames get a consistent indent and (when `color`)
// locations are highlighted. Returns nullopt for messages without a traceback.
RADAPTER_API std::optional<std::string> PrettyTraceback(std::string_view msg, bool color);

}
