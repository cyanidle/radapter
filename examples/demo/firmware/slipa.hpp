#pragma once
#include "msg2struct.hpp"

namespace slipa
{

enum Special : char {
    ESC = char(0xDB),
    END = char(0xC0),
    ESC_ESC = char(0xDD),
    ESC_END = char(0xDC),
};


template<typename Fn>
_FLATTEN void Write(msg2struct::String msg, Fn&& out) {
    constexpr char esc_end[] = {ESC, ESC_END, 0};
    constexpr char esc_esc[] = {ESC, ESC_ESC, 0};
    size_t collected = 0;
    auto* in = msg.str;
    auto commit = [&]{
        if (collected) out(msg2struct::String{in, collected});
        in += collected;
        collected = 0;
    };
    for (size_t i = 0; i < msg.size; ++i) {
        char ch = msg.str[i];
        if (ch == ESC) {
            commit();
            in++;
            out(msg2struct::String{esc_esc, 2});
        } else if (ch == END) {
            commit();
            in++;
            out(msg2struct::String{esc_end, 2});
        } else {
            collected++;
        }
    }
    commit();
}

}