#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

volatile int count = 0;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 100;

void IRAM_ATTR countup() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > debounceDelay) {
    count++;
    lastInterruptTime = interruptTime;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(26, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(26), countup, FALLING);

  LCD.init();
  LCD.backlight();
  LCD.setCursor(0, 0);
  LCD.print("24/2/2025 11:39:45");
}

void loop() {
  LCD.setCursor(0, 1);
  LCD.print("BARREL: ");
  LCD.print(count);
}
