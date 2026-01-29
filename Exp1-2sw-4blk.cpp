int ledA[] = {2,3,4,5};   // LED 1–4
int ledB[] = {6,7,8,9};   // LED 5–8

int sw1 = 10;             // Power switch

void setup() {
  for(int i=0; i<4; i++) {
    pinMode(ledA[i], OUTPUT);
    pinMode(ledB[i], OUTPUT);
  }
  pinMode(sw1, INPUT);
}

void loop() {
  int s1 = digitalRead(sw1);

  if(s1 == HIGH) {   // Power ON
    // Blink group A
    for(int i=0; i<4; i++) digitalWrite(ledA[i], HIGH);
    for(int i=0; i<4; i++) digitalWrite(ledB[i], LOW);
    delay(500);

    // Blink group B
    for(int i=0; i<4; i++) digitalWrite(ledA[i], LOW);
    for(int i=0; i<4; i++) digitalWrite(ledB[i], HIGH);
    delay(500);
  }
  else {             // Power OFF → all LEDs ON
    for(int i=0; i<4; i++) {
      digitalWrite(ledA[i], HIGH);
      digitalWrite(ledB[i], HIGH);
    }
  }
}