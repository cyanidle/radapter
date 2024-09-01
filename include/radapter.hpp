#ifndef RADAPTER_HPP
#define RADAPTER_HPP

#include <QString>
#include <QObject>
#include <QMap>
#include <QVariant>
#include "config.hpp"
#include "worker.hpp"

struct lua_State;

namespace radapter
{

namespace detail {
template<typename Cls, typename T> Cls* getcls(QVariant(Cls::*)(T));
}

class Instance;
using Factory = Worker*(*)(QVariantList const&, Instance*);
using ExtraMethod = QVariant(*)(Worker*, QVariantList const&);
using ExtraMethods = QMap<QString, ExtraMethod>;
using ExtraFunction = std::function<QVariant(Instance*, QVariantList const&)>;
using ExtraSchema = QVariant(*)();

template<typename T>
QVariant SchemaFor() {
    T val;
    QVariant res;
    PopulateSchema(val, res);
    return res;
}

template<typename T>
Worker* FactoryFor(QVariantList const& args, Instance* parent) {
    return new T{args, parent};
}
template<auto f>
QVariant AsExtraMethod(Worker* w, QVariantList const& args) {
    using cls = decltype(detail::getcls(f));
    return (static_cast<cls>(w)->*f)(args);
}

class Instance : public QObject
{
    Q_OBJECT
public:
    struct Impl;
    Instance();
    virtual ~Instance() override;

    template<typename T>
    void RegisterWorker(const char* name, ExtraMethods const& extra = {}) {
        static_assert(std::is_base_of_v<Worker, T>);
        RegisterWorker(name, FactoryFor<T>, extra);
    }
    QVariantMap GetSchemas();
    QSet<Worker*> GetWorkers();
    void Log(LogLevel lvl, const char *cat, fmt::string_view fmt, fmt::format_args args);
    template<typename...Args>
    void Debug(const char* cat, fmt::format_string<Args...> fmt, Args&&...a) {
        Log(debug, cat, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Info(const char* cat, fmt::format_string<Args...> fmt, Args&&...a) {
        Log(info, cat, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Warn(const char* cat, fmt::format_string<Args...> fmt, Args&&...a) {
        Log(warn, cat, fmt, fmt::make_format_args(a...));
    }
    template<typename...Args>
    void Error(const char* cat, fmt::format_string<Args...> fmt, Args&&...a) {
        Log(error, cat, fmt, fmt::make_format_args(a...));
    }
    void RegisterSchema(const char* name, ExtraSchema schemaGen);
    void RegisterFunc(const char* name, ExtraFunction func);
    void RegisterGlobal(const char* name, QVariant const& value);
    void RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra = {});
    void EvalFile(fs::path path);
    void Eval(std::string_view code);
    void Shutdown(unsigned timeout = 5000);

    lua_State* LuaState();
    static Instance* FromLua(lua_State* L);
signals:
    void WorkerCreated(Worker* worker);
    void HasShutdown();
private:

    QScopedPointer<Impl> d;
};


template<typename T>
struct KeyVal {
    string key;
    T value;
};
using FlatMap = vector<KeyVal<QVariant>>;
void Flatten(FlatMap& out, QVariant const& input);
void Unflatten(QVariant& out, FlatMap const& flat);
//! @return amount of affected keys
size_t MergePatch(QVariant& out, QVariant const& patch, QVariant* diff = nullptr);

}

#endif //RADAPTER_HPP
