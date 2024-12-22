#ifndef RADAPTER_HPP
#define RADAPTER_HPP

#include <QString>
#include <QObject>
#include <QMap>
#include <QVariant>
#include "radapter/config.hpp"
#include "logs.hpp"
#include "radapter/worker.hpp"
#include "glua/glua.hpp"

struct lua_State;
typedef int(*lua_CFunction)(lua_State*);

namespace radapter
{

template<typename Fn> struct defer {
    Fn f;
    defer(Fn f) : f(std::move(f)) {}
    defer(defer &&) = delete;
    ~defer() noexcept(false) {f();}
};
template<typename T> defer(T) -> defer<T>;

namespace compat {
int lua_absindex(lua_State *L, int i);
int luaL_getsubtable(lua_State *L, int i, const char *name);
void luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb);
void prequiref(lua_State *L, const char *modname, lua_CFunction openf, int glb);
}

namespace detail {
template<typename Cls, typename T> Cls* getcls(QVariant(Cls::*)(T));
}

template<typename T>
using if_valid_worker = std::enable_if_t<std::is_base_of_v<Worker, T>, int>;

class Instance;
using Factory = Worker*(*)(QVariantList const&, Instance*);
using ExtraMethod = QVariant(*)(Worker*, QVariantList const&);
using ExtraMethods = QMap<QString, ExtraMethod>;
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

QVariant MakeFunction(ExtraFunction func);

class Instance : public QObject
{
    Q_OBJECT
public:
    struct Impl;
    Instance();
    virtual ~Instance() override;

    void RegisterGlobal(const char* name, QVariant const& value);
    void RegisterFunc(const char* name, ExtraFunction func);

    void RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra = {});
    template<typename T, if_valid_worker<T> = 1>
    void RegisterWorker(const char* name, ExtraMethods const& extra = {}) {
        static_assert(std::is_base_of_v<Worker, T>);
        RegisterWorker(name, FactoryFor<T>, extra);
    }

    void RegisterSchema(const char* name, ExtraSchema schemaGen);
    template<typename T>
    void RegisterSchema(const char* name) {
        RegisterSchema(name, SchemaFor<T>);
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

    struct DebuggerOpts {
        string host = "127.0.0.1";
        uint16_t port = 8172;
        bool vscode = {false};
    };
    void DebuggerConnect(DebuggerOpts opts);

    enum LoadEmbedOpts {
        LoadEmbedGlobal = 1,
        LoadEmbedNoPop = 1,
    };

    void LoadEmbeddedFile(string name, int opts = 0);

    void EvalFile(fs::path path);
    void Eval(string_view code, string_view chunk = "<eval>");

    void Shutdown(unsigned timeout = 5000);

    lua_State* LuaState();
    static Instance* FromLua(lua_State* L);
signals:
    void ShutdownRequest();
    void WorkerCreated(Worker* worker);
    void ShutdownDone();
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
