#include "worker.hpp"
#include "pushpop.hpp"

namespace radapter
{

Worker::Worker(Instance *parent, const char *category) : QObject(parent), _category(category)
{
    _Inst = parent;
    _self = LUA_NOREF;
    _subs = LUA_NOREF;
}

void Worker::Log(LogLevel lvl, fmt::string_view fmt, fmt::format_args args)
{
    _Inst->Log(lvl, _category, fmt, args);
}

void Worker::Shutdown() {
    emit ShutdownDone();
}

Worker::~Worker()
{
    auto L = _Inst->LuaState();
    luaL_unref(L, LUA_REGISTRYINDEX, _subs);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _self);
    *static_cast<Worker**>(lua_touserdata(L, -1)) = nullptr;
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, _self);
}

}
