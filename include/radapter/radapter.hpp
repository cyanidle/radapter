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
int RADAPTER_API lua_absindex(lua_State *L, int i);
int RADAPTER_API luaL_getsubtable(lua_State *L, int i, const char *name);
void RADAPTER_API luaL_requiref(lua_State *L, const char *modname, lua_CFunction openf, int glb);
void RADAPTER_API prequiref(lua_State *L, const char *modname, lua_CFunction openf, int glb);
}

namespace detail {
template<typename Cls, typename Ret, typename... Args>
Cls* getcls(Ret(Cls::*)(Args...));

template<typename T> struct is_tuple : std::false_type {};
template<typename...Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};
}

class Instance;
using Factory = Worker*(*)(QVariantList const&, Instance*);
using ExtraSchema = QVariant(*)();

template<typename T>
QVariant SchemaFor() {
    T val;
    QVariant res;
    PopulateSchema(val, res);
    return res;
}

struct WorkerArguments
{
    QVariantList args;

    operator QVariantList() { return std::move(args); }

    operator QVariant() {
        if (args.size() < 1)
            Raise("Expected at least 1 argument");
        return args[0];
    }

    template<typename T>
    operator T() {
        if constexpr (detail::is_tuple<T>::value) {
            return ParseAs<T>(QVariant(args));
        } else {
            return ParseAs<T>(*this);
        }
    }
};

template<typename T>
using if_valid_worker = std::enable_if_t<
    std::is_constructible_v<T, WorkerArguments, Instance*>
    && std::is_base_of_v<Worker, T>,
    int>;

template<typename T>
Worker* FactoryFor(QVariantList const& args, Instance* parent) {
    return new T{WorkerArguments{args}, parent};
}

// If f takes QVariantList/QVariantList const&, pass args directly.
// Otherwise f must take a single std::tuple<...> which is filled via Parse.
template<auto f>
QVariant AsExtraMethod(Worker* w, QVariantList const& argList) {
    using Cls = std::remove_pointer_t<decltype(detail::getcls(f))>;
    return (static_cast<Cls*>(w)->*f)(WorkerArguments{argList});
}

QVariant RADAPTER_API MakeFunction(ExtraFunction func);

class RADAPTER_API Instance : public QObject
{
    Q_OBJECT
public:
    struct Impl;
    explicit Instance(QObject* parent = nullptr);
    virtual ~Instance() override;

    void RegisterGlobal(const char* name, QVariant const& value);
    void RegisterFunc(const char* name, ExtraFunction func);
    
    void EnableGui();
    void EnableTags();

    void RegisterWorker(const char* name, Factory factory, ExtraMethods const& extra = {});

    template<typename T, if_valid_worker<T> = 1>
    void RegisterWorker(const char* name, ExtraMethods const& extra = {}) {
        RegisterWorker(name, FactoryFor<T>, extra);
    }

    void RegisterSchema(const char* name, ExtraSchema schemaGen);
    template<typename T>
    void RegisterSchema(const char* name) {
        RegisterSchema(name, SchemaFor<T>);
    }

    // schemas of all registered workers, or only the named ones when `only` is non-empty
    QVariantMap GetSchemas(QStringList const& only = {});
    QSet<Worker*> GetWorkers();
    Worker* GetWorker(QString const& name);

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

    optional<fs::path> CurrentFile();

    void EvalFile(fs::path path);
    void Eval(string_view code, string_view chunk = "<eval>");

    lua_State* LuaState();
    static Instance* FromLua(lua_State* L);

    Impl* _GetPrivate() {
        return d.get();
    }
public slots:
    void Shutdown(unsigned timeout = 5000);
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
void RADAPTER_API Flatten(FlatMap& out, QVariant const& input);
void RADAPTER_API Unflatten(QVariant& out, FlatMap const& flat);
//! @return amount of affected keys
size_t RADAPTER_API MergePatch(QVariant& out, QVariant const& patch, QVariant* diff = nullptr);

}

#endif //RADAPTER_HPP
