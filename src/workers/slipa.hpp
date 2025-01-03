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

using CannotFail = std::false_type;

struct Encoder {
    Encoder() noexcept = default;
    std::string_view Encode(char byte) {
        switch (byte) {
        case ESC:
            buff[0] = ESC;
            buff[1] = ESC_ESC;
            buff[2] = 0;
            return {buff, 2};
        case END:
            buff[0] = ESC;
            buff[1] = ESC_END;
            buff[2] = 0;
            return {buff, 2};
        default:
            buff[0] = byte;
            buff[1] = 0;
            return {buff, 1};
        }
    }
protected:
    char buff[4] = {};
};

template<typename Fn>
constexpr void Write(std::string_view msg, Fn&& out) noexcept {
    constexpr char esc_end[] = {ESC, ESC_END, 0};
    constexpr char esc_esc[] = {ESC, ESC_ESC, 0};
    size_t offs = 0;
    size_t last = 0;
    auto commit = [&]{
        out(msg.substr(last, offs));
        last += offs;
        offs = 0;
    };
    for (auto ch: msg) {
        switch (ch) {
        case ESC:
            commit();
            last++;
            out(std::string_view{esc_esc});
            break;
        case END:
            commit();
            last++;
            out(std::string_view{esc_end});
            break;
        }
        offs++;
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
        auto next = msg.find_first_of(esc, pos);
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