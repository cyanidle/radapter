#pragma once

#include "msg2struct.hpp"
#include "map.h"

// Public API
///////////////////////////////////
enum TopicDir {
  PUB = 1,
  SUB = 2,
  PUBSUB = PUB | SUB,
};

void topics_error(const char* err);
msg2struct::OutIterator topics_send_begin();
void topics_send_finish();
inline void topics_receive_msg(msg2struct::InIterator iter);


//MAKE_MSG()
//MAKE_MSG_INHERIT()
//TOPICS()


// Private Impl
///////////////////////////////////

#define OPEN_PARENS(x) x
#define EAT_PARENS(x)

#define GET_FIELD_NAME(x) EAT_PARENS x
#define MAKE_FIELD(x) OPEN_PARENS x;


#define MAKE_MSG(name, ...) struct name { \
  MAP(MAKE_FIELD, __VA_ARGS__) \
  MSG_2_STRUCT(MAP_LIST(GET_FIELD_NAME, __VA_ARGS__)) \
}

#define MAKE_MSG_INHERIT(parent, name, ...) struct name : parent { \
  MAP(MAKE_FIELD, __VA_ARGS__) \
  MSG_2_STRUCT_INHERIT(parent, MAP_LIST(GET_FIELD_NAME, __VA_ARGS__)) \
}

///////////////////////////////////

#define _TOPIC_FUNCS_PUB(name, msg) namespace topics {void send_##name(msg const& m) { \
  auto iter = _topics_prepareMsg(__COUNTER__ - _topics_pub_start); \
  if (Dump(m, iter)) topics_send_finish(iter); \
  else topics_error("Could not send: " #name); \
}}

#define _TOPIC_FUNCS_SUB(name, msg) namespace topics {void on_##name(msg& m);}

#define _TOPIC_FUNCS_PUBSUB(name, msg) \
  _TOPIC_FUNCS_PUB(name, msg) \
  _TOPIC_FUNCS_SUB(name, msg)

#define _TOPIC_FUNCS_AUX(name, msg, mode) _TOPIC_FUNCS_##mode(name, msg)
#define _TOPIC_FUNCS(tuple) _TOPIC_FUNCS_AUX tuple

///////////////////////////////////

#define _TOPIC_BODY_PUB(name, msg) case (__COUNTER__ - _topics_sub_start): return false;
#define _TOPIC_BODY_PUBSUB(name, msg) _TOPIC_BODY_SUB(name, msg)
#define _TOPIC_BODY_SUB(name, msg) case (__COUNTER__ - _topics_sub_start): { \
  msg m; \
  if(msg2struct::Parse(m, iter)) topics::on_##name(m);\
  else topics_error("MSG ERR: " #msg);\
  return true; \
}

#define _TOPIC_BODY_AUX(name, msg, mode) _TOPIC_BODY_##mode(name, msg)
#define _TOPIC_BODY(tuple) _TOPIC_BODY_AUX tuple

///////////////////////////////////

#define TOPICS(...) \
  constexpr int _topics_pub_start = __COUNTER__ + 1; \
  MAP(_TOPIC_FUNCS, __VA_ARGS__) \
  constexpr int _topics_sub_start = __COUNTER__ + 1; \
  bool _topics_check_msg(int id, msg2struct::InIterator iter) { \
    switch (id) { MAP(_TOPIC_BODY, __VA_ARGS__) default: return false;} \
  } \
  struct Topics {}

///////////////////////////////////

bool _topics_check_msg(int id, msg2struct::InIterator iter);

inline msg2struct::OutIterator _topics_prepareMsg(int id) {
  msg2struct::OutIterator iter = topics_send_begin();
  iter.BeginArray(2);
  iter.WriteInteger(id);
  return iter;
}

inline void topics_receive_msg(msg2struct::InIterator iter) {
  size_t arr;
  int id;
  if (!iter.GetArraySize(arr) || arr < 2 || !iter.GetInteger(id)) {
    topics_error("Error: could not get topic ID");
    return;
  }
  if (!_topics_check_msg(id, iter)) {
    char temp[50];
    sprintf(temp, "Error: Unknown msg type: %d", id);
    topics_error(temp);
  }
}