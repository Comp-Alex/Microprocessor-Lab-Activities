#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

LiquidCrystal_I2C lcd(0x27,16,2); // I2C address 0x27, 16x2 LCD

const byte numRows = 4;
const byte numCols = 4;

char keymap[numRows][numCols] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[numRows] = {9, 8, 7, 6};
byte colPins[numCols] = {5, 4, 3, 2};

Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

float firstNum = 0;
float secondNum = 0;
char operation = 0;
String display = "";
bool hasOperation = false;
float result = 0;
bool hasResult = false;

void updateLCD() {
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,0);

  if (display.length() > 16) {
    lcd.print(display.substring(display.length() - 16));
  } else {
    lcd.print(display);
  }

  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  
  if (hasResult) {
    lcd.print(String(result, 2));
  }
}

void keypadEvent(KeypadEvent key) {
  Serial.print("Key pressed: ");
  Serial.println(key);
  KeyState state = myKeypad.getState();

  if (state == HOLD) {
    if (key == '*') { // clear all on hold
      firstNum = 0;
      secondNum = 0;
      operation = 0;
      hasOperation = false;
      hasResult = false;
      display = "";
      updateLCD();
    }
  } else if (state == PRESSED) {
    if (key >= '0' && key <= '9') {
      if (!hasOperation) {
        firstNum = firstNum * 10 + (key - '0');
        display = String(firstNum, 0);
        hasResult = false;
      } else {
        secondNum = secondNum * 10 + (key - '0');
        display = String(firstNum, 0) + " " + String(operation) + " " + String(secondNum, 0);
        hasResult = false;
      }
    } else if (key == 'A') { // +
      if (hasOperation && secondNum != 0) {
        if (operation == '+') result = firstNum + secondNum;
        else if (operation == '-') result = firstNum - secondNum;
        else if (operation == '*') result = firstNum * secondNum;
        else if (operation == '/' && secondNum != 0) result = firstNum / secondNum;
        else if (operation == '/' && secondNum == 0) {
          display = "Error: Div by 0";
          hasResult = false;
          updateLCD();
          return;
        }
        firstNum = result;
        secondNum = 0;
        hasResult = false;
      }
      if (firstNum != 0 || display != "") {
        operation = '+';
        hasOperation = true;
        display = String(firstNum, 0) + " + ";
        hasResult = false;
      }
    } else if (key == 'B') { // -
      if (hasOperation && secondNum != 0) {
        if (operation == '+') result = firstNum + secondNum;
        else if (operation == '-') result = firstNum - secondNum;
        else if (operation == '*') result = firstNum * secondNum;
        else if (operation == '/' && secondNum != 0) result = firstNum / secondNum;
        else if (operation == '/' && secondNum == 0) {
          display = "Error: Div by 0";
          hasResult = false;
          updateLCD();
          return;
        }
        firstNum = result;
        secondNum = 0;
        hasResult = false;
      }
      if (firstNum != 0 || display != "") {
        operation = '-';
        hasOperation = true;
        display = String(firstNum, 0) + " - ";
        hasResult = false;
      }
    } else if (key == 'C') { // *
      if (hasOperation && secondNum != 0) {
        if (operation == '+') result = firstNum + secondNum;
        else if (operation == '-') result = firstNum - secondNum;
        else if (operation == '*') result = firstNum * secondNum;
        else if (operation == '/' && secondNum != 0) result = firstNum / secondNum;
        else if (operation == '/' && secondNum == 0) {
          display = "Error: Div by 0";
          hasResult = false;
          updateLCD();
          return;
        }
        firstNum = result;
        secondNum = 0;
        hasResult = false;
      }
      if (firstNum != 0 || display != "") {
        operation = '*';
        hasOperation = true;
        display = String(firstNum, 0) + " * ";
        hasResult = false;
      }
    } else if (key == 'D') { // /
      if (hasOperation && secondNum != 0) {
        if (operation == '+') result = firstNum + secondNum;
        else if (operation == '-') result = firstNum - secondNum;
        else if (operation == '*') result = firstNum * secondNum;
        else if (operation == '/' && secondNum != 0) result = firstNum / secondNum;
        else if (operation == '/' && secondNum == 0) {
          display = "Error: Div by 0";
          hasResult = false;
          updateLCD();
          return;
        }
        firstNum = result;
        secondNum = 0;
        hasResult = false;
      }
      if (firstNum != 0 || display != "") {
        operation = '/';
        hasOperation = true;
        display = String(firstNum, 0) + " / ";
        hasResult = false;
      }
    } else if (key == '#') { // equals
      if (hasOperation) {
        if (operation == '+') result = firstNum + secondNum;
        else if (operation == '-') result = firstNum - secondNum;
        else if (operation == '*') result = firstNum * secondNum;
        else if (operation == '/' && secondNum != 0) result = firstNum / secondNum;
        else if (operation == '/' && secondNum == 0) {
          display = "Error: Div by 0";
          hasResult = false;
          updateLCD();
          return;
        }
        hasResult = true;
        display = String(firstNum, 0) + " " + String(operation) + " " + String(secondNum, 0);
        firstNum = result;
        secondNum = 0;
        operation = 0;
        hasOperation = false;
      }
    } else if (key == '*') { // clear one on press
      if (hasResult) {
        hasResult = false;
        display = String(firstNum, 0);
      } else if (hasOperation && secondNum != 0) {
        secondNum = 0;
        display = String(firstNum, 0) + " " + String(operation) + " ";
      } else if (hasOperation) {
        operation = 0;
        hasOperation = false;
        display = String(firstNum, 0);
      } else {
        firstNum = 0;
        display = "";
      }
    }

    updateLCD();
  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  updateLCD();

  myKeypad.setDebounceTime(50);
  myKeypad.setHoldTime(500);
  myKeypad.addEventListener(keypadEvent);
}

void loop() {
  myKeypad.getKey();
}
