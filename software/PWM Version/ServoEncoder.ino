/*
ServoEncoder
https://github.com/OpenTransat/ServoEncoder

Released under the Creative Commons Attribution ShareAlike 4.0 International License
https://creativecommons.org/licenses/by-sa/4.0/
*/

#define SERIAL_DEBUG //switch between serial debug and motor
#define LED 14 //PC0
#define PIN_CS 15 //PC1, EMS:6
#define PIN_CLK 17 //PC3, EMS:2
#define PIN_DO 16 //PC2, EMS:4
#define PIN_ON 0 //PD0 = Serial RXD, active LOW
#define PIN_REV 1 //PD1 = Serial TXD, active LOW
#define PIN_PWM 2 //PD2
#define PIN_FB 3 //PD3
#define ENC_ZERO 127
#define ENC_MAXDIF 100
#define ENC_MIN (ENC_ZERO - ENC_MAXDIF) //max left
#define ENC_MAX (ENC_ZERO + ENC_MAXDIF) //max right
#define MINCHANGE 0 //minimum PWM change to prevent flickering, 32 is a typical measurement error
#define CORRECTION_LEFT (+11)
#define CORRECTION_RIGHT (-10)

#define PWM_PULSES 3
#define PWM_VALID_THRESHOLD 65 //all PWM_PULSES pulses in the pwm_pulse[] array must be equal within the PWM_VALID_THRESHOLD
volatile unsigned long pwm_timer_start = 0;
volatile unsigned int pwm_pulse[PWM_PULSES];
volatile unsigned char pwm_index = 0;
#define PWM_TIMEOUT 2500

int pulse;
int enc_goto;

void calcRC() { //blocking max 2.5ms
  if(digitalRead(PIN_PWM) == HIGH) { 
    pwm_timer_start = micros();
    unsigned int pwm_length;
    while(digitalRead(PIN_PWM) == HIGH) {
      pwm_length = (unsigned int)(micros() - pwm_timer_start); //negative number converted OK on micros() overflow
      if (pwm_length > PWM_TIMEOUT) //timeout
        return;
    }
    pwm_index = (++pwm_index) % PWM_PULSES;
    pwm_pulse[pwm_index] = (unsigned int)(micros() - pwm_timer_start);
  }
}

/*
void calcRC() { //non-blocking
  if(digitalRead(PIN_PWM) == HIGH) { 
    pwm_timer_start = micros();
  }
  else { 
    if(pwm_timer_start != 0) {
      pwm_index = (++pwm_index) % PWM_PULSES;
      pwm_pulse[pwm_index] = (unsigned int)(micros() - pwm_timer_start); //negative number converted OK on micros() overflow
      pwm_timer_start = 0;
    }
  }
}
*/

void setup() {
  pinMode(LED, OUTPUT);

  #ifdef SERIAL_DEBUG
    Serial.begin(9600); //9600 supported for 2MHz
  #else
    pinMode(PIN_ON, OUTPUT);
    digitalWrite(PIN_ON, HIGH);
    pinMode(PIN_REV, OUTPUT);
    digitalWrite(PIN_REV, HIGH);
  #endif

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, LOW);
  pinMode(PIN_CLK, OUTPUT);
  digitalWrite(PIN_CLK, HIGH);
  pinMode(PIN_DO, INPUT);

  pinMode(PIN_PWM, INPUT);
  pinMode(PIN_FB, OUTPUT);
  digitalWrite(PIN_FB, LOW);
  attachInterrupt(digitalPinToInterrupt(PIN_PWM), calcRC, CHANGE);
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

  static int prev_pulse;

  #ifdef SERIAL_DEBUG
//    Serial.print(pulse);
  #endif

  if (pulse < 900 || pulse > 2100) { //bad pulse
    return false;
  }

  if (pulse < prev_pulse - MINCHANGE || pulse > prev_pulse + MINCHANGE) { //pulse is outside the safe interval
    prev_pulse = pulse;
    return true;
  }
  
  return false;
}

enum tstate {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_NEWPULSE //move interrupted by a new pulse
};

tstate state = STATE_IDLE;

void loop() {
  if (isNewPulse() || state != STATE_IDLE) {
    int enc_goto = map(pulse, 1000, 2000, ENC_MIN, ENC_MAX);
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
        #else
          motor_left();
        #endif
        while ((enc_pos = enc_read()) > enc_goto + CORRECTION_LEFT) {
          /*if (isNewPulse()) { //TO BE TESTED
            #ifdef SERIAL_DEBUG
              Serial.println("Interrupted by a new pulse");
            #endif
            state = STATE_NEWPULSE;
            break;
          }*/
        }
        if (state != STATE_NEWPULSE) {
          #ifdef SERIAL_DEBUG
            Serial.println("Stop");
          #else
            motor_stop();
          #endif        
          state = STATE_IDLE;
        }
      }
      else if (enc_pos < enc_goto) { //turn right
        state = STATE_RUNNING;
        #ifdef SERIAL_DEBUG
          Serial.println("\tturn right");
        #else
          motor_right();
        #endif
        while ((enc_pos = enc_read()) < enc_goto + CORRECTION_RIGHT) {
          /*if (isNewPulse()) { //TO BE TESTED
            #ifdef SERIAL_DEBUG
              Serial.println("Interrupted by a new pulse");
            #endif
            state = STATE_NEWPULSE;
            break;
          }*/
        }
        if (state != STATE_NEWPULSE) {
          #ifdef SERIAL_DEBUG
            Serial.println("Stop");
          #else
            motor_stop();
          #endif        
          state = STATE_IDLE;
        }
      }
      else { //equal position => stop the motor
        #ifdef SERIAL_DEBUG
          Serial.println("Stop");
        #else
          motor_stop();
        #endif
        state = STATE_IDLE;
      }
    }
    else { //bad encoder parity check => stop the motor
      #ifdef SERIAL_DEBUG
        Serial.println("bad parity");
      #else
        motor_stop();
      #endif
      state = STATE_IDLE;      
    }
  }
}

void motor_stop() {
  digitalWrite(PIN_ON, HIGH);
  digitalWrite(PIN_REV, HIGH);
}

void motor_right() {
  digitalWrite(PIN_REV, HIGH);
  digitalWrite(PIN_ON, LOW);
}

void motor_left() {
  digitalWrite(PIN_REV, LOW);
  digitalWrite(PIN_ON, LOW);
}

