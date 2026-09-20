#pragma once

void setupDisplay();
void updateDisplay(float x, float y, bool nfcEnabled);

// Emergency meeting screen: draw the static banner once, then either the
// "waiting for A" prompt (gather) or the countdown (discuss). clearScreen()
// blanks it so the tilt view can resume.
void showMeetingScreen();
void showMeetingWaiting();
void showMeetingCountdown(int secondsLeft);
void clearScreen();

// Role reveal card: a crewmate in the player's color, with CREWMATE (blue) or
// IMPOSTOR (red) banner. r/g/b is the player's profile color.
void showRoleCard(int colorR, int colorG, int colorB, bool isImpostor);
