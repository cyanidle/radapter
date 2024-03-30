#pragma once

#include "lua.hpp"
#include <QObject>


namespace radapter::modbus
{

struct Register
{
    string Name;
    lua::Ref OnChange;
};

struct Settings {

};



}
