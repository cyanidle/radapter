#pragma once
#include <string_view>
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <QObject>
#include "json_view.hpp"
extern "C" {
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>
#include <lua5.4/lua.h>
}


namespace radapter
{

using namespace jv;
#define QSV(x) QStringViewLiteral(x)
using std::string_view;
using std::string;

}
