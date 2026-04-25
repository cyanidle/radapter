#include "radapter/function.hpp"
#include "builtin.hpp"

using namespace radapter;

QVariant LuaFunction::Call(QVariantList const& args, TracebackMode trace) const
{
    if (!(*this)) {
        Raise("Attempt to call invalid lua function");
    }
    if (!lua_checkstack(_L, args.size() + 2)) {
        Raise("Could not reserve stack for call");
    }
    if (trace) {
        lua_pushcfunction(_L, builtin::help::traceback);
    }
    auto msgh = trace ? lua_gettop(_L) : 0;
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    for (auto& a: args) {
        glua::Push(_L, a);
    }
    auto status = lua_pcall(_L, args.size(), 1, msgh);
    if (status != LUA_OK) {
        Raise("{}", lua_tostring(_L, -1));
    }
    return builtin::help::toQVar(_L);
}
