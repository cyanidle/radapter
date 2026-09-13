#include "pretty.hpp"

#include <fmt/color.h>

#include <QDir>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace radapter {
namespace {

using fmt::emphasis;
using fmt::terminal_color;

constexpr fmt::text_style kMeta = emphasis::faint;
constexpr fmt::text_style kLabel = emphasis::bold | fmt::fg(terminal_color::red);
constexpr fmt::text_style kMessage = emphasis::bold | fmt::fg(terminal_color::red);
constexpr fmt::text_style kMarker = emphasis::faint;
constexpr fmt::text_style kErrorLoc = emphasis::bold | fmt::fg(terminal_color::cyan);
constexpr fmt::text_style kFrameLoc = fmt::fg(terminal_color::cyan);
constexpr fmt::text_style kCFrame = emphasis::faint | fmt::fg(terminal_color::yellow);

constexpr std::string_view kMarkerText = "stack traceback:";
constexpr std::string_view kHeaderIndent = "  ";
constexpr std::string_view kFrameIndent = "    ";

bool stderrIsTty()
{
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

std::string_view trimFront(std::string_view s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) ++i;
    return s.substr(i);
}

std::string_view trim(std::string_view s)
{
    s = trimFront(s);
    size_t n = s.size();
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) --n;
    return s.substr(0, n);
}

std::string const& cwd()
{
    // Leaked on purpose: a late log (during teardown) must not touch a destroyed static.
    static std::string const* dir = new std::string(QDir::currentPath().toUtf8().toStdString());
    return *dir;
}

bool isPathBoundary(char c)
{
    return c == '/' || c == '<' || c == '(' || c == '\'' || c == '"' || c == ' ' || c == '=';
}

// Copies `s`, rewriting every absolute path that lives under the cwd as cwd-relative.
void appendShortened(std::string& out, std::string_view s)
{
    auto const& dir = cwd();
    if (dir.size() < 2) {
        out += s;
        return;
    }
    size_t i = 0;
    while (i < s.size()) {
        auto p = s.find(dir, i);
        if (p == std::string_view::npos) {
            out += s.substr(i);
            return;
        }
        auto after = p + dir.size();
        if ((p == 0 || isPathBoundary(s[p - 1])) && after < s.size() && s[after] == '/') {
            out += s.substr(i, p - i);
            i = after + 1;
        } else {
            out += s.substr(i, after - i);
            i = after;
        }
    }
}

// Splits a Lua location prefix "src:line:" off a frame or error line. `loc` keeps the
// trailing colon, `rest` is whatever follows. The scan is digit-anchored so a ':' inside
// the source path (a Windows drive, for instance) does not fool it.
std::pair<std::string_view, std::string_view> splitLocation(std::string_view s)
{
    for (size_t i = 0; i + 2 < s.size(); ++i) {
        if (s[i] != ':') continue;
        size_t j = i + 1;
        while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
        if (j > i + 1 && j < s.size() && s[j] == ':') {
            return { s.substr(0, j + 1), s.substr(j + 1) };
        }
    }
    return { {}, s };
}

class Printer {
public:
    explicit Printer(bool color) : color(color) {}

    void label(std::string_view raw)
    {
        auto text = trim(raw);
        if (text.empty()) return;
        sep();
        out += kHeaderIndent;
        put(text, kLabel);
    }

    void marker(std::string_view raw)
    {
        sep();
        out += kHeaderIndent;
        put(trim(raw), kMarker);
    }

    // `errorLine` marks the message that opened the traceback (shown in red).
    void line(std::string_view raw, std::string_view indent, bool errorLine)
    {
        auto text = trim(raw);
        if (text.empty()) return;
        sep();
        out += indent;

        if (text.rfind("[C]", 0) == 0) {
            auto rest = trimFront(text.substr(3));
            if (!rest.empty() && rest[0] == ':') rest = trimFront(rest.substr(1));
            put("[C]", kCFrame);
            out += ':';
            if (!rest.empty()) {
                out += ' ';
                put(rest, {});
            }
            return;
        }

        auto [loc, rest] = splitLocation(text);
        if (loc.empty()) {
            put(text, {});
            return;
        }
        put(loc.substr(0, loc.size() - 1), errorLine ? kErrorLoc : kFrameLoc);
        out += ':';
        auto tail = trimFront(rest);
        if (!tail.empty()) {
            out += ' ';
            put(tail, errorLine ? kMessage : fmt::text_style{});
        }
    }

    std::string take() { return std::move(out); }

private:
    // Appends `text` styled, shortening cwd-absolute paths on the way.
    void put(std::string_view text, fmt::text_style style)
    {
        if (!color) {
            appendShortened(out, text);
            return;
        }
        std::string tmp;
        appendShortened(tmp, text);
        fmt::format_to(std::back_inserter(out), style, "{}", tmp);
    }

    void sep()
    {
        if (!first) out += '\n';
        first = false;
    }

    std::string out;
    bool color;
    bool first = true;
};

}

bool ColorizeLogs()
{
    static bool const enabled = [] {
        if (auto* force = std::getenv("CLICOLOR_FORCE"); force && *force && std::strcmp(force, "0") != 0) {
            return true;
        }
        if (auto* no = std::getenv("NO_COLOR"); no && *no) {
            return false;
        }
        return stderrIsTty();
    }();
    return enabled;
}

fmt::text_style MetadataStyle(bool color)
{
    return color ? kMeta : fmt::text_style{};
}

fmt::text_style LevelStyle(LogLevel lvl, bool color)
{
    if (!color) return {};
    switch (lvl) {
    case debug: return emphasis::faint;
    case info: return fmt::fg(terminal_color::green);
    case warn: return fmt::fg(terminal_color::yellow);
    case error: return emphasis::bold | fmt::fg(terminal_color::red);
    case disabled: break;
    }
    return {};
}

std::optional<std::string> PrettyTraceback(std::string_view msg, bool color)
{
    std::vector<std::string_view> lines;
    for (size_t pos = 0;;) {
        auto nl = msg.find('\n', pos);
        lines.push_back(msg.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos));
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }

    auto marker = lines.size();
    for (size_t i = 0; i < lines.size(); ++i) {
        if (trim(lines[i]) == kMarkerText) {
            marker = i;
            break;
        }
    }
    if (marker == lines.size()) {
        return std::nullopt;
    }

    Printer p{color};
    for (size_t i = 0; i < marker; ++i) {
        if (i == 0 && splitLocation(trim(lines[i])).first.empty()) {
            p.label(lines[i]);
        } else {
            p.line(lines[i], kHeaderIndent, true);
        }
    }
    p.marker(lines[marker]);
    for (size_t i = marker + 1; i < lines.size(); ++i) {
        p.line(lines[i], kFrameIndent, false);
    }
    auto out = p.take();
    out.shrink_to_fit();
    return out;
}

}
