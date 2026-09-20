#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "leds.h"

#define LED_PIN 3
#define NUM_LEDS 6

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastLedUpdate = 0;
uint16_t rainbowHue = 0;

static unsigned long flashUntil = 0;
static uint8_t flashR = 0, flashG = 0, flashB = 0;

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void setupLEDs() {
  strip.setBrightness(15);
  strip.begin();
  strip.show();
}

void flashLEDs(uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs) {
  flashR = r;
  flashG = g;
  flashB = b;
  flashUntil = millis() + durationMs;
}

void updateLEDs() {
  if (millis() < flashUntil) {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, strip.Color(flashR, flashG, flashB));
    }
    strip.show();
    return;
  }

  // Update LEDs every 20ms without blocking the CPU
  if (millis() - lastLedUpdate > 20) {
    lastLedUpdate = millis();
    
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i * 256 / strip.numPixels() + rainbowHue) & 255));
    }
    strip.show();
    
    rainbowHue++;
    if (rainbowHue >= 256) rainbowHue = 0;
  }
}
