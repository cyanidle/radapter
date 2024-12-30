#include "instance_impl.hpp"

int radapter::Instance::Impl::luaLog(lua_State *L) {
    auto inst = Instance::FromLua(L);
    if (inst->d->insideLogHandler) return 0;
    builtin::api::Format(L);
    auto lvl = LogLevel(lua_tointeger(L, lua_upvalueindex(1)));
    size_t len;
    auto s = lua_tolstring(L, -1, &len);
    auto sv = string_view{s, len};
    string_view cat = "lua";
    lua_Debug ar;
    if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "S", &ar)) {
        cat = ar.short_src;
        auto pos = cat.find_last_of("/\\");
        if (pos != string_view::npos) {
            cat = cat.substr(pos + 1);
        }
    }
    // cat is null-terminated
    Instance::FromLua(L)->Log(lvl, cat.data(), "{}", fmt::make_format_args(sv));
    return 0;
}

int radapter::Instance::Impl::log_level(lua_State *L) {
    luaL_checktype(L, 1, LUA_TSTRING);
    auto count = lua_gettop(L);
    auto inst = Instance::FromLua(L);
    if (count == 1) {
        auto lvl = builtin::help::toSV(L, 1);
        if (!describe::name_to_enum(lvl, inst->d->globalLevel)) {
            throw Err("Invalid log_level passed: {}, avail: [{}]", lvl, fmt::join(describe::field_names<LogLevel>(), ", "));
        }
    } else {
        luaL_checktype(L, 2, LUA_TSTRING);
        auto cat = builtin::help::toSV(L, 1);
        auto lvl = builtin::help::toSV(L, 2);
        if (!describe::name_to_enum(lvl, inst->d->perCat[string{cat}])) {
            throw Err("Invalid log_level passed: {}, avail: [{}]", lvl, fmt::join(describe::field_names<LogLevel>(), ", "));
        }
    }
    return 0;
}

int radapter::Instance::Impl::log__call(lua_State *L) {
    lua_remove(L, 1);
    return luaLog(L);
}

int radapter::Instance::Impl::log_handler(lua_State *L) {
    auto inst = Instance::FromLua(L);
    if (lua_isnil(L, 1)) {
        luaL_unref(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
        inst->d->luaHandler = LUA_NOREF;
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        luaL_unref(L, LUA_REGISTRYINDEX, inst->d->luaHandler);
        lua_pushvalue(L, 1);
        inst->d->luaHandler = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    return 0;
}