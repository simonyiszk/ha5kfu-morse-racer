const int BUTTON_PINS[] = {
  D6, D5, D2, D1
};

typedef struct btn_state {
  bool state;
  unsigned long update_time;
} btn_state;

btn_state states[4];

const unsigned long DEBOUNCE_TIME = 50; // ms
unsigned long LONG_TIME = 250; // ms

void setup() {
  // put your setup code here, to run once:
  for (int i = 0; i<4; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }
  Serial.begin(9600);
  Serial.setTimeout(200);
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long t = millis(); 
  for (int i = 0; i<4; i++) {
    bool pushed = !digitalRead(BUTTON_PINS[i]);
    if (pushed != states[i].state) {
      if (t - states[i].update_time > DEBOUNCE_TIME) {
        if (!pushed) {
          if (t - states[i].update_time >= LONG_TIME) {
            Serial.write('U');
            Serial.write('0'+i);
            Serial.write(':');
            Serial.println('L');
          } else {
            Serial.write('U');
            Serial.write('0'+i);
            Serial.write(':');
            Serial.println('S');
          }
        }
        states[i].state = pushed;
        states[i].update_time = t;
      }
    }
  }
}