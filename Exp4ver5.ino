// Calculator using Arduino, Keypad, and LCD Display
// This program creates a simple calculator that supports +, -, *, / with operator precedence.
// It uses a 4x4 keypad for input and a 16x2 LCD for output.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// LCD setup: I2C address 0x27, 16 columns, 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup: 4 rows, 4 columns
const byte numRows = 4;
const byte numCols = 4;

// Keypad layout: numbers 0-9, operators +, -, *, /, and special keys
char keymap[numRows][numCols] = {
  {'1', '2', '3', 'A'},  // A = +
  {'4', '5', '6', 'B'},  // B = -
  {'7', '8', '9', 'C'},  // C = *
  {'.', '0', '#', 'D'}   // . = decimal point, # = equals, D = /
};

// Pin connections for keypad rows and columns
byte rowPins[numRows] = {9, 8, 7, 6};
byte colPins[numCols] = {5, 4, 3, 2};

// Create keypad object
Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// Variables to store the current expression, display text, result, and states
String expression = "";  // The mathematical expression being built
String display = "";     // What to show on the LCD
float result = 0;        // The computed result
bool hasResult = false;  // True if we have a result to display
bool hasError = false;   // True if there's an error in the expression

// Function to validate if an expression is mathematically correct
// Checks for invalid operator sequences and syntax
bool validateExpression(String expr) {
  if (expr == "") return true;  // Empty expression is valid

  // Expression cannot start with * or /
  char first = expr[0];
  if (first == '*' || first == '/') return false;

  // Expression cannot end with an operator
  char last = expr[expr.length() - 1];
  if (last == '+' || last == '-' || last == '*' || last == '/') return false;

  // Check each character and the next one for invalid sequences
  for (int i = 0; i < expr.length() - 1; i++) {
    char c = expr[i];
    char next = expr[i + 1];

    // After * or /, must be a digit or negative number
    if (c == '*' || c == '/') {
      if (!(next == '-' || (next >= '0' && next <= '9'))) {
        return false;
      }
      // If negative, check that there's a digit after the minus
      if (next == '-' && i + 2 < expr.length()) {
        char after = expr[i + 2];
        if (!(after >= '0' && after <= '9')) return false;
      }
    }

    // After + or -, cannot have * or /
    if (c == '+' || c == '-') {
      if (next == '*' || next == '/') {
        return false;
      }
    }

    // No consecutive * or /
    if ((c == '*' && (next == '*' || next == '/')) ||
        (c == '/' && (next == '*' || next == '/'))) {
      return false;
    }

    // Handle chains of + and - (like 2+-3 is allowed, but +++ is not)
    if ((c == '+' || c == '-') && (next == '+' || next == '-')) {
      int chain = 1;
      for (int j = i + 1; j < expr.length() && (expr[j] == '+' || expr[j] == '-'); j++) chain++;
      if (chain > 2) return false;  // More than 2 in a row is invalid
    }
  }

  return true;  // Expression is valid
}

// Function to compute the result of a valid expression
// Handles operator precedence: * and / before + and -
float computeExpression(String expr) {
  if (expr == "") return 0;

  // First validate the expression
  if (!validateExpression(expr)) {
    return 0;  // Return 0 for invalid expressions
  }

  // Maximum number of tokens (numbers and operators) we can handle
  const int MAX_TOKENS = 32;
  float nums[MAX_TOKENS];  // Array to store numbers
  char ops[MAX_TOKENS];    // Array to store operators
  int numCount = 0;        // Number of numbers found
  int opCount = 0;         // Number of operators found

  // Parse the expression into numbers and operators
  String num = "";  // Temporary string to build numbers
  for (int i = 0; i < expr.length(); i++) {
    char c = expr[i];

    if ((c >= '0' && c <= '9') || c == '.') {
      num += c;  // Build the number string
    } else if ((c == '+' || c == '-') && num == "") {
      // Unary + or - at the start or after an operator
      num += c;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
      // It's an operator
      if (num != "") {
        nums[numCount++] = num.toFloat();  // Convert string to float and store
        num = "";  // Reset for next number
      }
      ops[opCount++] = c;  // Store the operator
    }
  }
  // Don't forget the last number
  if (num != "") {
    nums[numCount++] = num.toFloat();
  }

  if (numCount == 0) return 0;  // No numbers found

  // First pass: handle multiplication and division (higher precedence)
  float nums2[MAX_TOKENS];  // New array for processed numbers
  char ops2[MAX_TOKENS];    // New array for remaining operators
  int num2Count = 0;
  int op2Count = 0;

  nums2[num2Count++] = nums[0];  // Start with first number
  for (int i = 0; i < opCount; i++) {
    char op = ops[i];
    float next = nums[i + 1];

    if (op == '*') {
      // Multiply the last number in nums2 with next
      nums2[num2Count - 1] = nums2[num2Count - 1] * next;
    } else if (op == '/') {
      // Divide the last number in nums2 by next
      nums2[num2Count - 1] = nums2[num2Count - 1] / next;
    } else {
      // For + or -, keep the operator and add the number
      ops2[op2Count++] = op;
      nums2[num2Count++] = next;
    }
  }

  // Second pass: handle addition and subtraction
  float res = nums2[0];  // Start with first processed number
  for (int i = 0; i < op2Count; i++) {
    if (ops2[i] == '+') {
      res += nums2[i + 1];
    } else if (ops2[i] == '-') {
      res -= nums2[i + 1];
    }
  }

  return res;
}

