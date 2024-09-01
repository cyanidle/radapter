#include "config.hpp"

void radapter::Parse(bool &out, const QVariant &conf, TraceFrame const& frame) {
    CheckCanConvert<bool>(conf, frame);
    out = conf.toBool();
}

void radapter::Parse(string &out, const QVariant &conf, TraceFrame const& frame) {
    CheckCanConvert<QString>(conf, frame);
    out = conf.toString().toStdString();
}

void radapter::Parse(QString &out, const QVariant &conf, TraceFrame const& frame) {
    CheckCanConvert<QString>(conf, frame);
    out = conf.toString();
}

void radapter::Parse(QVariantMap &out, const QVariant &conf, const TraceFrame &frame)
{
    CheckCanConvert<QVariantMap>(conf, frame);
    out = conf.toMap();
}

void radapter::Parse(QVariant &out, const QVariant &conf, const TraceFrame &)
{
    out = conf;
}

void radapter::Parse(QVariantList &out, const QVariant &conf, const TraceFrame &frame)
{
    CheckCanConvert<QVariantList>(conf, frame);
    out = conf.toList();
}

void radapter::Parse(QObject *&out, const QVariant &conf, const TraceFrame &frame)
{
    CheckCanConvert<QObject*>(conf, frame);
    out = conf.value<QObject*>();
}

void radapter::Parse(LuaFunction &out, const QVariant &conf, const TraceFrame &frame)
{
    CheckCanConvert<LuaFunction>(conf, frame);
    out = conf.value<LuaFunction>();
}

void radapter::CheckCanConvert(int targetTypeId, const QVariant &from, const TraceFrame &frame)
{
    if (!from.canConvert(targetTypeId)) {
        throw Err("{}: Could not convert to '{}' from '{}'", frame, QMetaType(targetTypeId).name().data(), TypeNameOf(from));
    }
}

std::string radapter::TypeNameOf(const QVariant &conf)
{
    auto p = conf.typeName();
    return p ? string{p} : "<nil>";
}

void radapter::PopulateSchema(bool &, QVariant &schema) {
    schema = "bool";
}

void radapter::PopulateSchema(string &, QVariant &schema) {
    schema = "string";
}

void radapter::PopulateSchema(QString &, QVariant &schema) {
    schema = "string";
}

void radapter::PopulateSchema(QVariant &, QVariant &schema) {
    schema = "any";
}

void radapter::PopulateSchema(QVariantMap &, QVariant &schema) {
    schema = "any_map";
}

void radapter::PopulateSchema(QVariantList &, QVariant &schema) {
    schema = "any_list";
}

void radapter::PopulateSchema(QObject *&, QVariant &schema) {
    schema = "custom_object";
}

void radapter::PopulateSchema(LuaFunction &, QVariant &schema) {
    schema = "function";
}
