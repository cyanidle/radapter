#include "serialize.hpp"


std::string radapter::TraceFrame::PrintTrace() const {
    string res;
    Walk([&](auto k){
        res += '.';
        if constexpr (std::is_same_v<decltype(k), string_view>) {
            res += k;
        } else {
            res += '['+std::to_string(k)+']';
        }
    });
    return res;
}

const radapter::TraceFrame *radapter::TraceFrame::prepWalk() const
{
    auto pr = prev;
    auto nx = this;
    while(pr) {
        pr->next = nx;
        nx = pr;
        pr = pr->prev;
    }
    return nx->next;
}
