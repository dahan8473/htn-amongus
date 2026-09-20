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

void showRoleCard(int colorR, int colorG, int colorB, bool isImpostor) {
  uint16_t bg = isImpostor ? tft.color565(40, 0, 0) : tft.color565(0, 10, 30);
  uint16_t banner = isImpostor ? C_RED : tft.color565(40, 90, 220);
  tft.fillScreen(bg);

  // crewmate in the player's own profile color
  drawCrewmate(80, 128, tft.color565(colorR, colorG, colorB));

  // role banner on the right
  tft.fillRoundRect(150, 70, 158, 46, 8, banner);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  if (isImpostor) {
    tft.setCursor(160, 82);
    tft.print("IMPOSTOR");
  } else {
    tft.setCursor(160, 82);
    tft.print("CREW");
  }
  tft.setTextColor(ST77XX_WHITE, bg);
  tft.setTextSize(2);
  tft.setCursor(150, 140);
  tft.print(isImpostor ? "Sabotage & kill" : "Do your tasks");
}

void showTitleScreen(int players, int colorR, int colorG, int colorB) {
  tft.fillScreen(C_NAVY);
  drawCrewmate(66, 132, tft.color565(colorR, colorG, colorB));
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(4);
  tft.setCursor(120, 40);
  tft.print("AMONG");
  tft.setCursor(150, 82);
  tft.print("US");
  tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
  tft.setTextSize(2);
  char buf[24];
  snprintf(buf, sizeof(buf), "Players: %d  ", players);
  tft.setCursor(120, 140);
  tft.print(buf);
  tft.setCursor(120, 170);
  tft.print("START to begin");
}

void showVote(const char *name, int colorR, int colorG, int colorB,
              bool isSkip, int secondsLeft, bool alreadyVoted) {
  tft.fillScreen(C_NAVY);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  tft.setCursor(70, 18);
  tft.print("VOTE OUT");

  if (alreadyVoted) {
    tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
    tft.setTextSize(3);
    tft.setCursor(70, 110);
    tft.print("VOTED");
  } else if (isSkip) {
    tft.setTextSize(4);
    tft.setCursor(100, 100);
    tft.print("SKIP");
  } else {
    drawCrewmate(80, 132, tft.color565(colorR, colorG, colorB));
    tft.setTextColor(tft.color565(colorR, colorG, colorB), C_NAVY);
    tft.setTextSize(3);
    tft.setCursor(150, 120);
    tft.print(name);
  }

  tft.setTextColor(tft.color565(185, 185, 205), C_NAVY);
  tft.setTextSize(2);
  char buf[28];
  snprintf(buf, sizeof(buf), "%2ds  L/R  A=vote ", secondsLeft);
  tft.setCursor(24, 205);
  tft.print(buf);
}

void showEjectResult(const char *name, int colorR, int colorG, int colorB,
                     bool skipped) {
  tft.fillScreen(C_NAVY);
  if (skipped) {
    tft.setTextColor(ST77XX_WHITE, C_NAVY);
    tft.setTextSize(2);
    tft.setCursor(30, 110);
    tft.print("No one was ejected");
  } else {
    drawCrewmate(80, 120, tft.color565(colorR, colorG, colorB));
    tft.setTextColor(tft.color565(colorR, colorG, colorB));
    tft.setTextSize(3);
    tft.setCursor(150, 92);
    tft.print(name);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(150, 132);
    tft.print("was ejected");
  }
}
