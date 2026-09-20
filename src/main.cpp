#include <Arduino.h>
#include <string.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"
#include "meeting.h"

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
  // LEDs and buttons every loop. updateLEDs() shows a red flash while one is
  // active (e.g. during a meeting) and the rainbow otherwise.
  updateLEDs();
  updateButtons();

  // START calls an emergency meeting. During a meeting: A starts the timer
  // (once everyone's here), B ends it early.
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

  if (isMeetingActive()) {
    // Meeting owns the screen; skip the tilt view until it ends.
    updateMeeting();
  } else if (millis() - lastDisplayUpdate > 100) {
    lastDisplayUpdate = millis();
    float roll = 0;
    float pitch = 0;
    getRollPitch(roll, pitch);
    updateDisplay(roll, pitch);
  }
}
