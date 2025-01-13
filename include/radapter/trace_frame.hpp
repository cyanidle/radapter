#ifndef RADAPTER_TRACE_FRAME
#define RADAPTER_TRACE_FRAME

#include <fmt/base.h>
#include <string_view>
#include <string>
#include <QtGlobal>

namespace radapter
{

struct RADAPTER_API TraceFrame {
    constexpr TraceFrame() noexcept {}
    TraceFrame(TraceFrame&&) = delete;
    constexpr TraceFrame(unsigned idx, TraceFrame const& prev) noexcept :
        prev(&prev), size(idx), str(nullptr)
    {}
    constexpr TraceFrame(std::string_view key, TraceFrame const& prev) noexcept :
        prev(&prev), size(unsigned(key.size())), str(key.data())
    {
        str = str ? str : "";
    }
    template<typename F> void Walk(F&& f) const {
        auto node = prepareWalk();
        while(node) {
            if(node->str) f(std::string_view{node->str, node->size});
            else f(node->size);
            node = node->next;
        }
    }
    constexpr void SetIndex(unsigned _idx) noexcept {
        size = _idx;
        str = nullptr;
    }
    constexpr void SetKey(std::string_view key) noexcept {
        size = unsigned(key.size());
        str = key.data();
    }
    constexpr bool IsRoot() const noexcept {
        return prev == nullptr;
    }
    std::string PrintTrace() const
    {
        std::string result;
        Walk([&](auto key) {
            if constexpr (std::is_same_v<decltype(key), unsigned>) {
                result += std::string_view(".[");
                result += std::to_string(key);
                result += ']';
            } else {
                result += '.';
                result += key;
            }
        });
        return result;
    }
private:
    const TraceFrame* prepareWalk() const {
        if (!prev)
            return nullptr;
        const TraceFrame* parent = prev;
        const TraceFrame* current = this;
        while(parent) {
            parent->next = current;
            current = parent;
            parent = parent->prev;
        }
        return current->next;
    }

    const TraceFrame* prev{};
    mutable const TraceFrame* next{};
    unsigned size{};
    const char* str{};
};

} //radapter

template<>
struct fmt::formatter<radapter::TraceFrame> : fmt::formatter<string_view> {
    template<typename Ctx>
    auto format(const radapter::TraceFrame& frame, Ctx& ctx) const {
        return fmt::formatter<string_view>::format(frame.PrintTrace(), ctx);
    }
};

#endif //RADAPTER_TRACE_FRAME
