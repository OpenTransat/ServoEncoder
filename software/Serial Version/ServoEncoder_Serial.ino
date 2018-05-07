/*
ServoEncoder Serial Version
https://github.com/OpenTransat/ServoEncoder

Released under the Creative Commons Attribution ShareAlike 4.0 International License
https://creativecommons.org/licenses/by-sa/4.0/
*/
#include <avr/wdt.h>

#define SERIAL_DEBUG
#define RUN_MOTOR
#define LED 14 //PC0
#define PIN_CS 15 //PC1, EMS:6
#define PIN_CLK 17 //PC3, EMS:2
#define PIN_DO 16 //PC2, EMS:4
#define PIN_ON 2 //active LOW
#define PIN_REV 3 //active LOW
#define ENC_ZERO 127
#define ENC_MAXDIF 100
#define ENC_MIN (ENC_ZERO - ENC_MAXDIF) //max left
#define ENC_MAX (ENC_ZERO + ENC_MAXDIF) //max right
#define MINCHANGE 1 //minimum PWM change to prevent flickering
#define CORRECTION_LEFT (+11)
#define CORRECTION_RIGHT (-10)

#define FB_OK 47
#define FB_TIMEOUT 123
#define TIMEOUT_MS 4000UL

#define PWM_PULSES 2
#define PWM_VALID_THRESHOLD 0 //all PWM_PULSES pulses in the pwm_pulse[] array must be equal within the PWM_VALID_THRESHOLD
unsigned char pwm_pulse[PWM_PULSES];
unsigned char pwm_index = 0;

unsigned char pulse;
int enc_goto;

void serialRead() {
  while (Serial.available()) {
    pwm_index = (++pwm_index) % PWM_PULSES;
    pwm_pulse[pwm_index] = Serial.read();
  }  
}

void setup() {
  wdt_disable();
  
  pinMode(LED, OUTPUT);

  pinMode(PIN_ON, OUTPUT);
  digitalWrite(PIN_ON, HIGH);
  pinMode(PIN_REV, OUTPUT);
  digitalWrite(PIN_REV, HIGH);

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, LOW);
  pinMode(PIN_CLK, OUTPUT);
  digitalWrite(PIN_CLK, HIGH);
  pinMode(PIN_DO, INPUT);

  Serial.begin(9600); //9600 supported for 2MHz

  delay(1000L); //delay before watchdog to load bootloader https://bigdanzblog.wordpress.com/2014/10/24/arduino-watchdog-timer-wdt-example-code/
  wdt_enable(WDTO_8S); //call last!
}

int enc_read() { //output: 0..1023, -1 = parity error
  digitalWrite(PIN_CS, HIGH);
  delayMicroseconds(1);
  digitalWrite(PIN_CS, LOW);
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
    digitalWrite(PIN_CS, LOW);
    return pos;
  }
  digitalWrite(PIN_CS, LOW);
  return -1;
}

bool validPulse() {
  for (int i = 0; i < PWM_PULSES; i++) {
    if (abs((int)pwm_pulse[i] - (int)pwm_pulse[(i+1)%PWM_PULSES]) > PWM_VALID_THRESHOLD) {
      return false;
    }
  }
  return true;
}

bool isNewPulse() {
  if (!validPulse())
    return false;
  pulse = pwm_pulse[PWM_PULSES-1]; //save pwm_pulse from interrupts
//  pulse = ((millis() / 10000) % 4) * 500 + 1000; if (pulse > 2000) pulse = 1500; //simulation

  static unsigned char prev_pulse;

  #ifdef SERIAL_DEBUG
//    Serial.println(pulse);
  #endif

  if (pulse < (int)prev_pulse - MINCHANGE || pulse > (int)prev_pulse + MINCHANGE) { //pulse is outside the safe interval
    prev_pulse = pulse;
    return true;
  }
  
  return false;
}

enum tstate {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_NEWPULSE, //move interrupted by a new pulse
  STATE_TIMEOUT
};

tstate state = STATE_IDLE;

