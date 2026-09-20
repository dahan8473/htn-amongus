#pragma once

void setupDisplay();
void updateDisplay(float roll, float pitch);

// Emergency meeting screen: draw the static banner once, then either the
// "waiting for A" prompt (gather) or the countdown (discuss). clearScreen()
// blanks it so the tilt view can resume.
void showMeetingScreen();
void showMeetingWaiting();
void showMeetingCountdown(int secondsLeft);
void clearScreen();
