#include "function.hpp"
#include "pushpop.hpp"

LuaFunction::LuaFunction() : _L(nullptr), _ref(LUA_NOREF)
{

}

LuaFunction::LuaFunction(const radapter::LuaFunction& o) : _L(o._L), _ref(LUA_NOREF)
{
    if (o) {
        lua_checkstack(_L, 1);
        lua_rawgeti(_L, LUA_REGISTRYINDEX, o._ref);
        _ref = luaL_ref(_L, LUA_REGISTRYINDEX);
    }
}

LuaFunction &LuaFunction::operator=(const LuaFunction &o)
{
    if (this != &o) {
        this->~LuaFunction();
        if (o) {
            _L = o._L;
            lua_checkstack(_L, 1);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, o._ref);
            _ref = luaL_ref(_L, LUA_REGISTRYINDEX);
        }
    }
    return *this;
}

LuaFunction::LuaFunction(lua_State *L, int idx)
{
    this->_L = L;
    if (!lua_checkstack(L, 1)) {
        throw Err("Could not reserve stack space");
    }
    lua_pushvalue(L, idx);
    _ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

LuaFunction::operator bool() const noexcept
{
    return _L && _ref != LUA_NOREF;
}

QVariant LuaFunction::Call(QVariantList const& args) const
{
    if (!(*this)) {
        throw Err("Attempt to call invalid lua function");
    }
    if (!lua_checkstack(_L, args.size() + 2)) {
        throw Err("Could not reserve stack for call");
    }
    lua_pushcfunction(_L, traceback);
    auto msgh = lua_gettop(_L);
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    for (auto& a: args) {
        push(_L, a);
    }
    auto status = lua_pcall(_L, args.size(), 1, msgh);
    if (status != LUA_OK) {
        throw Err("Error calling into lua: \n{}",
                  lua_tostring(_L, -1));
    }
    return toQVar(_L);
}

LuaFunction::~LuaFunction()
{
    if (_L) {
        luaL_unref(_L, LUA_REGISTRYINDEX, _ref);
    }
    _L = nullptr;
    _ref = LUA_NOREF;
}
