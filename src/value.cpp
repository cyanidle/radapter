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

LuaValue::LuaValue(lua_State *L, ConsumeTopTag)
{
    this->_L = L;
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

void *LuaUserData::Data(const char *tname)
{
    if (!(*this)) {
        throw Err("Attempt to get data of invalid UserData");
    }
    if (!lua_checkstack(_L, 1)) {
        throw Err("Could not reserve stack for call");
    }
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    auto* d = luaL_testudata(_L, -1, tname);
    lua_pop(_L, 1);
    return d;
}

void *LuaUserData::UnsafeData()
{
    if (!lua_checkstack(_L, 1)) {
        throw Err("Could not reserve stack for call");
    }
    lua_rawgeti(_L, LUA_REGISTRYINDEX, _ref);
    void* res = lua_touserdata(_L, -1);;
    lua_pop(_L, 1);
    return res;
}

void* LuaUserData::init(lua_State *L, size_t sz)
{
    return lua_udata(L, sz);
}

static int wrapDtor(lua_State* L) noexcept {
    auto* dtor = reinterpret_cast<LuaUserData::Dtor>(lua_touserdata(L, lua_upvalueindex(1)));
    dtor(lua_touserdata(L, 1));
    return 0;
}

LuaUserData LuaUserData::initDone(lua_State* L, const char *tname, Dtor dtor)
{
    if (luaL_newmetatable(L, tname)) {
        lua_pushlightuserdata(L, reinterpret_cast<void*>(dtor));
        lua_pushcclosure(L, wrapDtor, 1);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -1);
    auto res = LuaUserData{L, -1};
    lua_pop(L, 1);
    return res;
}
