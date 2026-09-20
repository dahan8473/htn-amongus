#include <Arduino.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"

unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupWiFi();
  setupBroadcast();
  setupButtons();

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
    // You could flash the LEDs red here to indicate hardware failure
  }
}

void loop() {
  // 1. Keep the rainbow animation running continuously
  updateLEDs();

  updateButtons();
  updateBroadcast();

  // 2. Read IMU and update the screen at 10Hz to prevent lag
  if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    
    float roll = 0;
    float pitch = 0;
    
    getRollPitch(roll, pitch);
    updateDisplay(roll, pitch);
  }
}
