#include <Arduino.h>
#include <string.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"
#include "meeting.h"
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
  // LEDs + buttons every loop. updateLEDs() shows a red flash while one is
  // active (e.g. during a meeting) and the rainbow otherwise.
  updateLEDs();
  updateButtons();

  // Emergency meeting: START calls it; during a meeting A starts the timer
  // (everyone's here) and B ends it early.
  if (isButtonPressed(BTN_START)) {
    triggerEmergencyMeeting();
  }
  if (isButtonPressed(BTN_A) && isMeetingGathering()) {
    confirmEveryoneHere();
  }
  if (isButtonPressed(BTN_B) && isMeetingActive()) {
    endMeetingEarly();
  }

  // Receive broadcasts from other badges and dispatch by message type.
  char msg[32];
  if (pollMessage(msg, sizeof(msg)) > 0) {
    if (strcmp(msg, MEETING_MSG) == 0) startMeeting();
    else if (strcmp(msg, DISCUSS_MSG) == 0) beginDiscussion();
    else if (strcmp(msg, ENDMTG_MSG) == 0) endMeeting();
  }

  // AUX1 maintained switch toggles the NFC reader on/off.
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

  // When NFC is on, poll for task-completion stickers.
  if (nfcEnabled) {
    String uid = scanNFC();
    if (uid != "") {
      Serial.print("Task Completed! Scanned UID: ");
      Serial.println(uid);
      // Hint: broadcast the UID here later to score tasks over WiFi.
    }
  }

  // Display: a meeting owns the screen; otherwise show the tilt + NFC view.
  if (isMeetingActive()) {
    updateMeeting();
  } else if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    float x_angle = 0;
    float y_angle = 0;
    getRollPitch(x_angle, y_angle);
    updateDisplay(x_angle, y_angle, nfcEnabled);
  }
}
