// DC Motor control with transistor + PWM
int motorPin = 9;  // PWM pin connected to transistor base

void setup() {
  pinMode(motorPin, OUTPUT);
}

void loop() {
  // Max RPM (255 = full duty cycle)
  analogWrite(motorPin, 255);
  delay(5000);

  // Slowest RPM (small duty cycle, e.g., 50)
  analogWrite(motorPin, 50);
  delay(5000);
}