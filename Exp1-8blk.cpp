int leds[] = {2,3,4,5,6,7,8,9};
int sw = 10;

void setup() {
  for(int i=0; i<8; i++) {
    pinMode(leds[i], OUTPUT);
  }
  pinMode(sw, INPUT);
}

void loop() {
  int state = digitalRead(sw);

  // a. All LEDs ON
  for(int i=0; i<8; i++) {
    digitalWrite(leds[i], HIGH);
  }

  // b & c when switch is ON
  if(state == HIGH) {

    // b. Blinking LEDs
    for(int i=0; i<8; i++)
      digitalWrite(leds[i], HIGH);
    delay(500);
    for(int i=0; i<8; i++)
      digitalWrite(leds[i], LOW);
    delay(500);

    // c. Alternate blinking LEDs
    for(int i=0; i<8; i+=2)
      digitalWrite(leds[i], HIGH);
    for(int i=1; i<8; i+=2)
      digitalWrite(leds[i], LOW);
    delay(500);

    for(int i=0; i<8; i+=2)
      digitalWrite(leds[i], LOW);
    for(int i=1; i<8; i+=2)
      digitalWrite(leds[i], HIGH);
    delay(500);
  }
}