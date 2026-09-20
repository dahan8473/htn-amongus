#pragma once

// Emergency meeting, two stages:
//   1. Gather  - a badge presses START; every badge shows EMERGENCY MEETING and
//                BLINKS red while waiting.
//   2. Discuss - someone presses A ("everyone's here"); a shared countdown runs
//                on every badge with steady red LEDs. B ends it early for all,
//                or it ends when the timer hits zero.
// Runs over the WiFi broadcast; any badge can call, start, or end a meeting.

#define MEETING_MSG "MEETING"   // a meeting was called -> gather
#define DISCUSS_MSG "DISCUSS"   // everyone's here -> start the countdown
#define ENDMTG_MSG  "ENDMTG"    // finish the meeting early

void triggerEmergencyMeeting();  // START pressed: call a meeting for everyone
void startMeeting();             // enter the gather stage (on MEETING received)
void beginDiscussion();          // start the countdown (on DISCUSS received)
void confirmEveryoneHere();      // A during gather: tell everyone + start timer
void endMeetingEarly();          // B: tell everyone + end now
void endMeeting();               // end locally (timer done or ENDMTG received)

void updateMeeting();            // advance blink/timer/screen; call every loop
bool isMeetingActive();          // true during gather OR discuss
bool isMeetingGathering();       // true only while waiting for A
