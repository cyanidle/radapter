#pragma once
#include "radapter/common.hpp"
#include <radapter/logs.hpp>
#include <QtGlobal>

#include <fmt/color.h>

#include <optional>
#include <string>
#include <string_view>

namespace radapter {

// Whether log output should carry ANSI escapes: CLICOLOR_FORCE wins, then NO_COLOR
// disables, otherwise a terminal on stderr is required. Resolved once per process.
RADAPTER_API bool ColorizeLogs();

// Style for the metadata columns (timestamp, category). Empty when colors are off.
RADAPTER_API fmt::text_style MetadataStyle(bool color);

// Style for the one-letter level tag, tinted by severity. Empty when colors are off.
RADAPTER_API fmt::text_style LevelStyle(LogLevel lvl, bool color);

// If `msg` carries a Lua stack traceback, lays it out for a human: source paths are
// shortened relative to the cwd, frames get a consistent indent and (when `color`)
// locations are highlighted. Returns nullopt for messages without a traceback.
RADAPTER_API std::optional<std::string> PrettyTraceback(std::string_view msg, bool color);

}
