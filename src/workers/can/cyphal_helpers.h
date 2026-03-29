#include <libcanard/canard.h>
#include <string_view>

namespace radapter::can {
    class CyphalWorker;

    struct CanardMessageDynamic
    {
        size_t extent;
    };

    CanardMessageDynamic* lookup_canard_type(std::string_view name);
}

template<auto method, typename R, typename...Args>
static R doCanardWrap(CanardInstance *const canard, Args...args)
{
    return (static_cast<radapter::can::CyphalWorker*>(canard->user_reference)->*method)(args...);
}

template<auto method, typename R, typename...Args>
static constexpr auto canardWrap(R(radapter::can::CyphalWorker::*)(Args...))
{
    return doCanardWrap<method, R, Args...>;
}

#define CANARD_WRAP(method) canardWrap<&CyphalWorker::method>(&CyphalWorker::method)