void loop() {
  wdt_reset();
  serialRead();
  if (isNewPulse() || state != STATE_IDLE) {
    int enc_goto = map(pulse, 0, 255, ENC_MIN, ENC_MAX);
    enc_goto = max(enc_goto, ENC_MIN);
    enc_goto = min(enc_goto, ENC_MAX);
    int enc_pos = enc_read();
    if (enc_pos > 0) { //encoder parity check OK
      #ifdef SERIAL_DEBUG
        Serial.print(pulse);
        Serial.print("\t");
        Serial.print(enc_goto);
        Serial.print("\t");
        Serial.print(enc_pos);
      #endif
      if (enc_pos > enc_goto) { //turn left
        state = STATE_RUNNING;
        #ifdef SERIAL_DEBUG
          Serial.println("\tturn left");
        #endif
        
        motor_left();
        unsigned long timeout_start = millis();
        
        while ((enc_pos = enc_read()) > enc_goto + CORRECTION_LEFT) {
          wdt_reset();
          serialRead();
          if (isNewPulse()) {
            #ifdef SERIAL_DEBUG
              Serial.println("Left: Interrupted by a new pulse");
            #endif
            state = STATE_NEWPULSE;
            break;
          }

          if ((unsigned int)(millis() - timeout_start) > TIMEOUT_MS) {
            state = STATE_TIMEOUT;
            break;
          }
        }
        if (state == STATE_TIMEOUT) {
          motor_stop();
          #ifdef SERIAL_DEBUG
            Serial.println("Left: Timeout");
          #endif
          Serial.write(FB_TIMEOUT);
          state = STATE_IDLE;          
        }
        else if (state != STATE_NEWPULSE) {
          motor_stop();
          #ifdef SERIAL_DEBUG
            Serial.println("Left: Stop");
          #endif
          Serial.write(FB_OK);
          state = STATE_IDLE;
        }
      }
      else if (enc_pos < enc_goto) { //turn right
        state = STATE_RUNNING;
        #ifdef SERIAL_DEBUG
          Serial.println("\tturn right");
        #endif
        
        motor_right();
        unsigned long timeout_start = millis();
        
        while ((enc_pos = enc_read()) < enc_goto + CORRECTION_RIGHT) {
          wdt_reset();
          serialRead();
          if (isNewPulse()) { //TO BE TESTED
            #ifdef SERIAL_DEBUG
              Serial.println("Right: Interrupted by a new pulse");
            #endif
            state = STATE_NEWPULSE;
            break;
          }

          if ((unsigned int)(millis() - timeout_start) > TIMEOUT_MS) {
            state = STATE_TIMEOUT;
            break;
          }
        }
        if (state == STATE_TIMEOUT) {
          motor_stop();
          #ifdef SERIAL_DEBUG
            Serial.println("Left: Timeout");
          #endif
          Serial.write(FB_TIMEOUT);
          state = STATE_IDLE;          
        }
        else if (state != STATE_NEWPULSE) {
          motor_stop();
          #ifdef SERIAL_DEBUG
            Serial.println("Left: Stop");
          #endif
          Serial.write(FB_OK);
          state = STATE_IDLE;
        }
      }
      else { //equal position => stop the motor
        motor_stop();
        state = STATE_IDLE;

        #ifdef SERIAL_DEBUG
          Serial.println("Equal: Stop");
        #endif
      }
    }
    else { //bad encoder parity check => stop the motor
      motor_stop();
      state = STATE_IDLE;      

      #ifdef SERIAL_DEBUG
        Serial.println("bad parity");
      #endif
    }
  }
}

void motor_stop() {
    digitalWrite(PIN_ON, HIGH);
    digitalWrite(PIN_REV, HIGH);
}

void motor_right() {
  #ifdef RUN_MOTOR
    digitalWrite(PIN_REV, HIGH);
    digitalWrite(PIN_ON, LOW);
  #endif
}

void motor_left() {
  #ifdef RUN_MOTOR
    digitalWrite(PIN_REV, LOW);
    digitalWrite(PIN_ON, LOW);
  #endif
}

