#include <Keypad.h>

const byte numRows = 4;
const byte numCols = 4;

char keymap[numRows][numCols] = {
  {'1','2','3','\0'},   // A disabled
  {'4','5','6','\0'},   // B disabled
  {'7','8','9','\0'},   // C disabled
  {'*','0','#','\0'}    // D disabled
};

byte rowPins[numRows] = {9, 8, 7, 6};
byte colPins[numCols] = {5, 4, 3, 2};

Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// Mapping for text mode
String letters[] = {
  " ",    // 0 → space
  ".?!",  // 1 → punctuation
  "ABC",  // 2
  "DEF",  // 3
  "GHI",  // 4
  "JKL",  // 5
  "MNO",  // 6
  "PQRS", // 7
  "TUV",  // 8
  "WXYZ"  // 9
};

char lastKey = NO_KEY;
unsigned long lastPressTime = 0;
int letterIndex = 0;
String message = "";

// Event handler for keypad
void keypadEvent(KeypadEvent key) {
  KeyState state = myKeypad.getState();

  if (state == PRESSED) {
    if (key == '*') {
      // Backspace
      if (message.length() > 0) {
        message.remove(message.length()-1);
      }
    } else if (key == '#') {
      // Newline / confirm
      message += '\n';
    } else if (isDigit(key)) {
      int num = key - '0';
      unsigned long now = millis();

      if (key == lastKey && (now - lastPressTime) < 1000) {
        // cycle through letters
        letterIndex = (letterIndex + 1) % letters[num].length();
        message.remove(message.length()-1);
        message += letters[num][letterIndex];
      } else {
        // new key or timeout
        letterIndex = 0;
        if (letters[num].length() > 0) {
          message += letters[num][letterIndex];
        }
      }

      lastKey = key;
      lastPressTime = now;
    }

    Serial.println(message);
  }
}

void setup() {
  Serial.begin(19200);
  myKeypad.setDebounceTime(200);   //stable input
  myKeypad.setHoldTime(1000);      //hold detection
  myKeypad.addEventListener(keypadEvent); //attach event handler
}

void loop() {
  myKeypad.getKey();   // triggers event handler automatically
}