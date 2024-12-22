#ifndef RADAPTER_CONFIG_HPP
#define RADAPTER_CONFIG_HPP

#include <QVariant>
#include <fmt/ranges.h>
#include <optional>
#include "describe/describe.hpp"
#include "radapter/common.hpp"
#include "logs.hpp"
#include "trace_frame.hpp"
#include "function.hpp"

namespace radapter {

template<typename T>
struct WithDefault {
    template<typename...Args>
    WithDefault(Args&&...a) : value{std::forward<Args>(a)...} {}

    operator T const&() const noexcept {
        return value;
    }
    T value;
};

template<typename T>
struct OptionalPtr {
    std::unique_ptr<T> value;

    OptionalPtr() noexcept {}
    template<typename...Args>
    OptionalPtr(Args&&...a) : value(std::make_unique<T>(std::forward<Args>(a)...)) {}

    explicit operator bool() const noexcept {
        return bool(value);
    }
};

string TypeNameOf(QVariant const& conf);
void CheckCanConvert(int targetTypeId, QVariant const& from, TraceFrame const& frame);
template<typename T>
void CheckCanConvert(QVariant const& from, TraceFrame const& frame) {
    CheckCanConvert(QMetaType::fromType<T>().id(), from, frame);
}

template<typename T>
constexpr bool string_like = std::is_convertible_v<T, QString> || std::is_convertible_v<T, string>;
template<typename T>
using if_struct = std::enable_if_t<describe::is_described_struct_v<T>, int>;
template<typename T>
using if_enum = std::enable_if_t<describe::is_described_enum_v<T>, int>;
template<typename T>
using if_numeric = std::enable_if_t<std::is_arithmetic_v<T>, int>;
template<typename T>
using if_custom_object = std::enable_if_t<std::is_base_of_v<QObject, T>, int>;

void Parse(bool& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(string& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(QString& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(QVariant& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(QVariantMap& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(QVariantList& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(QObject*& out, QVariant const& conf, const TraceFrame &frame = {});
void Parse(LuaFunction& out, QVariant const& conf, const TraceFrame &frame = {});

template<typename T, if_custom_object<T> = 1>
void Parse(T*& out, QVariant const& conf, TraceFrame const& frame = {}) {
    QObject* o;
    Parse(o, conf, frame);
    if (auto c = qobject_cast<T*>(o)) {
        out = c;
    } else {
        throw Err("{}: could not cast {} => {}",
                  frame,
                  o ? o->metaObject()->className() : "<nil>",
                  T::staticMetaObject.className());
    }
}

template<typename T, if_enum<T> = 1>
void Parse(T& out, QVariant const& conf, TraceFrame const& frame = {}) {
    constexpr auto desc = describe::Get<T>();
    if (!conf.canConvert<QString>()) {
        throw Err("{}: Cannot convert to {} from {}", frame, desc.name, TypeNameOf(conf));
    }
    auto name = conf.toString().toStdString();
    if (!describe::name_to_enum(name, out)) {
        throw Err("{}: Invalid value for enum ({}): {}. Valid: []",
                  frame, desc.name, name, fmt::join(describe::field_names<T>(), ", "));
    }
}

template<typename T, if_numeric<T> = 1>
void Parse(T& out, QVariant const& conf, TraceFrame const& frame = {}) {
    CheckCanConvert<T>(conf, frame);
    out = conf.value<T>();
    if (QVariant(out) != conf) {
        throw Err("{}: Could not convert to number from: {} ({})",
                  frame, conf.toString().toStdString(), TypeNameOf(conf));
    }
}

template<typename T, if_struct<T> = 1>
void Parse(T& out, QVariant const& conf, TraceFrame const& frame = {}) {
    constexpr auto desc = describe::Get<T>();
    TraceFrame clsroot = TraceFrame(desc.name, frame);
    TraceFrame const& current = frame.IsRoot() ? clsroot : frame;
    if (conf.type() != conf.Map) {
        throw Err("{}: Cannot get {} from {}", current, desc.name, TypeNameOf(conf));
    }
    auto asMap = conf.toMap();
    desc.for_each([&](auto f){
        if constexpr (f.is_field) {
            static const QString k = QString::fromLatin1(f.name.data(), int(f.name.size()));
            Parse(f.get(out), asMap.value(k), TraceFrame(f.name, current));
        }
    });
}

template<typename T>
void Parse(WithDefault<T>& out, QVariant const& conf, TraceFrame const& frame = {}) {
    if (conf.isNull()) return;
    Parse(out.value, conf, frame);
}

template<typename T>
void Parse(std::optional<T>& out, QVariant const& conf, TraceFrame const& frame = {}) {
    if (conf.isNull()) {
        out.reset();
    } else {
        Parse(out.emplace(), conf, frame);
    }
}

template<typename T>
void Parse(OptionalPtr<T>& out, QVariant const& conf, TraceFrame const& frame = {}) {
    if (conf.isNull()) {
        out.value.reset();
    } else {
        out.value.reset(new T);
        Parse(*out.value, conf, frame);
    }
}


template<typename T>
void Parse(vector<T>& out, QVariant const& conf, TraceFrame const& frame = {}) {
    auto t = conf.type();
    if constexpr (string_like<T>) {
        if (t == QVariant::StringList) {
            auto l = conf.toStringList();
            out.resize(l.size());
            int idx = 0;
            for (auto& str: out) {
                auto i = idx++;
                if constexpr (std::is_same_v<T, string>) {
                    str = l.at(i).toStdString();
                } else {
                    str = l.at(i);
                }
            }
            return;
        }
    }
    if (t == QVariant::List) {
        auto l = conf.toList();
        out.resize(size_t(l.size()));
        int idx = 0;
        for (auto& v: out) {
            auto i = idx++;
            Parse(v, l[i], TraceFrame(unsigned(i), frame));
        }
        return;
    }
    throw Err("{}: Non-array config passed ({})", frame, TypeNameOf(conf));
}

template<typename K, typename T>
void Parse(map<K, T>& out, QVariant const& conf, TraceFrame const& frame = {}) {
    static_assert(string_like<K>);
    if (conf.type() != conf.Map) {
        throw Err("{}: Cannot parse map from {}", frame, TypeNameOf(conf));
    }
    out.clear();
    auto asmap = conf.toMap();
    for (auto it = asmap.keyValueBegin(); it != asmap.keyValueEnd(); ++it) {
        if constexpr (std::is_same_v<K, string>) {
            auto k = K(it->first.toStdString());
            auto sv = string_view(k); //unsafe? (k is moved but view should be valid)
            Parse(out[k], it->second, TraceFrame(sv, frame));
        } else {
            Parse(out[K(it->first)], it->second, TraceFrame(it->first.toStdString(), frame));
        }
    }
}


void PopulateSchema(bool&, QVariant& schema);
void PopulateSchema(string&, QVariant& schema);
void PopulateSchema(QString&, QVariant& schema);
void PopulateSchema(QVariant&, QVariant& schema);
void PopulateSchema(QVariantMap&, QVariant& schema);
void PopulateSchema(QVariantList&, QVariant& schema);
void PopulateSchema(QObject*&, QVariant& schema);
void PopulateSchema(LuaFunction&, QVariant& schema);

template<typename T, if_custom_object<T> = 1>
void PopulateSchema(T*&, QVariant& schema) {
    schema = T::staticMetaObject.className();
}

template<typename T, if_enum<T> = 1>
void PopulateSchema(T&, QVariant& schema) {
    QVariantList opts;
    for (auto k: describe::field_names<T>()) {
        opts.push_back(QString::fromLatin1(k.data(), int(k.size())));
    }
    schema = opts;
}

template<typename T, if_numeric<T> = 1>
void PopulateSchema(T&, QVariant& schema) {
    schema = QMetaType::fromType<T>().name();
}

template<typename T> void PopulateSchema(WithDefault<T>& out, QVariant& schema);
template<typename T> void PopulateSchema(std::optional<T>& out, QVariant& schema);
template<typename T> void PopulateSchema(std::optional<T>& out, QVariant& schema);
template<typename K, typename T> void PopulateSchema(map<K, T>& out, QVariant& schema);
template<typename T> void PopulateSchema(vector<T>& out, QVariant& schema);

template<typename T, if_struct<T> = 1>
void PopulateSchema(T& out, QVariant& schema) {
    constexpr auto desc = describe::Get<T>();
    QVariantMap map;
    desc.for_each([&](auto f){
        if constexpr (f.is_field) {
            QVariant nested;
            PopulateSchema(f.get(out), nested);
            map[QString::fromLatin1(f.name.data(), int(f.name.size()))] = nested;
        }
    });
    schema = map;
}

template<typename T>
void PopulateSchema(WithDefault<T>& out, QVariant& schema) {
    QVariant nested;
    PopulateSchema(out.value, nested);
    if (nested.canConvert<QString>()) {
        schema = nested.toString() + " [has_default]";
    } else {
        schema = nested;
    }
}

template<typename T>
void PopulateSchema(std::optional<T>& out, QVariant& schema) {
    QVariant nested;
    PopulateSchema(out.emplace(), nested);
    if (nested.canConvert<QString>()) {
        schema = nested.toString() + " [optional]";
    } else {
        schema = nested;
    }
}

template<typename T>
void PopulateSchema(OptionalPtr<T>&, QVariant& schema) {
    std::optional<T> opt;
    PopulateSchema(opt, schema);
}

template<typename T>
void PopulateSchema(vector<T>& out, QVariant& schema) {
    QVariant nested;
    PopulateSchema(out.emplace_back(), nested);
    schema = QVariantList{nested};
}

template<typename K, typename T>
void PopulateSchema(map<K, T>& out, QVariant& schema) {
    QVariant nested;
    PopulateSchema(out[K{}], nested);
    schema = QVariantMap{{"<key>", nested}};
}

} //radapter

#endif //RADAPTER_CONFIG_HPP
