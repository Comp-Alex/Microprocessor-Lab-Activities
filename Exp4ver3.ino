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
  // Supports + - * / with operator precedence (PEMDAS without parentheses)
  if (expr == "") return 0;

  // Tokenize numbers and operators
  const int MAX_TOKENS = 32;
  float nums[MAX_TOKENS];
  char ops[MAX_TOKENS];
  int numCount = 0;
  int opCount = 0;

  String num = "";
  for (int i = 0; i < expr.length(); i++) {
    char c = expr[i];
    if (c >= '0' && c <= '9' || c == '.') {
      num += c;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
      if (num != "") {
        nums[numCount++] = num.toFloat();
        num = "";
      }
      ops[opCount++] = c;
    }
  }
  if (num != "") {
    nums[numCount++] = num.toFloat();
  }

  if (numCount == 0) return 0;

  // First pass: handle * and /
  float nums2[MAX_TOKENS];
  char ops2[MAX_TOKENS];
  int num2Count = 0;
  int op2Count = 0;

  nums2[num2Count++] = nums[0];
  for (int i = 0; i < opCount; i++) {
    char op = ops[i];
    float next = nums[i + 1];
    if (op == '*') {
      nums2[num2Count - 1] = nums2[num2Count - 1] * next;
    } else if (op == '/') {
      nums2[num2Count - 1] = nums2[num2Count - 1] / next;
    } else {
      ops2[op2Count++] = op;
      nums2[num2Count++] = next;
    }
  }

  // Second pass: handle + and -
  float res = nums2[0];
  for (int i = 0; i < op2Count; i++) {
    if (ops2[i] == '+') res += nums2[i + 1];
    else if (ops2[i] == '-') res -= nums2[i + 1];
  }

  return res;
}

void updateLCD() {
  lcd.setCursor(0,0);
  lcd.print("                ");  // Clear first line
  lcd.setCursor(0,0);
  
  if (display.length() > 16) {
    lcd.print(display.substring(display.length() - 16));
  } else {
    lcd.print(display);
  }
  
  lcd.setCursor(0,1);
  lcd.print("                ");  // Clear second line
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
      if (hasResult) {
        expression = key;
        display = expression;
        hasResult = false;
      } else {
        expression += key;
        display = expression;
        hasResult = false;
      }
    } else if (key == 'A') { // +
      if (hasResult) {
        expression = String(result, 2) + '+';
        display = expression;
        hasResult = false;
      } else if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '+';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'B') { // -
      if (hasResult) {
        expression = String(result, 2) + '-';
        display = expression;
        hasResult = false;
      } else if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '-';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'C') { // *
      if (hasResult) {
        expression = String(result, 2) + '*';
        display = expression;
        hasResult = false;
      } else if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '*';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'D') { // /
      if (hasResult) {
        expression = String(result, 2) + '/';
        display = expression;
        hasResult = false;
      } else if (expression != "" && expression[expression.length()-1] >= '0' && expression[expression.length()-1] <= '9') {
        expression += '/';
        display = expression;
        hasResult = false;
      }
    } else if (key == '#') { // equals
      if (expression != "") {
        result = computeExpression(expression);
        hasResult = true;
        expression = "";
      //  display = "";  //remove the comment to clear the inputs after showing result
      }
    } else if (key == '*') { // clear one on press or clear result
      if (expression.length() > 0) {
        expression = expression.substring(0, expression.length()-1);
        display = expression;
        hasResult = false;
      } else if (hasResult) {
        hasResult = false;
        updateLCD();
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
