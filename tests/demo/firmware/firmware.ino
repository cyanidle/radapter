#define __LITTLE_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#include "msg2struct.hpp"
#include "slipa.hpp"
#include "msgs.hpp"

void log(const char* log);

struct LedTicker {
  unsigned power = 100;
  long lastFlip = 0;
  bool state = false;

  void tick();
};

LedTicker led;

//////////////////////


MAKE_MSG(LogMsg, 0,
  (msg2struct::String) data
);

MAKE_MSG(LedCmd, 1,
  (unsigned) power
) {
  log("New Power!");
  led.power = 360 - (power > 360 ? 360 : power);
}

//////////////////////


void LedTicker::tick() {
  auto ms = millis();
  auto diff = ms - lastFlip;
  if (diff > power * 3) {
    digitalWrite(LED_BUILTIN, state = !state);
    lastFlip = ms;
  }
}


//////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);
  log("### Reset!");
}

char buffer[400];
char out_buffer[400];
int ptr = 0;
bool escaped = false;
bool error = false;

#define CHECK_MSG(T) \
case T::ID: { \
  T m; \
  if (Parse(m, iter)) m.handle(); \
  else log("Not a msg of type: "), log(#T); \
  break; \
}

void outSlipReady(size_t count) {
    slipa::Write(msg2struct::String{out_buffer, count}, [&](msg2struct::String part){
      Serial.write(part.str, part.size);
    });
    Serial.write(slipa::END);
    Serial.flush();
}

msg2struct::OutIterator prepareMsg(int id) {
  msg2struct::OutIterator iter(out_buffer, sizeof(out_buffer));
  iter.BeginArray(2);
  iter.WriteInteger(LogMsg::ID);
  return iter;
}

template<typename T>
void sendMsg(const T& msg) {
  auto iter = prepareMsg(msg.ID);
  if (Dump(msg, iter)) {
    outSlipReady(iter.Written());
  }
}

void slipReady() {
  msg2struct::InIterator iter(buffer, ptr);
  size_t arr;
  int id;
  if (!iter.GetArraySize(arr) || arr < 2 || !iter.GetInteger(id)) {
    log("Error: expected a pair (array) of id + msg");
    return;
  }
  switch (id) {
  CHECK_MSG(LedCmd)
  default: {
    char temp[50];
    sprintf(temp, "Error: Unknown msg type: %d", id);
    log(temp);
  }
  }
}

void inputError(const char* err) {
  log("Input Err: Reset input! Details:");
  log(err);
  ptr = 0;
  error = true;
}

void loop() {
  led.tick();
  int _ch;
  while((_ch = Serial.read()) >= 0) {
    char ch = (char)_ch;
    if (error && ch == slipa::END) {
      error = false;
      continue;
    }
    if (ptr == sizeof(buffer)) {
      inputError("Buffer Overflow");
      continue;
    }
    if (escaped) {
      escaped = false;
      if (ch == slipa::ESC_END) {
        buffer[ptr++] = slipa::END;
      } else if (ch == slipa::ESC_ESC) {
        buffer[ptr++] = slipa::ESC;
      } else {
        inputError("Invalid escape");
        continue;
      }
    } else if (ch == slipa::END) {
      slipReady();
      ptr = 0;
    } else if (ch == slipa::ESC) {
      escaped = true;
    } else {
      buffer[ptr++] = ch;
      escaped = false;
    }
  }
}

void log(const char* log) {
  LogMsg msg;
  msg.data = {log, strlen(log)};
  sendMsg(msg);
}
