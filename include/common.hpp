#pragma once
#include <memory>
#include <string_view>
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <QObject>
extern "C" {
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
}


namespace radapter
{

#define QSV(x) QStringViewLiteral(x)
using std::string_view;
using std::string;
using std::unique_ptr;

}


#ifdef __GNUC__
#define _Likely(x)       __builtin_expect(!!(x), 1)
#define _Unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define _Likely(x)       (x)
#define _Unlikely(x)     (x)
#endif
