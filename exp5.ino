#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ----- DHT11 SETUP -----
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ----- LCD SETUP -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- PINS -----
const int switchPin = 7;
const int ldrPin = A0;

// ----- FUNCTIONS -----

// Display Temperature and Humidity together
void showTempHumidity(float temp, float hum) {
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  if (isnan(temp)) {
    lcd.print("Err ");
  } else {
    lcd.print(temp);
    lcd.print("C ");
  }

  lcd.setCursor(0, 1);
  lcd.print("Humid:");
  if (isnan(hum)) {
    lcd.print("Err ");
  } else {
    lcd.print(hum);
    lcd.print("% ");
  }
}

// Display Light Level
void showLight(int lightVal) {
  lcd.setCursor(0, 0);
  lcd.print("Light Level:");

  lcd.setCursor(0, 1);
  lcd.print(lightVal);
}

void setup() {
  lcd.init();
  lcd.backlight();

  dht.begin();

  pinMode(switchPin, INPUT_PULLUP);
}

void loop() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int lightVal = analogRead(ldrPin);

  int switchState = digitalRead(switchPin);

  // Clear LCD properly
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");

  // Switch between modes
  if (switchState == LOW) {
    showTempHumidity(temp, hum);
  } else {
    showLight(lightVal);
  }

  delay(2000);
}