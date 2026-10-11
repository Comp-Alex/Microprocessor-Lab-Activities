#include <Servo.h>

Servo myServo;

void setup() {
  myServo.attach(9);  // signal pin
}

void loop() {
  // Longest left
  myServo.write(0);
  delay(5000);

  // Longest right
  myServo.write(180);
  delay(5000);
}