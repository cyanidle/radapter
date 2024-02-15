#include "json.hpp"

using namespace radapter;

Json Json::operator[](int idx) {
    AssertType(Type::Array, "[index]");
    auto a = val.toArray();
    if (idx >= a.size()) {
        throw Err("index is too big: {} (max: {})", idx, a.size());
    }
    return a.at(idx);
}

Json Json::FromLua(lua_State *L, int idx)
{

}

Json Json::operator[](string_view key) {
    AssertType(Type::Object, key);
    auto o = val.toObject();
    auto res = o.find(QLatin1String(key.data(), key.size()));
    if (res == o.end()) {
        throw Err("key missing: {}", key);
    }
    return *res;
}

std::string_view Json::PrintType(Type t) {
    switch (t) {
    case Type::String: return "String";
    case Type::Bool: return "Bool";
    case Type::Array: return "Array";
    case Type::Object: return "Object";
    case Type::Null: return "Null";
    case Type::Undefined: return "Undefined";
    default: return "Unknown";
    }
}

std::string_view Json::PrintType() const {
    return PrintType(val.type());
}

void Json::AssertType(Type type, string_view msg) const {
    if (type != val.type()) {
        throw Err("{}: Type Missmatch: actual: {} != wanted: {}",
                  msg, PrintType(), PrintType(type));
    }
}
