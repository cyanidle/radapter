#define __LITTLE_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#include "msg2struct.hpp"
#include "slipa.hpp"
#include "rad_topics.hpp"

void log(const char* log);

//////////////////////

MAKE_MSG(LogMsg,
  (msg2struct::String) data
);

MAKE_MSG(LedCmd,
  (unsigned) power
);

TOPICS(
  (log, LogMsg, PUB),
  (led, LedCmd, SUB)
);

//////////////////////

struct LedTicker {
  unsigned power = 100;
  long lastFlip = 0;
  bool state = false;

  void tick() {
    auto ms = millis();
    auto diff = ms - lastFlip;
    if (diff > power * 3) {
      digitalWrite(LED_BUILTIN, state = !state);
      lastFlip = ms;
  }
}
};

LedTicker led;

void on_led(LedCmd& cmd) {
  log("New Power!");
  led.power = 360 - (cmd.power > 360 ? 360 : cmd.power);
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

msg2struct::OutIterator topics_send_begin() {
  return {out_buffer, sizeof(out_buffer)};
}

void topics_send_finish(msg2struct::OutIterator iter) {
  auto count = iter.Written();
  slipa::Write(msg2struct::String{out_buffer, count}, [&](msg2struct::String part){
    Serial.write(part.str, part.size);
  });
  Serial.write(slipa::END);
  Serial.flush();
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
      topics_receive_msg(msg2struct::InIterator(buffer, ptr));
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
  send_log(msg);
}

void topics_error(const char* err) {
  char buff[50];
  sprintf(buff, "Topic error: %s", err);
  log(buff);
}
