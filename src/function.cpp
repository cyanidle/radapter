#include "radapter/function.hpp"
#include "builtin.hpp"

using namespace radapter;

QVariant LuaFunction::Call(QVariantList const& args) const
{
    if (!(*this)) {
        throw Err("Attempt to call invalid lua function");
    }
    if (!lua_checkstack(_L, args.size() + 2)) {
        throw Err("Could not reserve stack for call");
    }
    lua_pushcfunction(_L, builtin::help::traceback);
    auto msgh = lua_gettop(_L);
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    for (auto& a: args) {
        glua::Push(_L, a);
    }
    auto status = lua_pcall(_L, args.size(), 1, msgh);
    if (status != LUA_OK) {
        throw Err("Error calling into lua: \n{}",
                  lua_tostring(_L, -1));
    }
    return builtin::help::toQVar(_L);
}
