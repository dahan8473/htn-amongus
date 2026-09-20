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

// Title / lobby screen: AMONG US logo, a crewmate in your color, player count.
void showTitleScreen(int players, int colorR, int colorG, int colorB);

// Voting screen: the current pick (a color, or SKIP), countdown, and hints.
void showVote(const char *name, int colorR, int colorG, int colorB,
              bool isSkip, int secondsLeft, bool alreadyVoted);

// Result of a vote: who got ejected (or that it was skipped).
void showEjectResult(const char *name, int colorR, int colorG, int colorB,
                     bool skipped);
