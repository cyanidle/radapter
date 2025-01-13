#pragma once
#include "radapter/radapter.hpp"
#include "json_view/json_view.hpp"

static constexpr int intCut(size_t src) {
    return (std::max)(0, int(src));
}

inline std::string_view fromStr(const QString& s, jv::Arena& alloc) {
    auto u8 = s.toUtf8();
    return CopyString(std::string_view(u8.data(), unsigned(u8.size())), alloc);
}

template<>
struct jv::Convert<QVariant>
{
    static JsonView DoIntoJson(QVariant const& value, Arena& alloc) {
        switch (value.type()) {
        case QVariant::Map: {
            auto& map = *static_cast<const QVariantMap*>(value.constData());
            auto count = unsigned(map.size());
            auto res = MakeObjectOf(count, alloc);
            auto it = map.cbegin();
            for (auto i = 0u; i < count; ++i) {
                auto current = it++;
                res[i].key = fromStr(current.key(), alloc);
                res[i].value = JsonView::From(current.value(), alloc);
            }
            return JsonView(res, count);
        }
        case QVariant::List: {
            auto& list = *static_cast<const QVariantList*>(value.constData());
            auto count = unsigned(list.size());
            auto res = MakeArrayOf(count, alloc);
            for (auto i = 0u; i < count; ++i) {
                res[i] = JsonView::From(list[int(i)], alloc);
            }
            return JsonView(res, count);
        }
        case QVariant::Bool: {
            return JsonView(*static_cast<const bool*>(value.constData()));
        }
        case QVariant::Int: {
            return JsonView(*static_cast<const int*>(value.constData()));
        }
        case QVariant::UInt: {
            return JsonView(*static_cast<const unsigned int*>(value.constData()));
        }
        case QVariant::ULongLong: {
            return JsonView(*static_cast<const unsigned long long*>(value.constData()));
        }
        case QVariant::LongLong: {
            return JsonView(*static_cast<const long long*>(value.constData()));
        }
        case QVariant::Double: {
            return JsonView(*static_cast<const double*>(value.constData()));
        }
        case QVariant::ByteArray: {
            auto& arr = *static_cast<const QByteArray*>(value.constData());
            return JsonView::Binary(string_view(arr.data(), unsigned(arr.size())));
        }
        case QVariant::Char: {
            auto ch = *static_cast<const QChar*>(value.constData());
            return fromStr(QString(ch), alloc);
        }
        case QVariant::String: {
            auto& str = *static_cast<const QString*>(value.constData());
            return fromStr(str, alloc);
        }
        case QVariant::StringList: {
            auto& list = *static_cast<const QStringList*>(value.constData());
            auto count = unsigned(list.size());
            auto res = MakeArrayOf(count, alloc);
            for (auto i = 0u; i < count; ++i) {
                res[i] = fromStr(list[int(i)], alloc);
            }
            return JsonView(res, count);
        }
        default: {
            return {};
        }
        }
    }

    static void DoFromJson(QVariant& value, JsonView json, TraceFrame const& frame) {
        switch (json.GetType())
        {
        case t_binary: {
            auto bin = json.GetBinaryUnsafe();
            value = QByteArray(bin.data(), intCut(bin.size()));
            break;
        }
        case t_boolean: {
            value = json.GetUnsafe().d.boolean;
            break;
        }
        case t_number: {
            value = json.GetUnsafe().d.number;
            break;
        }
        case t_string: {
            auto str = json.GetStringUnsafe();
            value = QString::fromUtf8(str.data(), intCut(str.size()));
            break;
        }
        case t_signed: {
            value = QVariant(qlonglong(json.GetUnsafe().d.integer));
            break;
        }
        case t_unsigned: {
            value = QVariant(qulonglong(json.GetUnsafe().d.uinteger));
            break;
        }
        case t_array: {
            QVariantList res;
            auto src = json.Array(false);
            if (src.size() > INT_MAX) {
                throw radapter::Err("List is too big for conversion from json");
            }
            res.reserve(int(src.size()));
            for (unsigned i = 0; i < src.size(); ++i) {
                res.push_back(src.begin()[i].Get<QVariant>(TraceFrame(i, frame)));
            }
            value = std::move(res);
            break;
        }
        case t_object: {
            QVariantMap res;
            auto src = json.Object(false);
            for (unsigned i = 0; i < src.size(); ++i) {
                auto& curr = src.begin()[i];
                auto& v = res[QString::fromUtf8(curr.key.data(), intCut(curr.key.size()))];
                v = curr.value.Get<QVariant>(TraceFrame(curr.key, frame));
            }
            value = std::move(res);
            break;
        }
        default:
            break;
        }
    }
};


namespace glua {
void Push(lua_State* L, QVariant const& val);
}

#ifdef RADAPTER_JIT
#define lua_udata(L, ...) lua_newuserdata(L, (__VA_ARGS__))
#else
#define lua_udata(L, ...) lua_newuserdatauv(L, (__VA_ARGS__), 0)
#endif

namespace radapter::builtin {

namespace help {
void PrintStack(lua_State* L, string msg = "");
int traceback(lua_State* L) noexcept;
QVariant toQVar(lua_State* L, int idx = -1);
QString toQStr(lua_State* L, int idx = -1);
string_view toSV(lua_State* L, int idx = -1) noexcept;
QVariantList toArgs(lua_State* L, int from);
}

namespace api {
int Format(lua_State* L);
int Get(lua_State* L);
int Set(lua_State* L);
int Each(lua_State* L);
int After(lua_State* L);
int LoadPlugin(lua_State* L);
}


namespace workers {

inline int Marker;

void gui(Instance* inst);
void test(Instance* inst);
void modbus(Instance* inst);
void websocket(Instance* inst);
void redis(Instance* inst);
void sql(Instance* inst);
void serial(Instance* inst);

using InitSystem = void(*)(Instance*);

// gui is separate
inline InitSystem all[] = {
    test, modbus, websocket, 
    redis, sql, serial,
};

}

}
