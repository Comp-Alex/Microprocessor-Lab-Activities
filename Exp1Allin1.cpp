#include <Arduino.h>

const int ledPins[8] = {2,3,4,5,6,7,8,9};

const int SW1 = 12;   // main switch / button
const int SW2 = 11;   // second switch
const int SW3 = 10;	  // third switch

int mode = 0;
bool lastButtonState = HIGH;

unsigned long previousMillis = 0;
const long interval = 500;
bool ledState = false;

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
}

void loop() {

  // -------- MODE CHANGE (button press) --------
  bool buttonState = digitalRead(SW1);

  if (buttonState == LOW && lastButtonState == HIGH) {
    mode++;
    if (mode > 4) mode = 1;
    delay(200); // debounce
  }
  lastButtonState = buttonState;

  // -------- MODES --------
  switch (mode) {

    // CASE 1: All LEDs blinking
    case 1:
      blinkAll();
      break;

    // CASE 2: Alternating blink
    case 2:
      alternateBlink();
      break;
    
    case 3:
      if (digitalRead(SW2) == HIGH) {
        allOn();
      } else {
        allOff();
      }
      break;

    default:
      switchLogic();
      break;
  }
}

// -------- FUNCTIONS --------

void blinkAll() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;

    for (int i = 0; i < 8; i++) {
      digitalWrite(ledPins[i], ledState);
    }
  }
}

void alternateBlink() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    ledState = !ledState;

    for (int i = 0; i < 4; i++) {
      digitalWrite(ledPins[i], ledState);
      digitalWrite(ledPins[i + 4], !ledState);
    }
  }
}

void allOn() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}

void allOff() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
}

void switchLogic() {
  bool sw1 = digitalRead(SW3);
  bool sw2 = digitalRead(SW2);

  if (sw1 == LOW && sw2 == LOW) {
    allOff();
  }
  else if (sw1 == LOW && sw2 == HIGH) {
    for (int i = 0; i < 4; i++) digitalWrite(ledPins[i], HIGH);
    for (int i = 4; i < 8; i++) digitalWrite(ledPins[i], LOW);
  }
  else if (sw1 == HIGH && sw2 == LOW) {
    for (int i = 0; i < 4; i++) digitalWrite(ledPins[i], LOW);
    for (int i = 4; i < 8; i++) digitalWrite(ledPins[i], HIGH);
  }
  else {
    allOn();
  }
}