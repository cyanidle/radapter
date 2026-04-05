#include <libcanard/canard.h>
#include <QString>
#include <qstringview.h>
#include <string_view>

class QVariant;
class QByteArray;

namespace radapter::can {
    class CyphalWorker;

    struct CanardMessageDynamic
    {
        QStringView name;
        QStringView full_name;
        QStringView full_name_and_ver;
        size_t extent;
        QVariant (*deserialize)(const uint8_t* buffer, size_t size);
        void (*serialize)(QVariant const& data, uint8_t* buffer, size_t size);
    };
    const CanardMessageDynamic* lookup_canard_type(QStringView name);
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

