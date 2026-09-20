#pragma once

void setupDisplay();
void updateDisplay(float roll, float pitch);

// Emergency meeting screen: draw the static banner once, then update just the
// countdown each second. clearScreen() blanks it so the tilt view can resume.
void showMeetingScreen();
void showMeetingCountdown(int secondsLeft);
void clearScreen();
