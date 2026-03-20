#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define MAX_EXPR 48
#define STACK_SIZE 24

LiquidCrystal_I2C lcd(0x27,16,2);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
{'1','2','3','+'},
{'4','5','6','-'},
{'7','8','9','*'},
{'A','0','=','B'}
};

byte rowPins[ROWS] = {9,8,7,6};
byte colPins[COLS] = {5,4,3,2};

Keypad keypad = Keypad(makeKeymap(keys),rowPins,colPins,ROWS,COLS);

char expression[MAX_EXPR+1];
char lastCalc[60];

int exprLen = 0;
int parenCount = 0;

bool isOperator(char c){
  return c=='+'||c=='-'||c=='*'||c=='/';
}

void setup(){

  lcd.init();
  lcd.backlight();

  lcd.print("Arduino Calc");
  delay(1500);
  lcd.clear();

  lcd.cursor();
  lcd.blink();

  expression[0]='\0';
}

void loop(){

  char key = keypad.getKey();
  if(!key) return;

  if(key=='A'){ showHistory(); return; }
  if(key=='B'){ backspace(); return; }
  if(key=='C'){ clearExpression(); return; }
  if(key=='='){ calculate(); return; }

  appendKey(key);
}

void appendKey(char key){

  if(exprLen>=MAX_EXPR) return;

  if(key=='('){
    parenCount++;
  }

  if(key==')'){
    if(parenCount<=0) return;
    parenCount--;
  }

  if(isOperator(key)){
    if(exprLen==0) return;
    if(isOperator(expression[exprLen-1])) return;
  }

  expression[exprLen++]=key;
  expression[exprLen]='\0';

  updateDisplay();
}

void backspace(){

  if(exprLen==0) return;

  if(expression[exprLen-1]=='(') parenCount--;
  if(expression[exprLen-1]==')') parenCount++;

  exprLen--;
  expression[exprLen]='\0';

  updateDisplay();
}

void clearExpression(){

  exprLen=0;
  parenCount=0;

  expression[0]='\0';
  lcd.clear();
}

void showHistory(){

  lcd.clear();
  lcd.print("LAST:");

  lcd.setCursor(0,1);
  lcd.print(lastCalc);

  delay(2000);
  updateDisplay();
}

void updateDisplay(){

  int start = exprLen>32 ? exprLen-32 : 0;

  lcd.setCursor(0,0);
  for(int i=0;i<16;i++){
    int idx=start+i;
    if(idx<exprLen) lcd.print(expression[idx]);
    else lcd.print(" ");
  }

  lcd.setCursor(0,1);
  for(int i=16;i<32;i++){
    int idx=start+i;
    if(idx<exprLen) lcd.print(expression[idx]);
    else lcd.print(" ");
  }

  int cursorPos = exprLen-start;
  if(cursorPos<16) lcd.setCursor(cursorPos,0);
  else lcd.setCursor(cursorPos-16,1);
}

int precedence(char op){
  if(op=='+'||op=='-') return 1;
  if(op=='*'||op=='/') return 2;
  return 0;
}

bool applyOp(float a,float b,char op,float &r){

  if(op=='+') r=a+b;
  else if(op=='-') r=a-b;
  else if(op=='*') r=a*b;
  else if(op=='/'){
    if(b==0) return false;
    r=a/b;
  }

  return true;
}

bool evaluate(float &result){

  float values[STACK_SIZE];
  char ops[STACK_SIZE];

  int vTop=-1;
  int oTop=-1;

  for(int i=0;i<exprLen;i++){

    char c = expression[i];

    if(isdigit(c)){

      float val=0;

      while(i<exprLen && isdigit(expression[i])){
        val = val*10+(expression[i]-'0');
        i++;
      }

      i--;

      values[++vTop]=val;
    }

    else if(c=='('){
      ops[++oTop]=c;
    }

    else if(c==')'){

      while(oTop>=0 && ops[oTop]!='('){

        float b=values[vTop--];
        float a=values[vTop--];
        float r;

        if(!applyOp(a,b,ops[oTop--],r)) return false;

        values[++vTop]=r;
      }

      oTop--;
    }

    else{

      while(oTop>=0 && precedence(ops[oTop])>=precedence(c)){

        float b=values[vTop--];
        float a=values[vTop--];
        float r;

        if(!applyOp(a,b,ops[oTop--],r)) return false;

        values[++vTop]=r;
      }

      ops[++oTop]=c;
    }
  }

  while(oTop>=0){

    float b=values[vTop--];
    float a=values[vTop--];
    float r;

    if(!applyOp(a,b,ops[oTop--],r)) return false;

    values[++vTop]=r;
  }

  result=values[vTop];
  return true;
}

void calculate(){

  if(parenCount!=0){
    lcd.clear();
    lcd.print("Paren Error");
    delay(2000);
    updateDisplay();
    return;
  }

  float result;

  if(!evaluate(result)){

    lcd.clear();
    lcd.print("Math Error");
    delay(2000);
    clearExpression();
    return;
  }

  lcd.clear();
  lcd.print("=");
  lcd.print(result,4);

  snprintf(lastCalc,sizeof(lastCalc),"%s=%.4f",expression,result);

  delay(2000);

  exprLen = snprintf(expression,sizeof(expression),"%.4f",result);

  updateDisplay();
}