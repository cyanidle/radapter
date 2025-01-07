#pragma once

#include "msg2struct.hpp"
#include "map.h"

#define OPEN_PARENS(x) x
#define EAT_PARENS(x)

#define GET_FIELD_NAME(x) EAT_PARENS x
#define MAKE_FIELD(x) OPEN_PARENS x;

#define MAKE_MSG(name, id, ...) struct name { \
  static constexpr int ID = id;\
  MAP(MAKE_FIELD, __VA_ARGS__) \
  MSG_2_STRUCT(MAP_LIST(GET_FIELD_NAME, __VA_ARGS__)) \
  void handle();\
}; void name::handle()