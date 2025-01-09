#pragma once

#include "msg2struct.hpp"
#include "map.h"

// Public API
////////////////////////
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

////////////////////////

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

#define _MAKE_TOPIC_SUB(name, msg, mode) case (__COUNTER__ - _topics_sub_start): {\
  if (!(mode & SUB)) return false; \
  msg m; \
  void on_##name(msg& m);\
  if(msg2struct::Parse(m, iter)) on_##name(m);\
  else topics_error("MSG ERR: " #msg);\
  return true; \
}

#define _DO_MAKE_TOPIC_PUB(name, msg) void send_##name(msg const& m) { \
  auto iter = _topics_prepareMsg(__COUNTER__ - _topics_pub_start); \
  if (Dump(m, iter)) topics_send_finish(iter); \
  else topics_error("Could not send: " #name); \
}
#define _DO_MAKE_TOPIC_PUBSUB(...) _DO_MAKE_TOPIC_PUB(__VA_ARGS__)
#define _DO_MAKE_TOPIC_SUB(...)

#define _MAKE_TOPIC_PUB(name, msg, mode) _DO_MAKE_TOPIC_##mode(name, msg)

#define _MAKE_TOPIC_SUB0(...) _MAKE_TOPIC_SUB __VA_ARGS__
#define _MAKE_TOPIC_PUB0(...) _MAKE_TOPIC_PUB __VA_ARGS__

#define TOPICS(...) \
  constexpr int _topics_sub_start = __COUNTER__ + 1; \
  bool _topics_check_msg(int id, msg2struct::InIterator iter) { \
    switch (id) { MAP(_MAKE_TOPIC_SUB0, __VA_ARGS__) default: return false;} \
  } \
  constexpr int _topics_pub_start = __COUNTER__ + 1; \
  MAP(_MAKE_TOPIC_PUB0, __VA_ARGS__) \
  struct TopicsSub {}



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