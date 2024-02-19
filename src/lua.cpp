#include "lua.hpp"
#include "logs.hpp"

using namespace radapter;
using namespace radapter::lua;

std::string_view lua::ToString(lua_State *L, int idx) noexcept {
    size_t len;
    auto ptr = lua_tolstring(L, idx, &len);
    return {ptr, len};
}

std::string_view lua::ToStringWithConv(lua_State *L, int idx) noexcept {
    size_t len;
    auto ptr = luaL_tolstring(L, idx, &len);
    return {ptr, len};
}

static int jsonmt = LUA_REFNIL;
static std::vector<bool> visited;

static bool isArray(lua_State* L, int idx) {
    lua_len(L, idx);
    auto len = lua_tonumber(L, -1);
    visited.clear();
    visited.resize(len);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) != LUA_TNUMBER) {
            return false;
        }
        if (!lua_isinteger(L, -2)) {
            return false;
        }
        auto asInt = lua_tointeger(L, -2);
        if (auto&& was = visited[asInt - 1]) {
            return false;
        } else {
            was = true;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return true;
}

static void doToJson(lua_State *L, Json& out) {
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        out = bool(lua_toboolean(L, -1));
        break;
    case LUA_TNIL:
        out = Json{};
        break;
    case LUA_TNUMBER: {
        if (lua_isinteger(L, -1)) {
            out = {lua_tointeger(L, -1)};
            break;
        }
        out = lua_tonumber(L, -1);
        break;
    }
    case LUA_TSTRING:
        out = lua::ToString(L, -1);
        break;
    case LUA_TTABLE: {
        if (isArray(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                //
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        } else {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (lua_type(L, -2) != LUA_TSTRING) {
                    throw Err("non string key in json");
                }
                Json helper;
                doToJson(L, helper);
                out[lua::ToString(L, -2)] = std::move(helper);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    case LUA_TUSERDATA: {
        if(auto ud = luaL_testudata(L, -1, "Json")) {
            out = *static_cast<Json*>(ud);
        }
        throw Err("userdata is not supported for json");
    }
    default: throw Err("unsupported type for json: {}", lua_typename(L, -1));
    }
}

Json lua::TopToJson(lua_State *L) noexcept try
{
    Json result;
    doToJson(L, result);
    return result;
} catch (std::exception& exc) {
    Error(L, exc.what());
}

static int newJson(lua_State* L) noexcept {
    auto n = lua_gettop(L);
    if (n == 0) {
        auto out = static_cast<Json*>(lua_newuserdata(L, sizeof(Json)));
        new (out) Json {Json{}};
        luaL_getmetatable(L, "Json");
        lua_setmetatable(L, -2);
        return 1;
    } else if (n == 1) {
        auto out = static_cast<Json*>(lua_newuserdata(L, sizeof(Json)));
        if(auto ud = luaL_testudata(L, 1, "Json")) {
            new (out) Json {*static_cast<Json*>(ud)};
        } else {
            lua_pushvalue(L, 1);
            auto source = TopToJson(L);
            lua_pop(L, 1);
            new (out) Json {source};
        }
        luaL_getmetatable(L, "Json");
        lua_setmetatable(L, -2);
        return 1;
    } else {
        Error(L, "json.new() accepts 0 or 1 params");
    }
}

static int setItem(lua_State* L) noexcept {
    //t, key, val
}

static int getItem(lua_State* L) noexcept {
    //t, key
}

static int getSz(lua_State* L) noexcept {
    //t
}

static luaL_Reg jsonLib[] = {
    {"new", newJson},
    {"__len", getSz},
    {"__index", getItem},
    {"__newindex", setItem},
    gcFor<Json>,
    {NULL, NULL}
};

void lua::RegisterJson(lua_State *L)
{
    luaL_newmetatable(L, "Json");
    jsonmt = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushliteral(L, "json");
    luaL_newlib(L, jsonLib);
    lua_setglobal(L, "json");
}

void PushJson(Json j) noexcept
{

}
