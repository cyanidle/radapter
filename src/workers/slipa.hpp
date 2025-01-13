#ifndef SLIPA_HPP
#define SLIPA_HPP
#pragma once

#include <type_traits>
#include <string_view>
#include <stdlib.h>
#include <assert.h>

namespace slipa
{

enum Special : char {
    ESC = char(0xDB),
    END = char(0xC0),
    ESC_ESC = char(0xDD),
    ESC_END = char(0xDC),
};

template<typename Fn>
void Write(std::string_view msg, Fn&& out) {
    constexpr char esc_end[] = {ESC, ESC_END, 0};
    constexpr char esc_esc[] = {ESC, ESC_ESC, 0};
    size_t collected = 0;
    auto* in = msg.data();
    auto commit = [&]{
        if (collected) out(std::string_view{in, collected});
        in += collected;
        collected = 0;
    };
    for (char ch: msg) {
        if (ch == ESC) {
            commit();
            in++;
            out(std::string_view{esc_esc, 2});
        } else if (ch == END) {
            commit();
            in++;
            out(std::string_view{esc_end, 2});
        } else {
            collected++;
        }
    }
    commit();
}

enum ReadErrors {
    NoError = 0,
    UnterminatedEscape = 1,
    InvalidEscape = 2,
};

template<typename Fn>
[[nodiscard]]
constexpr ReadErrors Read(std::string_view msg, Fn&& out) {
    constexpr char esc[] = {ESC, 0};
    constexpr char end[] = {END, 0};
    if (msg.size() && msg.back() == END) {
        msg = msg.substr(0, msg.size() - 1);
    }
    size_t pos = 0;
    while (pos < msg.size()) {
        auto next = msg.find_first_of(ESC, pos);
        if (next == std::string_view::npos) {
            out(msg.substr(pos));
            break;
        } else if (next == msg.size() - 1) {
            return UnterminatedEscape;
        } else {
            if (pos != next) {
                out(msg.substr(pos, next - pos));
            }
            auto escaped = msg[next + 1];
            if (escaped == ESC_END) {
                out(std::string_view{end});
            } else if (escaped == ESC_ESC) {
                out(std::string_view{esc});
            } else {
                return InvalidEscape;
            }
            pos = next + 2;
        }
    }
    return NoError;
}


} //slipa

#endif //SLIPA_HPP
