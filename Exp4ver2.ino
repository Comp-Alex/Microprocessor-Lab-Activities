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

String expression = "";
String display = "";
float result = 0;
bool hasResult = false;

float computeExpression(String expr) {
  if (expr == "") return 0;
  float res = 0;
  char op = '+';
  String num = "";
  for (int i = 0; i < expr.length(); i++) {
    char c = expr[i];
    if (c >= '0' && c <= '9') {
      num += c;
    } else {
      if (num != "") {
        float n = num.toFloat();
        if (op == '+') res += n;
        else if (op == '-') res -= n;
        else if (op == '*') res *= n;
        else if (op == '/') res /= n;
        num = "";
      }
      op = c;
    }
  }
  if (num != "") {
    float n = num.toFloat();
    if (op == '+') res += n;
    else if (op == '-') res -= n;
    else if (op == '*') res *= n;
    else if (op == '/') res /= n;
  }
  return res;
}

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
      expression = "";
      display = "";
      hasResult = false;
      updateLCD();
    }
  } else if (state == PRESSED) {
    if (key >= '0' && key <= '9') {
      expression += key;
      display = expression;
      hasResult = false;
    } else if (key == 'A') { // +
      if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '+';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'B') { // -
      if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '-';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'C') { // *
      if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '*';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'D') { // /
      if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '/';
        display = expression;
        hasResult = false;
      }
    } else if (key == '#') { // equals
      if (expression != "") {
        result = computeExpression(expression);
        hasResult = true;
        expression = "";
        display = "";
      }
    } else if (key == '*') { // clear one on press
      if (expression.length() > 0) {
        expression = expression.substring(0, expression.length()-1);
        display = expression;
        hasResult = false;
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
