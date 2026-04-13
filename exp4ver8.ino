// Arduino Calculator - ULTIMATE PERFECT VERSION (With Double Precision)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// HARDWARE CONFIGURATION
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

const byte numRows = 4;
const byte numCols = 4;

char keymap[numRows][numCols] = {
  {'1', '2', '3', 'A'},  // A = +
  {'4', '5', '6', 'B'},  // B = -
  {'7', '8', '9', 'C'},  // C = *
  {'.', '0', '#', 'D'}   // . = decimal, # = equals, D = /
};

byte rowPins[numRows] = {9, 8, 7, 6};
byte colPins[numCols] = {5, 4, 3, 2};

Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// GLOBAL VARIABLES
String expression = "";
bool hasError = false;
bool justCalculated = false;
int DECIMAL_PRECISION = 16;
String lastOperand = ""; // store result for repeat operations
char lastOperator = '+'; // store operator for repeat operations 

// HELPER FUNCTIONS
bool isOperator(char c) {
  return (c == '+' || c == '-' || c == '*' || c == '/');
}

bool isDigit(char c) {
  return (c >= '0' && c <= '9');
}

// EXPRESSION VALIDATION
bool validateExpression(String expr) {
  if (expr == "") return true;
  
  char first = expr[0];
  if (first == '*' || first == '/') return false;
  
  char last = expr[expr.length() - 1];
  if (isOperator(last)) return false;

  for (int i = 0; i < expr.length() - 1; i++) {
    char c = expr[i];
    char next = expr[i + 1];
    if (c == '*' || c == '/') {
      if (!(next == '-' || isDigit(next) || next == '.')) return false;
    }
    if (c == '+' || c == '-') {
      if (next == '*' || next == '/') return false;
    }
  }
  return true;
}

// EXPRESSION COMPUTATION 
double computeExpression(String expr) {
  if (expr == "" || !validateExpression(expr)) {
    hasError = true;
    return 0;
  }

  const int MAX_TOKENS = 32;
  double nums[MAX_TOKENS]; 
  char ops[MAX_TOKENS];
  int numCount = 0;
  int opCount = 0;

  String num = "";
  for (int i = 0; i < expr.length(); i++) {
    char c = expr[i];
    if (isDigit(c) || c == '.') {
      num += c;
    } else if ((c == '+' || c == '-') && num == "") {
      num += c;
    } else if (isOperator(c)) {
      if (num != "") {
        nums[numCount++] = num.toDouble(); 
        num = "";
      }
      ops[opCount++] = c;
    }
  }
  if (num != "") nums[numCount++] = num.toDouble(); 

  if (numCount == 0 || opCount >= numCount) {
    hasError = true;
    return 0;
  }

  double nums2[MAX_TOKENS]; 
  char ops2[MAX_TOKENS];
  int num2Count = 0;
  int op2Count = 0;

  nums2[num2Count++] = nums[0];
  for (int i = 0; i < opCount; i++) {
    char op = ops[i];
    double next = nums[i + 1];
    if (op == '*') {
      nums2[num2Count - 1] *= next;
    } else if (op == '/') {
      if (next == 0) {
        hasError = true;
        return 0;
      }
      nums2[num2Count - 1] /= next;
    } else {
      ops2[op2Count++] = op;
      nums2[num2Count++] = next;
    }
  }

  double result = nums2[0]; 
  for (int i = 0; i < op2Count; i++) {
    if (ops2[i] == '+') result += nums2[i + 1];
    else if (ops2[i] == '-') result -= nums2[i + 1];
  }
  return result;
}

// LCD DISPLAY
void displayStartup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Calculator Ready");
  delay(2000);
  lcd.clear();
  lcd.blink();
}

void displayExpression(String expr) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (expr.length() > LCD_COLS) {
    lcd.print(expr.substring(expr.length() - LCD_COLS));
  } else {
    lcd.print(expr);
  }
}

// BINAGO: Ipinatupad ang .00 logic
void displayResult(String expr, double result) { 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(expr);
  
  lcd.setCursor(0, 1);
  if (hasError) {
    lcd.print("Undefined");
  } else {
    lcd.print("=");
    
    String resultStr;
    // Kung whole number, maglagay ng .00
    if (result == (long)result) {
      resultStr = String((long)result) + ".00";
    } else {
      // Kung may decimal, ipakita at linisin ang sobrang zeros sa dulo
      resultStr = String(result, DECIMAL_PRECISION);
      while (resultStr.endsWith("0") && resultStr.indexOf('.') != -1) {
        resultStr.remove(resultStr.length() - 1);
      }
      // Kung sakaling natanggal lahat at nag-end sa ".", gawing ".00"
      if (resultStr.endsWith(".")) {
        resultStr += "00";
      }
    }
    
    int spaceLeft = LCD_COLS - 2;
    if (resultStr.length() > spaceLeft) {
      resultStr = String(result, 4); // Fallback precision para magkasya sa screen
    }
    
    lcd.print(resultStr);
  }
}

