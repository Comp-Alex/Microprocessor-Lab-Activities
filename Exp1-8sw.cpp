int ledA[] = {2,3,4,5};   // LED 1–4
int ledB[] = {6,7,8,9};   // LED 5–8

int sw1 = 10;
int sw2 = 11;

void setup() {
  for(int i=0; i<4; i++) {
    pinMode(ledA[i], OUTPUT);
    pinMode(ledB[i], OUTPUT);
  }
  pinMode(sw1, INPUT);
  pinMode(sw2, INPUT);
}

void loop() {
  int s1 = digitalRead(sw1);
  int s2 = digitalRead(sw2);

  // Default OFF
  for(int i=0; i<4; i++) {
    digitalWrite(ledA[i], LOW);
    digitalWrite(ledB[i], LOW);
  }

  // Evaluation Table Logic
  if(s1==LOW && s2==LOW) {
    // 0 0 → 1 1
    for(int i=0; i<4; i++) {
      digitalWrite(ledA[i], HIGH);
      digitalWrite(ledB[i], HIGH);
    }
  }
  else if(s1==LOW && s2==HIGH) {
    // 0 1 → 1 0
    for(int i=0; i<4; i++)
      digitalWrite(ledA[i], HIGH);
  }
  else if(s1==HIGH && s2==LOW) {
    // 1 0 → 0 1
    for(int i=0; i<4; i++)
      digitalWrite(ledB[i], HIGH);
  }
  // 1 1 → 0 0 (do nothing, all OFF)
}
