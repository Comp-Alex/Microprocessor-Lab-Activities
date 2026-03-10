#include <Keypad.h>

const byte numRows = 4; // number of rows
const byte numCols = 4; // number of columns

// Keymap layout
char keymap[numRows][numCols] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Pin connections
byte rowPins[numRows] = {9, 8, 7, 6}; // connect to row pins
byte colPins[numCols] = {5, 4, 3, 2}; // connect to column pins

// Create keypad object
Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

void setup() {
  Serial.begin(9600);
}

void loop() {
  char keypressed = myKeypad.getKey();
  if (keypressed) {
    Serial.println(keypressed);
  }
}