// INPUT HANDLING
void handleDecimal() {
  bool hasDecimal = false;
  int len = expression.length();
  
  for (int i = len - 1; i >= 0; i--) {
    char c = expression[i];
    if (isOperator(c)) break;
    if (c == '.') {
      hasDecimal = true;
      break;
    }
  }
  
  if (!hasDecimal) {
    expression += '.';
  }
}

void handleOperator(char op) {
  int len = expression.length();
  
  if (len == 0) {
    if (op == '-') {
      expression += op;
    }
    return;
  }
  
  char lastChar = expression[len - 1];
  bool lastIsOperator = isOperator(lastChar);
  if (lastIsOperator) {
    if ((lastChar == '*' || lastChar == '/') && op == '-') {
      expression += op;
    }
    else if (lastChar == '-' && len > 1 && 
             (expression[len - 2] == '*' || expression[len - 2] == '/')) {
      expression.remove(len - 1);
      expression[len - 2] = op;
    }
    else {
      expression[len - 1] = op;
    }
  } else {
    expression += op;
  }
}

void handleNumber(char digit) {
  int len = expression.length();
  // Case 1: Empty expression
  if (len == 0) {
    if (digit == '0') {
      expression = "0";
    } else {
      expression += digit;
    }
    return;
  }

  // Case 2: Prevent multiple leading zeros in a number
  char lastChar = expression[len - 1];
  if (lastChar == '0') {
    // Check if last '0' is at start of expression OR after an operator
    bool atStart = (len == 1);
    bool afterOp = isOperator(expression[len - 2]);

    if ((atStart || afterOp)) {
      // If next input is another '0' without decimal, BLOCK
      if (digit == '0') {
        return;
      }
    }
  }

  expression += digit;
}

void handleEquals() {
  if (expression.length() == 0) return;
  
  hasError = false;
  double result = computeExpression(expression);
  
  displayResult(expression, result);
  if (!hasError) {
    String cleanResult;
    // Para sa internal memory na expression, mas malinis kapag walang .00
    if (result == (long)result) {
      cleanResult = String((long)result);
    } else {
      cleanResult = String(result, DECIMAL_PRECISION);
      while (cleanResult.endsWith("0") && cleanResult.indexOf('.') != -1) {
        cleanResult.remove(cleanResult.length() - 1);
      }
      if (cleanResult.endsWith(".")) {
        cleanResult.remove(cleanResult.length() - 1);
      }
    }
    // on first equals press, store the result for repeat operations
    if (!justCalculated) {
      lastOperand = cleanResult;
      lastOperator = '+';
    }
    expression = cleanResult;
    justCalculated = true;
  } else {
    expression = "";
  }
}

char mapKeyToOperator(char key) {
  if (key == 'A') return '+';
  if (key == 'B') return '-';
  if (key == 'C') return '*';
  if (key == 'D') return '/';
  return key;
}

// ULTIMATE FIXED LOOP - BLOCKS ALL INVALID STARTS
void setup() {
  displayStartup();
}

void loop() {
  char key = myKeypad.getKey();
  if (!key) return;
  
  char mappedKey = mapKeyToOperator(key);
  
  if ((expression.length() == 0 || expression == "-" || expression == "+") && 
      (mappedKey == '*' || mappedKey == '/')) {
    return; // block * and / at the very start or after a leading sign
  }
  
  if (key == '#') {
    // if pressing = on a result, repeat the operation (add the previous result again)
    if (justCalculated && lastOperand != "") {
      expression = expression + lastOperator + lastOperand;
    }
    handleEquals();
    return;
  }

  if (justCalculated) {
    if (isDigit(mappedKey) || mappedKey == '.') {
      expression = "";
    }
    justCalculated = false;
  }
  
  if (mappedKey == '.') {
    handleDecimal();
  } else if (isOperator(mappedKey)) {
    handleOperator(mappedKey);
  } else if (isDigit(mappedKey)) {
    handleNumber(mappedKey);
  }
  
  displayExpression(expression);
}