// Function to update the LCD display
void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("                ");  // Clear the line
  lcd.setCursor(0, 0);

  // Show the expression, scrolling if too long
  if (display.length() > 16) {
    lcd.print(display.substring(display.length() - 16));
  } else {
    lcd.print(display);
  }

  // Clear and update second line (result or error)
  lcd.setCursor(0, 1);
  lcd.print("                ");  // Clear the line
  lcd.setCursor(0, 1);

  if (hasError) {
    lcd.print("ERROR");
  } else if (hasResult) {
    // Show result with 2 decimal places
    lcd.print(String(result, 2));
  }
}

// Function called when a keypad key is pressed or held
void keypadEvent(KeypadEvent key) {
  Serial.print("Key pressed: ");
  Serial.println(key);
  KeyState state = myKeypad.getState();

  if (state == HOLD) {
    // Hold # to clear everything
    if (key == '#') {
      expression = "";
      display = "";
      hasResult = false;
      hasError = false;
      updateLCD();
    }
  } else if (state == PRESSED) {
    char lastChar = expression.length() > 0 ? expression[expression.length() - 1] : ' ';
    // Check if we can add an operator
    bool canAddOperator = (expression == "" ||
                          (lastChar >= '0' && lastChar <= '9') ||
                          lastChar == '.' ||
                          lastChar == '+' || lastChar == '-' ||
                          lastChar == '*' || lastChar == '/');

    if (key >= '0' && key <= '9') {
      // Number key pressed
      if (hasResult || hasError) {
        // Start new expression with this number
        expression = key;
        display = expression;
        hasResult = false;
        hasError = false;
      } else {
        // Add to existing expression
        expression += key;
        display = expression;
        hasResult = false;
        hasError = false;
      }
    } else if (key == '.') {  // Decimal point key
      if (hasResult || hasError) {
        // Start new number after result or error
        expression = "0.";
        display = expression;
        hasResult = false;
        hasError = false;
      } else if ((lastChar >= '0' && lastChar <= '9')) {
        // Add decimal to number
        expression += '.';
        display = expression;
        hasResult = false;
        hasError = false;
      }
    } else if (key == 'A') {  // + key
      if (hasResult) {
        // Use previous result as start
        expression = String(result, 2) + '+';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        // Smart error handling: replace consecutive operators
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/') {
          expression = expression.substring(0, expression.length() - 1) + '+';
        } else {
          expression += '+';
        }
        display = expression;
        hasResult = false;
      }
    } else if (key == 'B') {  // - key
      if (hasResult) {
        expression = String(result, 2) + '-';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        // Smart error handling: replace consecutive operators
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/') {
          expression = expression.substring(0, expression.length() - 1) + '-';
        } else {
          expression += '-';
        }
        display = expression;
        hasResult = false;
      }
    } else if (key == 'C') {  // * key
      if (hasResult) {
        expression = String(result, 2) + '*';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        // Smart error handling: replace consecutive operators
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/') {
          expression = expression.substring(0, expression.length() - 1) + '*';
        } else {
          expression += '*';
        }
        display = expression;
        hasResult = false;
      }
    } else if (key == 'D') {  // / key
      if (hasResult) {
        expression = String(result, 2) + '/';
        display = expression;
        hasResult = false;
      } else if (canAddOperator) {
        // Smart error handling: replace consecutive operators
        if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/') {
          expression = expression.substring(0, expression.length() - 1) + '/';
        } else {
          expression += '/';
        }
        display = expression;
        hasResult = false;
      }
    } else if (key == '#') {  // Equals key
      if (expression != "") {
        if (validateExpression(expression)) {
          result = computeExpression(expression);
          hasResult = true;
          hasError = false;
          expression = "";  // Clear expression after computing
        } else {
          hasError = true;
          hasResult = false;
        }
      }
    }

    updateLCD();
  }
}

// Setup function: runs once when Arduino starts
void setup() {
  Serial.begin(9600);  
  lcd.init();          
  lcd.backlight();     
  updateLCD();      


  myKeypad.setDebounceTime(50);   
  myKeypad.setHoldTime(500);     
  myKeypad.addEventListener(keypadEvent);  
}

// Loop function: runs repeatedly
void loop() {
  myKeypad.getKey();
}