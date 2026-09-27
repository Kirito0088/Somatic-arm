// Somatic Arm: one EMG sensor on A0 opens and closes 3 servos.
#include <Servo.h>

const byte EMG_PIN = A0;
const byte SERVO_PINS[3]  = {9, 6, 5};
const int OPEN_ANGLE[3]   = {0, 0, 0};     // set per servo when fitting the hand
const int CLOSED_ANGLE[3] = {90, 90, 90};  // set per servo when fitting the hand, a few degrees at a time
const unsigned long STAGGER_MS  = 100;     // servos start 100 ms apart to spread current spikes
const unsigned long MAX_HOLD_MS = 10000;   // closed longer than this: open (loose electrode or hot servo)
const int MIN_SPAN = 50;                   // squeeze must read at least this much above rest

Servo servos[3];
float level = 0;            // smoothed EMG reading, 0-1023
int openAt, closeAt;
bool closed = false;
bool locked = false;        // after calibration or a forced open, wait until the arm relaxes
unsigned long closedSince = 0;

// Also prints during calibration, so the Serial Plotter shows the signal even when calibration keeps failing.
void readLevel() {
  level += 0.1 * (analogRead(EMG_PIN) - level);
  delay(5);

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 20) {
    lastPrint = millis();
    Serial.print("emg:");
    Serial.print((int)level);
    Serial.print(" close:");
    Serial.print(closeAt);
    Serial.print(" open:");
    Serial.println(openAt);
  }
}

// Average level over ms (relax), or peak level while blinking the LED (squeeze).
int measure(unsigned long ms, bool squeeze) {
  unsigned long start = millis();
  float sum = 0, peak = 0;
  long n = 0;
  while (millis() - start < ms) {
    readLevel();
    if (squeeze) digitalWrite(LED_BUILTIN, (millis() / 250) % 2);
    sum += level;
    n++;
    if (level > peak) peak = level;
  }
  return squeeze ? peak : sum / n;
}

// LED solid = relax (3 s), blinking = squeeze hard (3 s), off = done.
// Fast blinking = signal too weak, it tries again.
void calibrate() {
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    int rest = measure(3000, false);
    int squeeze = measure(3000, true);
    digitalWrite(LED_BUILTIN, LOW);
    if (squeeze - rest >= MIN_SPAN) {
      closeAt = rest + (squeeze - rest) / 2;
      openAt  = rest + (squeeze - rest) / 4;
      return;
    }
    for (int i = 0; i < 20; i++) {
      digitalWrite(LED_BUILTIN, i % 2);
      delay(100);
    }
  }
}

void moveAll(bool close) {
  for (int i = 0; i < 3; i++) {
    servos[i].write(close ? CLOSED_ANGLE[i] : OPEN_ANGLE[i]);
    delay(STAGGER_MS);
  }
  closed = close;
  closedSince = millis();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    servos[i].write(OPEN_ANGLE[i]);  // set before attach, so the servo does not jump
    servos[i].attach(SERVO_PINS[i]);
    delay(STAGGER_MS);
  }
  // Wiring check: servo 1, then 2, then 3 moves a quarter of the way to closed and back.
  for (int i = 0; i < 3; i++) {
    servos[i].write(OPEN_ANGLE[i] + (CLOSED_ANGLE[i] - OPEN_ANGLE[i]) / 4);
    delay(400);
    servos[i].write(OPEN_ANGLE[i]);
    delay(400);
  }
  level = analogRead(EMG_PIN);
  calibrate();
  locked = true;  // the calibration squeeze may still be on: don't close until the arm relaxes
}

void loop() {
  readLevel();
  if (locked) {
    if (level < openAt) locked = false;
  } else if (!closed && level > closeAt) {
    moveAll(true);
  } else if (closed && level < openAt) {
    moveAll(false);
  } else if (closed && millis() - closedSince > MAX_HOLD_MS) {
    moveAll(false);
    locked = true;
  }
}
