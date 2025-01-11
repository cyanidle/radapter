#define __LITTLE_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#include "msg2struct.hpp"
#include "slipa.hpp"
#include "rad_topics.hpp"

void Log(const char* log, bool err = false);
void Error(const char* log);

//////////////////////

MAKE_MSG(LogMsg,
  (msg2struct::String) data
);

MAKE_MSG(LedCmd,
  (unsigned) power
);

MAKE_MSG(Responce,
  (uint32_t) id,
  (bool) ok
);

MAKE_MSG(Request, 
  (uint32_t) id
);

MAKE_MSG_INHERIT(Request, Gamble,
  (int32_t) amount
);

MAKE_MSG_INHERIT(Responce, Payout,
  (int32_t) amount
);

TOPICS(
  (log, LogMsg, PUB),
  (error, LogMsg, PUB),
  (led, LedCmd, SUB),
  (gamble_req, Gamble, SUB),
  (gamble_resp, Payout, PUB)
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

void topics::on_led(LedCmd& cmd) {
  Log("New Power!");
  led.power = 360 - (cmd.power > 360 ? 360 : cmd.power);
}

void topics::on_gamble_req(Gamble &gamble) {
  char buff[50];
  sprintf(buff, "GAMBLE!!! %lu", gamble.amount);
  Log(buff);
  Payout resp;
  resp.id = gamble.id;
  resp.ok = true;
  auto r = random();
  if (r < RANDOM_MAX / 3) {
    resp.amount = 1 * gamble.amount;
  } else if (r < RANDOM_MAX / 10) {
    resp.amount = 3 * gamble.amount;
  } else {
    resp.amount = -1 * gamble.amount;
  }
  topics::send_gamble_resp(resp);
}

//////////////////////

void setup() {
  // put your setup code here, to run once:
  Serial.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);
  Log("### Reset!");
}

namespace input {

unsigned char buffer[400];
unsigned char out_buffer[400];
int ptr = 0;
bool escaped = false;
bool error = false;

}

msg2struct::OutIterator topics_send_begin() {
  return {input::out_buffer, sizeof(input::out_buffer)};
}

void topics_send_finish(msg2struct::OutIterator iter) {
  auto count = iter.Written();
  slipa::Write(msg2struct::String{(const char*)input::out_buffer, count}, [&](msg2struct::String part){
    Serial.write(part.str, part.size);
  });
  Serial.write(slipa::END);
  Serial.flush();
}


void inputError(const char* err) {
  Error("Input Err: Reset input! Details:");
  Error(err);
  input::ptr = 0;
  input::error = true;
}

void loop() {
  led.tick();
  int _ch;
  while((_ch = Serial.read()) >= 0) {
    char ch = (char)_ch;
    if (input::error && ch == slipa::END) {
      input::error = false;
      continue;
    }
    if (input::ptr == sizeof(input::buffer)) {
      inputError("Buffer Overflow");
      continue;
    }
    if (input::escaped) {
      input::escaped = false;
      if (ch == slipa::ESC_END) {
        input::buffer[input::ptr++] = slipa::END;
      } else if (ch == slipa::ESC_ESC) {
        input::buffer[input::ptr++] = slipa::ESC;
      } else {
        inputError("Invalid escape");
        continue;
      }
    } else if (ch == slipa::END) {
      topics_receive_msg(msg2struct::InIterator(input::buffer, input::ptr));
      input::ptr = 0;
    } else if (ch == slipa::ESC) {
      input::escaped = true;
    } else {
      input::buffer[input::ptr++] = ch;
      input::escaped = false;
    }
  }
}

void Log(const char* log, bool err) {
  LogMsg msg;
  msg.data = {log, strlen(log)};
  !err ? topics::send_log(msg) : topics::send_error(msg);
}
void Error(const char* msg) {
  Log(msg, true);
}

void topics_error(const char* err) {
  char buff[50];
  sprintf(buff, "Topic error: %s", err);
  Error(buff);
}
