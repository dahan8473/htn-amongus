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

// Among Us-style palette
#define C_NAVY   tft.color565(10, 12, 34)
#define C_RED    tft.color565(197, 27, 27)
#define C_REDDK  tft.color565(120, 12, 12)
#define C_VISOR  tft.color565(150, 210, 236)
#define C_SHADOW tft.color565(90, 12, 12)

// A classic Among Us crewmate: body, backpack, legs, visor. (cx,cy) = body center.
static void drawCrewmate(int cx, int cy, uint16_t body) {
  // backpack (behind the body)
  tft.fillRoundRect(cx + 18, cy - 16, 16, 36, 7, C_SHADOW);
  // body (tall rounded capsule)
  tft.fillRoundRect(cx - 26, cy - 38, 50, 76, 22, body);
  // leg gap carved out of the bottom
  tft.fillRect(cx - 4, cy + 24, 9, 16, C_NAVY);
  // visor
  tft.fillRoundRect(cx - 20, cy - 24, 38, 18, 9, C_VISOR);
  tft.fillRoundRect(cx - 15, cy - 21, 12, 7, 3, ST77XX_WHITE); // shine
}

void showMeetingScreen() {
  tft.fillScreen(C_NAVY);
  drawCrewmate(58, 128, C_RED);

  // red title banner on the right
  tft.fillRoundRect(104, 44, 208, 78, 10, C_RED);
  tft.drawRoundRect(104, 44, 208, 78, 10, C_REDDK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(118, 56);
  tft.print("EMERGENCY");
  tft.setCursor(142, 88);
  tft.print("MEETING");
}

void showMeetingWaiting() {
  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setTextSize(2);
  tft.setCursor(118, 146);
  tft.print("Everyone here?");
  tft.setCursor(118, 170);
  tft.print("A=start B=cancel");
}

void showMeetingCountdown(int secondsLeft) {
  // pulsing alarm border (alternates each second)
  uint16_t border = (secondsLeft % 2 == 0) ? C_RED : tft.color565(240, 170, 20);
  for (int i = 0; i < 3; i++) {
    tft.drawRect(i, i, 320 - 2 * i, 240 - 2 * i, border);
  }
  // countdown + end hint (fixed width so old digits don't ghost)
  tft.setTextColor(ST77XX_WHITE, C_NAVY);
  tft.setTextSize(2);
  char buf[24];
  snprintf(buf, sizeof(buf), "Discuss: %2ds  ", secondsLeft);
  tft.setCursor(118, 146);
  tft.print(buf);
  tft.setCursor(118, 170);
  tft.print("B = end early ");
}

void clearScreen() {
  tft.fillScreen(ST77XX_BLACK);
}
