// Servo zero: for fitting the pulleys. No EMG sensor needed.
// All three servos go to the open angle (0°) and stay there, so each pulley can be pushed on.
// 3 s after power-on or reset, servo 1, then 2, then 3 turns a little toward closed and back:
// that shows which way "close" turns. Press the Uno's reset button to see it again.
// Pins and angles are copied from somatic_arm.ino: keep them the same.
#include <Servo.h>

const byte SERVO_PINS[3]  = {9, 6, 5};     // 1 thumb, 2 index, 3 middle+ring+little
const int OPEN_ANGLE[3]   = {0, 0, 0};
const int CLOSED_ANGLE[3] = {90, 90, 90};

Servo servos[3];

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 3; i++) {
    servos[i].write(OPEN_ANGLE[i]);  // set before attach, so the servo does not jump
    servos[i].attach(SERVO_PINS[i]);
    delay(100);
  }
  Serial.println("all servos at open: fit the pulleys");
  delay(3000);
  for (int i = 0; i < 3; i++) {
    Serial.print("servo ");
    Serial.print(i + 1);
    Serial.println(": turning toward closed and back");
    servos[i].write(OPEN_ANGLE[i] + (CLOSED_ANGLE[i] - OPEN_ANGLE[i]) / 3);
    delay(1000);
    servos[i].write(OPEN_ANGLE[i]);
    delay(1000);
  }
}

void loop() {}
