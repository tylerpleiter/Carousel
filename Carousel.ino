
#include "TimerOne.h"
#include "MD_YX5300.h"

#define STEPPER_MIN   300    // mHz 
#define STEPPER_MAX   3000   // mHz

#define LOOP_DELAY_MS 10
#define LED_CYCLES    50
#define RUN_CYCLES    4900  // 20s
#define LOCK_CYCLES   6000  // 60s

#define MICROSTEPS    16
#define STEPS_PER_REV 200


#define PIR_PIN     3
#define SPD_PIN     A0
#define VOL_PIN     A1

#define MODE_PIN    19
#define MAN_PIN     2

#define STEP_PUL    9
#define STEP_DIR    10
#define STEP_ENA    11

#define LED_BANK_X  18
#define LED_BANK_1  4
#define LED_BANK_2  5
#define LED_BANK_3  6
#define LED_BANK_4  7
#define LED_BANK_5  8
#define LED_BANK_6  12
#define LED_BANK_7  13
#define LED_BANK_8  16
#define LED_BANK_9  17

typedef enum {WAIT = 0, START, RUN, STOP, TIMEOUT};


int led_array[] = {LED_BANK_1, LED_BANK_2, LED_BANK_3, LED_BANK_4, LED_BANK_5, LED_BANK_6, LED_BANK_7, LED_BANK_8, LED_BANK_9};
int led_seq[] = {0, 1, 2, 3, 4, 5};



int sensor = 0;
int state = WAIT;
int led_step, led_delay, run_ctr, lock_ctr, vol_last, vol_cur, mode;
unsigned long stepper_speed, max_speed;

MD_YX5300 mp3(Serial);

// Setting Darlington high should enable LEDs
void ledSet(uint8_t set) {
  digitalWrite(LED_BANK_X, set);
  digitalWrite(LED_BANK_1, set);
  digitalWrite(LED_BANK_2, set);
  digitalWrite(LED_BANK_3, set);
  digitalWrite(LED_BANK_4, set);
  digitalWrite(LED_BANK_5, set);
  digitalWrite(LED_BANK_6, set);
  digitalWrite(LED_BANK_7, set);
  digitalWrite(LED_BANK_8, set);
  digitalWrite(LED_BANK_9, set);
}

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(SPD_PIN, INPUT);
  pinMode(VOL_PIN, INPUT);
  
  pinMode(MODE_PIN, INPUT_PULLUP);
  pinMode(MAN_PIN, INPUT_PULLUP);
  
  pinMode(STEP_PUL, OUTPUT);
  pinMode(STEP_DIR, OUTPUT);
  pinMode(STEP_ENA, OUTPUT);
  
  pinMode(LED_BANK_X, OUTPUT);
  pinMode(LED_BANK_1, OUTPUT);
  pinMode(LED_BANK_2, OUTPUT);
  pinMode(LED_BANK_3, OUTPUT);
  pinMode(LED_BANK_4, OUTPUT);
  pinMode(LED_BANK_5, OUTPUT);
  pinMode(LED_BANK_6, OUTPUT);
  pinMode(LED_BANK_7, OUTPUT);
  pinMode(LED_BANK_8, OUTPUT);
  pinMode(LED_BANK_9, OUTPUT);

  // digitalWrite(LED_BANK_X, 0);  // ON? -- no, high should be on
  ledSet(1);
  delay(500);
  ledSet(0);

  digitalWrite(STEP_DIR, 1); // check
  digitalWrite(STEP_ENA, 0); // check

  Timer1.initialize(100);  // uS
  Timer1.pwm(STEP_PUL, 900); // 900/1023 duty cycle
  Timer1.stop();

  Serial.begin(9600);

  // MP3Stream.begin(MD_YX5300::SERIAL_BPS);
  vol_cur = 0, vol_last = 0;
  mp3.begin();
  mp3.setSynchronous(false);
  vol_cur = map(analogRead(VOL_PIN), 0, 1023, 0, mp3.volumeMax());
  mp3.volume(vol_cur);
  vol_last = vol_cur;

  lock_ctr = LOCK_CYCLES;
  
}

void loop() {

  switch(state) {
    case WAIT:
      if (digitalRead(MODE_PIN) == 0) {
        // Automatic mode
        if (digitalRead(PIR_PIN) && lock_ctr > LOCK_CYCLES) {
          state = START;
          stepper_speed = STEPPER_MIN;
          Timer1.restart();
          Timer1.setPeriod(1000);
          ledSet(1); // on
        }
      } else {
        // Manual mode
        if (digitalRead(MAN_PIN) == 0) {
          state = START;
          stepper_speed = STEPPER_MIN;
          Timer1.restart();
          Timer1.setPeriod(1000);
          ledSet(1); // on
        }
      }
      break;
      
    case START:
      // Ramp up stepper motor
      digitalWrite(STEP_ENA, 1);
      stepper_speed+=10;
      Timer1.setPeriod((1000000000 / (stepper_speed * 200 * 16)));
      max_speed = map(analogRead(SPD_PIN), 0, 1024, STEPPER_MIN, STEPPER_MAX);
      if (stepper_speed >= max_speed) {
        state = RUN;
        led_step = 0; led_delay = 0;
        run_ctr = 0;
        // start music
        mp3.playNext();
        mp3.repeat(true);
      }
      break;
      
    case RUN:
      // Set stepper speed
      stepper_speed = map(analogRead(SPD_PIN), 0, 1024, STEPPER_MIN, STEPPER_MAX);
      Timer1.setPeriod((1000000000 / (stepper_speed * 200 * 16)));

      // Flash lights in sequence
      led_delay++;
      if (led_delay >= LED_CYCLES) {
        led_delay = 0;

        for (int i = 0; i < sizeof(led_array)/sizeof(int); i++) {
          digitalWrite(led_array[i], 1);
        }
        digitalWrite(led_array[led_seq[led_step]], 0);
        
        led_step++;
        if (led_step > sizeof(led_seq)/sizeof(led_seq[0]) - 1) led_step = 0;
        
      }

      // Volume control
      vol_cur = map(analogRead(VOL_PIN), 0, 1023, 0, mp3.volumeMax());
      if (vol_cur != vol_last) {
        mp3.volume(vol_cur);
        vol_last = vol_cur;
      }

      // Reset run timer if PIR pin detected
      run_ctr++;
//      if (digitalRead(PIR_PIN)) run_ctr = 0;
      if (run_ctr >= RUN_CYCLES) {
        for (int i = 0; i < sizeof(led_array)/sizeof(int); i++) {
          digitalWrite(led_array[i], 1);
        }
        state = STOP;
        // stop music
        mp3.playStop();
      }
      break;

    case STOP:
      // Ramp down stepper motor
      stepper_speed-=10;
      Timer1.setPeriod((1000000000 / (stepper_speed * 200 * 16)));
      if (stepper_speed <= STEPPER_MIN) {
        state = WAIT;
        Timer1.stop();
        digitalWrite(STEP_ENA, 0);
        ledSet(0); // off
      }
      lock_ctr = 0;
      break;
  }
  mp3.check();
  if (lock_ctr < LOCK_CYCLES + 10) lock_ctr++;
  delay(LOOP_DELAY_MS);
  
}
