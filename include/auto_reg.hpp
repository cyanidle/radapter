#pragma once
#include "lua.hpp"
#include "serialize.hpp"
#include <set>

namespace radapter
{

struct ClassSettings{};
struct Signal{};
struct NativeMethod{};

template<typename T>
string& NameOf() {
    static std::string storage = fmt::format("{}\0", describe::Get<T>().name);
    return storage;
}

namespace detail {
template<int offs, typename Fn, typename...Args, size_t...Is>
int Call(lua_State* L, Fn& func, meta::TypeList<Args...>, std::index_sequence<Is...>) {
    using ret = std::invoke_result_t<Fn, Args...>;
    if constexpr (std::is_void_v<ret>) {
        func(Deserialize<Args>(L, - int(Is) - 1 - offs)...);
        return 0;
    } else {
        Serialize(L, func(Deserialize<Args>(L, - int(Is) - 1 - offs)...));
        return 1;
    }
}

template<typename T, auto func>
int AsMethod(lua_State* L) {
    constexpr auto args = meta::FuncArgs_t<decltype(func)>{};
    auto clsName = static_cast<const char*>(lua_touserdata(L, lua_upvalueindex(1)));
    T* object = static_cast<T*>(luaL_checkudata(L, 1, clsName));
    auto impl = [&](auto&&...a) {
        return (object->*func)(std::forward<decltype(a)>(a)...);
    };
    return Call<1>(L, impl, args, args.idxs());
}

template<typename T, auto func>
int AsNative(lua_State* L) {
    constexpr auto args = meta::FuncArgs_t<decltype(func)>{};
    auto clsName = static_cast<const char*>(lua_touserdata(L, lua_upvalueindex(1)));
    T* object = static_cast<T*>(luaL_checkudata(L, 1, clsName));
    lua_rotate(L, 1, -1);
    lua_pop(L, 1);
    return (object->*func)(L);
}

template<typename...Args>
auto MakeSlotFor(const char* cls, lua::Ref func, meta::TypeList<Args...>) {
    return [func, cls = string{cls}](Args...a){
        func.push();
        auto err = lua::PCall(func.L, a...);
        if (err != 0) {
            logErr("Error invoking slot of '{}': {}", cls, lua::ToString(func.L, -1));
        }
    };
}

template<typename T, auto sig>
int AsSignal(lua_State* L) {
    constexpr auto args = meta::FuncArgs_t<decltype(sig)>{};
    auto clsName = static_cast<const char*>(lua_touserdata(L, lua_upvalueindex(1)));
    T* object = static_cast<T*>(luaL_checkudata(L, 1, clsName));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    QObject::connect(object, sig, MakeSlotFor(clsName, lua::Ref(L, 2), args));
    return 0;
}

template<int offs, typename T, typename...Args, size_t...Is>
T* Construct(void* stor, lua_State* L, std::index_sequence<Is...>) {
    return new (stor) T{Deserialize<Args>(L, -int(Is)-1-offs)...};
}

template<typename T, typename...Args>
int MakeNew(lua_State* L) {
    constexpr auto count = sizeof...(Args);
    auto clsName = static_cast<const char*>(lua_touserdata(L, lua_upvalueindex(1)));
    while (lua_gettop(L) < count) {
        lua_pushnil(L);
    }
    auto ud = lua_newuserdata(L, sizeof(T));
    Construct<1, T, Args...>(ud, L, std::index_sequence_for<Args...>());
    luaL_setmetatable(L, clsName);
    return 1;
}
} //detail

template<typename T>
void MakeClass(lua_State* L) {
    using namespace detail;
    constexpr auto desc = describe::Get<T>();
    constexpr auto metaMethods = 2;
    constexpr auto totalMethods = desc.methods_count + metaMethods;
    luaL_Reg methods[totalMethods + 1];
    methods[0] = lua::gcFor<T>;
    using settings = describe::extract_attr_t<ClassSettings, T>;
    if constexpr (std::is_void_v<settings>) {
        methods[1] = {"new", MakeNew<T>};
    } else {
        methods[1] = {"new", MakeNew<T, settings>};
    }
    methods[totalMethods] = {NULL, NULL};
    size_t idx = metaMethods;
    desc.for_each_method([&](auto m){
        using m_t = decltype(m);
        auto& method = methods[idx++];
        if constexpr (describe::has_attr_v<Signal, m_t>) {
            static string nameStorage = "On" + string{m.name};
            method = {nameStorage.c_str(), lua::Protected<AsSignal<T, m.value>>};
        } else if constexpr (describe::has_attr_v<NativeMethod, m_t>) {
            static string nameStorage{m.name};
            method = {nameStorage.c_str(), lua::Protected<AsNative<T, m.value>>};
        } else {
            static string nameStorage{m.name};
            method = {nameStorage.c_str(), lua::Protected<AsMethod<T, m.value>>};
        }
    });
    if (!luaL_newmetatable(L, NameOf<T>().c_str())) {
        throw Err("Could not create metatable for class: {}", NameOf<T>());
    }
    lua_pushlightuserdata(L, NameOf<T>().data());
    luaL_setfuncs(L, methods, 1);
    auto meta = lua_absindex(L, -1);

    Serialize(L, "__index");
    lua_pushvalue(L, meta);
    lua_rawset(L, meta);

    // leave metatable on top
}
    
}
