#pragma once
#include "common.hpp"
#include <qglobal.h>

[[maybe_unused]]
static long tryInt(std::string_view part) {
    long res = 0;
    auto sz = part.size();
    if (sz < 3 || part[0] != '[' || part[sz-1] != ']') return -1;
    part = part.substr(1, sz - 2);
    for (auto ch: part) {
        if ('0' <= ch && ch <= '9') {
            res = res * 10 + (ch - '0'); //check for overflow?
        } else {
            return -1;
        }
    }
    return res;
}
