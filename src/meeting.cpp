#include <Arduino.h>
#include "meeting.h"
#include "broadcast.h"
#include "leds.h"
#include "display.h"

#define MEETING_MS 20000  // discussion window

static bool active = false;
static unsigned long endTime = 0;
static int lastSecs = -1;

void startMeeting() {
  if (active) {
    return; // already in a meeting; ignore repeats
  }
  active = true;
  endTime = millis() + MEETING_MS;
  lastSecs = -1;
  showMeetingScreen();                 // static "EMERGENCY MEETING" banner
  flashLEDs(90, 0, 0, MEETING_MS);     // hold red for the whole meeting
}

void triggerEmergencyMeeting() {
  broadcastMessage(MEETING_MSG);       // tell every other badge
  startMeeting();                      // and enter it ourselves
}

bool isMeetingActive() {
  return active;
}

void updateMeeting() {
  if (!active) {
    return;
  }
  long remaining = (long)endTime - (long)millis();
  if (remaining <= 0) {
    active = false;
    clearScreen();                     // hand the screen back to the tilt view
    return;
  }
  int secs = (int)((remaining + 999) / 1000);
  if (secs != lastSecs) {
    lastSecs = secs;
    showMeetingCountdown(secs);         // redraw only the countdown
  }
}
