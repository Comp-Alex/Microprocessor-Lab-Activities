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
bool hasError = false;

bool validateExpression(String expr) {
  if (expr == "") return true;

  // Start cannot be * or /
  char first = expr[0];
  if (first == '*' || first == '/') return false;

  // End cannot be operator (except a trailing expressed signed number is caught in the sequence rules)
  char last = expr[expr.length() - 1];
  if (last == '+' || last == '-' || last == '*' || last == '/') return false;

  for (int i = 0; i < expr.length() - 1; i++) {
    char c = expr[i];
    char next = expr[i + 1];

    // If current is * or /, next must be digit or '-' (negative term only)
    if (c == '*' || c == '/') {
      if (!(next == '-' || (next >= '0' && next <= '9'))) {
        return false;
      }
      if (next == '-' && i + 2 < expr.length()) {
        char after = expr[i + 2];
        if (!(after >= '0' && after <= '9')) return false; // must be -number after *-/
      }
    }

    // If current is + or -, next can be +, - (sign), or digit, but not * or /
    if (c == '+' || c == '-') {
      if (next == '*' || next == '/') {
        return false;
      }
    }

    // No consecutive * or /
    if ((c == '*' && (next == '*' || next == '/')) || (c == '/' && (next == '*' || next == '/'))) {
      return false;
    }

    // Prevent mid-expression ++/-- as operator rather than unary number parsing when out of scope: allow one plus/minus chain in those circumstances
    if ((c == '+' || c == '-') && (next == '+' || next == '-')) {
      // allow 2+-/2 or 2--2 etc but not ++++ sequences
      int chain = 1;
      for (int j = i + 1; j < expr.length() && (expr[j] == '+' || expr[j] == '-'); j++) chain++;
      if (chain > 2) return false;
    }
  }

  return true;
}

float computeExpression(String expr) {
  // Supports + - * / with operator precedence (PEMDAS without parentheses)
  if (expr == "") return 0;

  // Validate expression first
  if (!validateExpression(expr)) {
    return 0; // Error value
  }

  // Tokenize numbers and operators
  const int MAX_TOKENS = 32;
  float nums[MAX_TOKENS];
  char ops[MAX_TOKENS];
  int numCount = 0;
  int opCount = 0;

  String  num = "";
  for (int i = 0; i < expr.length(); i++) {
    char c = expr[i];

    if ((c >= '0' && c <= '9') || c == '.') {
      num += c;
    } else if ((c == '+' || c == '-') && num == "") {
      // Unary + or - (leading or after operator)
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
    if (ops2[i] == '+') {
      res += nums2[i + 1];
    } else if (ops2[i] == '-') {
      res -= nums2[i + 1];
    }
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
  
  if (hasError) {
    lcd.print("ERROR");
  } else if (hasResult) {
    lcd.print(String(result, 2));
  }
}

void keypadEvent(KeypadEvent key) {
  Serial.print("Key pressed: ");
  Serial.println(key);
  KeyState state = myKeypad.getState();

  if (state == HOLD) {
    if (key == '*') { // HOLD *: clear all
      expression = "";
      display = "";
      hasResult = false;
      hasError = false;
      updateLCD();
    }
  } else if (state == PRESSED) {
    // Helper to check if we can add ANY operator (+,-,*,/)
    char lastChar = expression.length() > 0 ? expression[expression.length()-1] : ' ';
    bool canAddOperator = (expression == "" || 
                          (lastChar >= '0' && lastChar <= '9') || 
                          lastChar == '.' ||
                          lastChar == '+' || lastChar == '-' ||
                          lastChar == '*' || lastChar == '/');

    if (key >= '0' && key <= '9') {
      if (hasResult || hasError) {
        expression = key;
        display = expression;
        hasResult = false;
        hasError = false;
      } else {
        expression += key;
        display = expression;
        hasResult = false;
        hasError = false;
      }
    } else if (key == 'A') { // +
      if (hasResult) {
        expression = String(result, 2) + '+';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        expression += '+';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'B') { // -
      if (hasResult) {
        expression = String(result, 2) + '-';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        expression += '-';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'C') { // *
      if (hasResult) {
        expression = String(result, 2) + '*';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {  // FIXED: Now allows after +/-
        expression += '*';
        display = expression;
        hasResult = false;
      }
    } else if (key == 'D') { // /
      if (hasResult) {
        expression = String(result, 2) + '/';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {  // FIXED: Now allows after +/-
        expression += '/';
        display = expression;
        hasResult = false;
      }
    } else if (key == '#') { // equals
      if (expression != "") {
        if (validateExpression(expression)) {
          result = computeExpression(expression);
          hasResult = true;
          hasError = false;
          expression = "";
        } else {
          hasError = true;
          hasResult = false;
        }
      }
    } else if (key == '*') { // PRESS *: clear one character at a time
      if (expression.length() > 0) {
        expression = expression.substring(0, expression.length()-1);
        display = expression;
        hasResult = false;
        hasError = false;
      } else if (hasResult || hasError) {
        hasResult = false;
        hasError = false;
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