#pragma once

// Emergency meeting. One badge calls it (Start button); every badge shows the
// EMERGENCY MEETING screen with a discussion countdown and glows red until it
// ends. Built on the WiFi broadcast transport.

#define MEETING_MSG "MEETING"

// Called when this badge's player calls a meeting: notifies everyone, then
// enters the meeting locally.
void triggerEmergencyMeeting();

// Enter the meeting state locally (used when a MEETING message is received).
void startMeeting();

// Advance the meeting timer + screen. Call every loop while active.
void updateMeeting();

bool isMeetingActive();
