#pragma once
#include "radapter.hpp"

namespace glua {
void Push(lua_State* L, QVariant const& val);
}

#ifdef RADAPTER_JIT
#define lua_udata(L, ...) lua_newuserdata(L, (__VA_ARGS__))
#else
#define lua_udata(L, ...) lua_newuserdatauv(L, (__VA_ARGS__), 0)
#endif

namespace radapter::builtin {

namespace help {
void PrintStack(lua_State* L, string msg = "");
int traceback(lua_State* L) noexcept;
QVariant toQVar(lua_State* L, int idx = -1);
QString toQStr(lua_State* L, int idx = -1);
string_view toSV(lua_State* L, int idx = -1) noexcept;
QVariantList toArgs(lua_State* L, int from);
}

namespace api {
int Format(lua_State* L);
int Get(lua_State* L) noexcept;
int Set(lua_State* L) noexcept;
int Each(lua_State* L);
int After(lua_State* L);
}

namespace workers {
void test(Instance* inst);
void modbus(Instance* inst);
void websocket(Instance* inst);
void redis(Instance* inst);
void sql(Instance* inst);
using InitSystem = void(*)(Instance*);
inline InitSystem all[] = {
    test, modbus, websocket, redis, sql,
};
}


}
