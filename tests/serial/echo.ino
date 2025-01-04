void setup() {
  Serial.begin(57600);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.setTimeout(300);
}

template<typename T, size_t N> constexpr size_t size(const T(&)[N]) {return N;}

char buff[100];
int ptr = 0;

void loop() {
  while(true) {
    auto ch = Serial.read();
    if (ch < 0) break;
    if (ptr == size(buff)) ptr--;
    buff[ptr++] = (char)ch;
    if (ch == 0xc0) {
      Serial.write(buff, ptr);
      ptr = 0;
      break;
    }
  }
}
