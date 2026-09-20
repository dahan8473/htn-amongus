#include <Arduino.h>
#include "meeting.h"
#include "broadcast.h"
#include "leds.h"
#include "display.h"

#define MEETING_MS 20000  // discussion countdown length
#define BLINK_MS   400    // gather-stage red blink period

enum { M_OFF, M_GATHER, M_DISCUSS };
static int phase = M_OFF;
static unsigned long endTime = 0;
static int lastSecs = -1;

void startMeeting() {
  if (phase != M_OFF) return;      // already meeting; ignore repeats
  phase = M_GATHER;
  showMeetingScreen();
  showMeetingWaiting();
}

void triggerEmergencyMeeting() {
  broadcastMessage(MEETING_MSG);
  startMeeting();
}

void beginDiscussion() {
  if (phase == M_OFF || phase == M_DISCUSS) return;  // only from gather
  phase = M_DISCUSS;
  endTime = millis() + MEETING_MS;
  lastSecs = -1;
  showMeetingScreen();             // redraw base, clearing the waiting text
}

void confirmEveryoneHere() {
  if (phase != M_GATHER) return;
  broadcastMessage(DISCUSS_MSG);
  beginDiscussion();
}

void endMeeting() {
  if (phase == M_OFF) return;
  phase = M_OFF;
  clearScreen();                   // hand the screen back to the tilt view
}

void endMeetingEarly() {
  if (phase == M_OFF) return;
  broadcastMessage(ENDMTG_MSG);
  endMeeting();
}

bool isMeetingActive() { return phase != M_OFF; }
bool isMeetingGathering() { return phase == M_GATHER; }

void updateMeeting() {
  if (phase == M_OFF) return;

  if (phase == M_GATHER) {
    // blink red until someone presses A
    bool on = (millis() / BLINK_MS) % 2 == 0;
    flashLEDs(on ? 90 : 0, 0, 0, BLINK_MS + 100);
    return;
  }

  // M_DISCUSS: steady red + countdown
  flashLEDs(90, 0, 0, 250);        // re-armed each loop so it holds
  long remaining = (long)endTime - (long)millis();
  if (remaining <= 0) {
    endMeeting();
    return;
  }
  int secs = (int)((remaining + 999) / 1000);
  if (secs != lastSecs) {
    lastSecs = secs;
    showMeetingCountdown(secs);
  }
}
