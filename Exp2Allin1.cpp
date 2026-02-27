#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

// Use the correct I2C address from your module (commonly 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Button pin
const int buttonPin = 2;

// Variable to track current display screen
int currentScreen = 0;

// Variable to track button state
int lastButtonState = HIGH;
int debounceCounter = 0;
const int debounceDelay = 2;  // Counter threshold for debounce

// Scrolling variables
const char namesText[] = "Aaron ludwig Altar, Alexander John Balagso, Carl Jayson Eli Bonaobra, Ronald William Geron";
int scrollPos = 0;
int scrollCounter = 0;
const int scrollDelay = 50;  // Counter threshold for scroll steps

void setup() {
  // Initialize the LCD
  lcd.init();
  lcd.backlight();

  // Initialize button pin
  pinMode(buttonPin, INPUT_PULLUP);

  // Display the first screen
  displayScreen(currentScreen);
}

void loop() {
  // Read the button state with debouncing
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    debounceCounter = 0;
  }

  debounceCounter++;
  if (debounceCounter > debounceDelay) {
    if (reading == LOW) {  // Button is pressed
      // Move to next screen
      currentScreen = (currentScreen + 1) % 3;  // Cycle through 0, 1, 2
      scrollPos = 0;  // Reset scroll position
      displayScreen(currentScreen);
      delay(300);  // Button press delay to avoid multiple triggers
    }
  }

  lastButtonState = reading;

  // Handle scrolling for names screen
  if (currentScreen == 1) {
    scrollCounter++;
    if (scrollCounter > scrollDelay) {
      // Scroll text on first line only
      lcd.setCursor(0, 0);
      // Display 16 characters of the scrolling text
      for (int i = 0; i < 16; i++) {
        if (scrollPos + i < strlen(namesText)) {
          lcd.write(namesText[scrollPos + i]);
        } else {
          lcd.write(' ');
        }
      }
      scrollPos++;
      // Reset when text scrolled completely off screen
      if (scrollPos >= strlen(namesText) - 16) {
        scrollPos = 0;
      }
      scrollCounter = 0;
    }
  }

  delay(10);  // Small delay for program stability
}

void displayScreen(int screen) {
  // Clear the display
  lcd.clear();

  switch(screen) {
    case 0:
      // Screen 1: hello world and arduino
      lcd.setCursor(0, 0);
      lcd.print("hello world");
      lcd.setCursor(0, 1);
      lcd.print("arduino");
      break;

    case 1:
      // Screen 2: names scrolling and CPE3A & CPE3B
      lcd.setCursor(0, 1);
      lcd.print("CPE3A & CPE3B");
      break;

    case 2:
      // Screen 3: Hugot line
      lcd.setCursor(0, 0);
      lcd.print("sana all");
      lcd.setCursor(0, 1);
      lcd.print("may valentine");
      break;
  }
}