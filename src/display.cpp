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

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setupDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  tft.init(240, 320); 
  tft.setRotation(3); 
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
  tft.setTextSize(3);
}

void updateDisplay(float x, float y, bool nfcEnabled) {
  char xStr[10];
  char yStr[10];
  dtostrf(x, 6, 1, xStr);
  dtostrf(y, 6, 1, yStr);

  tft.setCursor(20, 40);
  tft.print("X: ");
  tft.print(xStr);
  
  tft.setCursor(20, 100);
  tft.print("Y: ");
  tft.print(yStr);

  tft.setCursor(20, 160);
  tft.print("NFC: ");
  
  // Use color to indicate state, with a trailing space on "ON " 
  // so it fully overwrites the "FF" from "OFF" when toggling
  if (nfcEnabled) {
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.print("ON ");
  } else {
    tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft.print("OFF");
  }
  
  // Reset text color back to white for the next loop's X/Y text
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK); 
}
