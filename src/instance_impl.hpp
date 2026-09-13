#pragma once
#include "radapter/radapter.hpp"
#include <QSet>
#include <QPointer>
#include <vector>
#include "builtin.hpp"
#include "tags.hpp"

class QQuickItem;

namespace radapter::qml_test {
class RecordFilter;
}

struct radapter::Instance::Impl {
    Instance* self;
    lua_State* L;
    lua_State* currentCaller = nullptr; // thread invoking a worker factory (may be a fiber's Lua thread)
    std::unique_ptr<FiberPool> fibers;
    std::unique_ptr<TagRegistry> tagRegistry;
    QSet<Worker*> workers;
    LogLevel globalLevel = LogLevel::debug;
    std::map<string, LogLevel, std::less<>> perCat;
    std::map<string, ExtraSchema> schemas;
    std::vector<LuaFunction> shutdownHandlers;
    bool shutdown = false;
    bool shutdownDone = false;
    bool shutdownIdleArmed = false;
    unsigned shutdownTimeout = 5000; // Shutdown()'s default budget
    int debuggerActive = 0; // 1 normal, 2 vscode
    int insideLogHandler = false;
    int luaLogHandler = LUA_NOREF;
    unsigned logCatLen = 12;
    optional<fs::path> currentFile;
    // non-empty when the running script was loaded over HTTP; base URL that relative
    // `require` paths resolve against.
    QString scriptBaseUrl;
    QPointer<qml_test::RecordFilter> guiRecordFilter;
    // non-window QQuickItem roots from QML{} workers with an Item-rooted QML file;
    // QML_Tester searches these when there is no QQuickWindow to search through
    std::vector<QPointer<QQuickItem>> guiItems;


    static int luaLog(lua_State* L);
    static int log_level(lua_State* L);
    static int log__call(lua_State* L); // convert __call(t, ...) -> luaLog(...)
    static int log_handler(lua_State* L);
    static int onShutdown(lua_State* L);

    void newThreadCreated(lua_State* T);
};

