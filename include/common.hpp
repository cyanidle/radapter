#pragma once
#include <string_view>
#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <QObject>
extern "C" {
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>
#include <lua5.4/lua.h>
}


namespace radapter
{

#define QSV(x) QStringViewLiteral(x)
using std::string_view;
using std::string;

template<typename F> struct defer {
    F f;
    defer(F f) : f(std::move(f)) {}
    ~defer() noexcept(std::is_nothrow_invocable_v<F>) {f();}
};
template<typename F> defer(F) -> defer<F>;

}
