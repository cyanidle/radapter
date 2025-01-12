#include "radapter/value.hpp"
#include "builtin.hpp"

using namespace radapter;


LuaValue::LuaValue() : _L(nullptr), _ref(LUA_NOREF)
{

}

LuaValue::LuaValue(const radapter::LuaValue& o) : _L(o._L), _ref(LUA_NOREF)
{
    if (o) {
        lua_checkstack(_L, 1);
        lua_rawgeti(_L, LUA_REGISTRYINDEX, o._ref);
        _ref = luaL_ref(_L, LUA_REGISTRYINDEX);
    }
}

LuaValue &LuaValue::operator=(const LuaValue &o)
{
    if (this != &o) {
        this->~LuaValue();
        if (o) {
            _L = o._L;
            lua_checkstack(_L, 1);
            lua_rawgeti(_L, LUA_REGISTRYINDEX, o._ref);
            _ref = luaL_ref(_L, LUA_REGISTRYINDEX);
        }
    }
    return *this;
}

LuaValue::LuaValue(lua_State *L, int idx)
{
    this->_L = L;
    if (!lua_checkstack(L, 1)) {
        throw Err("Could not reserve stack space");
    }
    lua_pushvalue(L, idx);
    _ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

LuaValue::operator bool() const noexcept
{
    return _L && _ref != LUA_NOREF;
}

bool LuaValue::operator==(const LuaValue &o) const noexcept
{
    return _ref == o._ref;
}

LuaValue::~LuaValue()
{
    if (_L) {
        luaL_unref(_L, LUA_REGISTRYINDEX, _ref);
    }
    _L = nullptr;
    _ref = LUA_NOREF;
}

void *LuaUserData::Data()
{
    if (!(*this)) {
        throw Err("Attempt to get data of invalid UserData");
    }
    if (!lua_checkstack(_L, 1)) {
        throw Err("Could not reserve stack for call");
    }
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    auto* d = lua_touserdata(_L, -1);
    lua_pop(_L, 1);
    return d;
}
