// Servo test: no EMG sensor needed. Each servo closes and opens on its own (1, 2, 3),
// then all three close and open together 5 times. Repeats forever.
// Pins and angles are copied from somatic_arm.ino: keep them the same.
#include <Servo.h>

const byte SERVO_PINS[3]  = {9, 6, 5};
const int OPEN_ANGLE[3]   = {0, 0, 0};
const int CLOSED_ANGLE[3] = {90, 90, 90};
const unsigned long STAGGER_MS = 100;  // servos start 100 ms apart to spread current spikes

Servo servos[3];

void moveAll(bool close) {
  for (int i = 0; i < 3; i++) {
    servos[i].write(close ? CLOSED_ANGLE[i] : OPEN_ANGLE[i]);
    delay(STAGGER_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("servo test start");  // printed again mid-test = the Uno reset (weak AA cells or wiring)
  for (int i = 0; i < 3; i++) {
    servos[i].write(OPEN_ANGLE[i]);  // set before attach, so the servo does not jump
    servos[i].attach(SERVO_PINS[i]);
    delay(STAGGER_MS);
  }
  delay(1000);
}

void loop() {
  for (int i = 0; i < 3; i++) {
    Serial.print("servo ");
    Serial.print(i + 1);
    Serial.println(": close");
    servos[i].write(CLOSED_ANGLE[i]);
    delay(1000);
    Serial.print("servo ");
    Serial.print(i + 1);
    Serial.println(": open");
    servos[i].write(OPEN_ANGLE[i]);
    delay(1000);
  }
  for (int n = 0; n < 5; n++) {
    Serial.println("all: close");
    moveAll(true);
    delay(1000);
    Serial.println("all: open");
    moveAll(false);
    delay(1000);
  }
}
