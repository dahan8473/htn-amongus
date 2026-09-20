#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "display.h"

#define TFT_MOSI 10
#define TFT_SCLK 1
#define TFT_CS   2
#define TFT_DC   0
#define TFT_RST  4

#define ST7789_MADCTL 0x36

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setupDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  tft.init(240, 320); 
  
  // Try rotation 3 first (flips the screen 180 degrees)
  tft.setRotation(3); 
  
  // UNCOMMENT the following 3 lines ONLY if the text is mirrored backward:
  // tft.sendCommand(ST7789_MADCTL);
  // uint8_t madctl_mirrored_y = 0x60; // Adjusts Row/Column addressing
  // tft.spiWrite(madctl_mirrored_y);
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(3);
}

void updateDisplay(float roll, float pitch) {
  // We use dtostrf to ensure the strings are always the exact same width,
  // preventing old text from ghosting when the numbers shrink.
  char rollStr[10];
  char pitchStr[10];
  dtostrf(roll, 6, 1, rollStr);
  dtostrf(pitch, 6, 1, pitchStr);

  tft.setCursor(20, 60);
  tft.print("Roll:  ");
  tft.print(rollStr);

  tft.setCursor(20, 120);
  tft.print("Pitch: ");
  tft.print(pitchStr);
}

void showMeetingScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setCursor(20, 50);
  tft.print("EMERGENCY");
  tft.setCursor(70, 90);
  tft.print("MEETING");
}

void showMeetingCountdown(int secondsLeft) {
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  char buf[24];
  // trailing spaces keep the width fixed so old digits don't ghost
  snprintf(buf, sizeof(buf), "Discuss: %2ds  ", secondsLeft);
  tft.setCursor(60, 160);
  tft.print(buf);
}

void clearScreen() {
  tft.fillScreen(ST77XX_BLACK);
}
