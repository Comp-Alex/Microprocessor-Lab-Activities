#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Use the correct I2C address from your module (commonly 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);  

int seconds = 0;

void setup() {
  // Initialize the LCD
  lcd.init();          
  lcd.backlight();     // Turn on backlight

  // Print initial message
  lcd.setCursor(0, 0);
  lcd.print("hello world");
}

void loop() {
  // Set cursor to first column of second row
  lcd.setCursor(0, 1);

  // Print the seconds counter
  lcd.print(seconds);

  // Wait for 1 second
  delay(1000);

  // Increment seconds
  seconds++;
}