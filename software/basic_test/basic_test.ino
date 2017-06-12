/*
ServoEncoder
TEST PROGRAM

https://github.com/OpenTransat/ServoEncoder

Released under the Creative Commons Attribution ShareAlike 4.0 International License
https://creativecommons.org/licenses/by-sa/4.0/
*/

#define SERIAL_DEBUG //switch between serial debug and motor
#define LED 14 //PC0
#define PIN_CS 15 //PC1, EMS:6
#define PIN_CLK 17 //PC3, EMS:2
#define PIN_DO 16 //PC2, EMS:4
#define PIN_ON 0 //PD0
#define PIN_REV 1 //PD1
#define PIN_PWM 2 //PD2
#define PIN_FB 3 //PD3
#define ZERO_POS 30
#define DELTA 70

void setup() {
  pinMode(LED, OUTPUT);

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_CLK, OUTPUT);
  digitalWrite(PIN_CLK, HIGH);
  pinMode(PIN_DO, INPUT);

  #ifdef SERIAL_DEBUG
    Serial.begin(9600); //9600 supported for 2MHz
  #else
    pinMode(PIN_ON, OUTPUT);
    digitalWrite(PIN_ON, HIGH);
    pinMode(PIN_REV, OUTPUT);
    digitalWrite(PIN_REV, HIGH);
  #endif
}

byte dir = 1;

void gotopos(int where, int where_stop) {
  digitalWrite(PIN_CS, LOW);
  delayMicroseconds(1);  
  byte b[16];
  int pos = 0;
  byte parity = 0;

  for (int i = 0; i < 16; i++) {
    digitalWrite(PIN_CLK, LOW);
    delayMicroseconds(1);
    digitalWrite(PIN_CLK, HIGH);
    b[i] = digitalRead(PIN_DO) == HIGH ? 1 : 0;

    if (i < 15)
      parity += b[i];
  }

  parity %= 2;

  if (b[15] == parity) {

    for (int i = 0; i < 10; i++) {
      pos += b[i] * (1 << 10-(i+1));
    }

    pos = (pos - ZERO_POS) % 1024;
    if (pos > 512)
      pos = pos - 1024;

    if (pos < where && pos < where_stop) {
      #ifdef SERIAL_DEBUG
        Serial.println("go forward");
      #else
        digitalWrite(PIN_ON, LOW);
        digitalWrite(PIN_REV, LOW);
      #endif
    } else if (pos > where && pos > where_stop) {
      #ifdef SERIAL_DEBUG
        Serial.println("go back");
      #else
        digitalWrite(PIN_ON, LOW);
        digitalWrite(PIN_REV, HIGH);
      #endif
    }
    else if (pos > where && pos < where_stop) {
      #ifdef SERIAL_DEBUG
      #else
        digitalWrite(PIN_ON, HIGH);      
      #endif
    }
    else if (pos < where && pos > where_stop) {
      #ifdef SERIAL_DEBUG
      #else
        digitalWrite(PIN_ON, HIGH);      
      #endif
    }
  
    #ifdef SERIAL_DEBUG
      Serial.println(pos);
    #endif
  }
  else {
    digitalWrite(PIN_CS, HIGH);
    #ifdef SERIAL_DEBUG
      Serial.println("parity error!");
    #else
      digitalWrite(PIN_ON, HIGH);
      digitalWrite(PIN_REV, HIGH);
    #endif
    delay(1000);
  }
  digitalWrite(PIN_CS, HIGH);
}

unsigned long previousMillis = 0;
#define INTERVAL 1000UL
byte state = 0;

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis < previousMillis) { //millis() overflow
    previousMillis = 0;
  }
  
  if (currentMillis - previousMillis >= INTERVAL) {
    previousMillis = currentMillis;
    state = 1 - state;
  }

  if (state)
    gotopos(DELTA, DELTA+50);
  else
    gotopos(-DELTA, -DELTA-50);
}

