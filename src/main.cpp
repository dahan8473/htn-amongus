#include <Arduino.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"
#include "nfc.h"

unsigned long lastDisplayUpdate = 0;
bool nfcEnabled = false;

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupWiFi();
  setupBroadcast();
  setupButtons();

  // Start with NFC powered down to conserve energy
  powerDownNFC();

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
    // You could flash the LEDs red here to indicate hardware failure
  }
}

void loop() {
  // 1. Keep the rainbow animation running continuously
  updateLEDs();

  // 2. Process buttons and network broadcasts
  updateButtons();
  updateBroadcast();

  // 3. Monitor the maintained AUX1 switch for NFC toggling
  bool currentSwitchState = isButtonHeld(BTN_AUX1);
  if (currentSwitchState != nfcEnabled) {
    nfcEnabled = currentSwitchState;
    
    if (nfcEnabled) {
      beginNFCScan();
      Serial.println("NFC Powered ON");
    } else {
      powerDownNFC();
      Serial.println("NFC Powered OFF");
    }
  }

  // 4. If NFC is powered on, poll for task completion stickers
  if (nfcEnabled) {
    String uid = scanNFC();
    if (uid != "") {
      Serial.print("Task Completed! Scanned UID: ");
      Serial.println(uid);
      
      // Hint: You can use your broadcast module here to send the UID over WiFi!
    }
  }

  // 5. Read IMU and update the screen at 10Hz to prevent lag
  if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    
    float x_angle = 0;
    float y_angle = 0;
    
    // Using the existing IMU function, mapped to X and Y variables
    getRollPitch(x_angle, y_angle); 
    
    // Pass X, Y, and the NFC status to the display
    updateDisplay(x_angle, y_angle, nfcEnabled);
  }
}
