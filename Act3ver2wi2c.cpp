#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ======== LCD SETUP ========
LiquidCrystal_I2C lcd(0x27,16,2); // I2C address 0x27, 16x2 LCD

// ======== KEYPAD SETUP ========
const byte numRows = 4;
const byte numCols = 4;

// Key layout (old phone style)
char keymap[numRows][numCols] = {
  {'1','2','3','\0'},
  {'4','5','6','\0'},
  {'7','8','9','\0'},
  {'*','0','#','\0'}
};

// Arduino pins connected to rows and columns
byte rowPins[numRows] = {9, 8, 7, 6};
byte colPins[numCols] = {5, 4, 3, 2};

// Create keypad object
Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// Multi-tap letters for each key
String letters[] = {
  " 0",     
  ",.<>/?!@#$%^&*()-_=+[]{}\|1",  
  "ABC2",  
  "DEF3",  
  "GHI4",  
  "JKL5",  
  "MNO6",  
  "PQRS7", 
  "TUV8",  
  "WXYZ9"  
};

// ======== VARIABLES ========
char lastKey = NO_KEY;               // last key pressed
unsigned long lastPressTime = 0;     // last key press time
unsigned long lastActivity = 0;      // last activity timestamp

int letterIndex = 0;                 // multi-tap index
String message = "";                 // stores typed message
int scrollPos = 0;                   // scroll position for LCD
bool previewMode = false;            // auto-scroll mode flag
unsigned long previewTimer = 0;      // timer for auto scroll
int wordCount = 0;                   // number of words

// =======================
// IMPROVED WORD COUNT FUNCTION
// Ignores symbols and counts only words with letters/numbers
// =======================
void countWords() {
  wordCount = 0;            // reset count
  bool inWord = false;      // flag kung nasa word ka
  int charCount = 0;        // count chars in current word

  for(int i=0; i<message.length(); i++){
    char c = message[i];

    if(isAlphaNumeric(c)){      // valid letter/number
      if(!inWord){              // new word
        inWord = true;
        charCount = 1;
      } else {
        charCount++;
      }
    }
    else{ // space or symbol
      if(inWord){               // end ng word
        if(charCount > 0)      // count only if at least 1 valid char
          wordCount++;
        inWord = false;
        charCount = 0;
      }
    }
  }

  // Check last word if message doesn't end with space/symbol
  if(inWord && charCount > 0)
    wordCount++;
}

// =======================
// UPDATE LCD FUNCTION
// =======================
void updateLCD() {

  // First row: Word count
  lcd.setCursor(0,0);
  lcd.print("Words:");
  lcd.print(wordCount);
  lcd.print("        "); // clear leftover chars

  // Second row: Message display
  lcd.setCursor(0,1);
  lcd.print("                "); // clear row first
  lcd.setCursor(0,1);

  // Get visible part of message based on scroll
  String visible = message.substring(scrollPos);
  if (visible.length() > 15)
    visible = visible.substring(0,15); // max 15 chars

  lcd.print(visible);

  // --- blinking cursor at end of visible text ---
  lcd.setCursor(visible.length(), 1);
  lcd.blink();  // enable blinking cursor
}

// =======================
// KEYPAD EVENT FUNCTION
// =======================
void keypadEvent(KeypadEvent key) {

  KeyState state = myKeypad.getState();

  if (state == PRESSED) {

    previewMode = false;          // stop preview if typing
    lastActivity = millis();      // reset activity timer

    // -------- CLEAR ALL MESSAGE ----------
    if (key == '#') {
      message = "";
      wordCount = 0;
      scrollPos = 0;
    }

    // -------- BACKSPACE ----------
    else if (key == '*') {
      if (message.length() > 0) {
        message.remove(message.length()-1);
        if(scrollPos > message.length())
          scrollPos = message.length();
      }
    }

    // -------- LETTER INPUT (multi-tap) ----------
    else if (isDigit(key)) {

      int num = key - '0';
      unsigned long now = millis();

      // Same key pressed within 1 second → cycle letter
      if (key == lastKey && (now - lastPressTime) < 700) {
        letterIndex = (letterIndex + 1) % letters[num].length();
        message.remove(message.length()-1);       // remove last letter
        message += letters[num][letterIndex];     // add new letter
      } 
      else {
        letterIndex = 0;                         // new key, first letter
        if (letters[num].length() > 0)
          message += letters[num][letterIndex];
      }

      lastKey = key;
      lastPressTime = now;

      // Auto-scroll if message longer than LCD
      if(message.length() - scrollPos > 15)
        scrollPos = message.length() - 15;
    }

    countWords();   // update word count
    updateLCD();    // refresh LCD
  }
}

// =======================
// SETUP FUNCTION
// =======================
void setup() {

  lcd.init();        // initialize LCD
  lcd.backlight();   // turn on backlight
  lcd.setCursor(0,0);
  lcd.print("Words:0");   // initial display

  lastActivity = millis();  // start activity timer

  // Keypad settings
  myKeypad.setDebounceTime(50);         // prevent double read
  myKeypad.setHoldTime(500);            // hold duration
  myKeypad.addEventListener(keypadEvent); // attach event handler
}

// =======================
// LOOP FUNCTION
// =======================
void loop() {

  myKeypad.getKey();    // check for keypad input

  // ---------- Auto preview mode (scroll message) ----------
  if(!previewMode && millis() - lastActivity > 4000 && message.length() > 16){
    previewMode = true;           // start auto-scroll
    previewTimer = millis();      // reset scroll timer
    scrollPos = 0;                // start from beginning
  }

  if(previewMode){
    // Scroll every 450ms
    if(millis() - previewTimer > 450){
      previewTimer = millis();
      String scrollMessage = message + "    "; // add spaces for smooth scroll

      String visible = "";
      for(int i=0;i<16;i++){
        visible += scrollMessage[(scrollPos + i) % scrollMessage.length()];
      }

      lcd.setCursor(0,1);
      lcd.print(visible);  // show scrolling message

      scrollPos++;          // advance scroll
      if(scrollPos >= scrollMessage.length())
        scrollPos = 0;      // loop scroll
    }
  }
}