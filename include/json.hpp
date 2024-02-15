#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include "common.hpp"
#include "logs.hpp"

namespace radapter
{

struct Json
{
    using Type = QJsonValue::Type;
    Json() = default;
    Json(const Json&) = default;
    Json(Json&&) noexcept = default;
    Json& operator=(const Json&) = default;
    Json& operator=(Json&&) noexcept = default;
    Json(QJsonValue val) : val(val) {}
    Json(QJsonValueRef val) : val(val) {}
    Json(QJsonArray arr) : val(arr) {}
    Json(QJsonObject obj) : val(obj) {}
    operator QJsonValue() const {return val;}
    Json operator[](int idx);
    Json operator[](string_view key);
    static Json FromLua(lua_State* L, int idx = 0);
    static string_view PrintType(Type t);
    string_view PrintType() const;
    void AssertType(Type type, string_view msg = "json") const;
private:
    QJsonValue val;
};


}
