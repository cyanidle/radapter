#pragma once
#include "radapter.hpp"

namespace radapter::builtin {

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
