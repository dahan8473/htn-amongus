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

// Role reveal card: a crewmate in the player's color, CREWMATE (blue) or
// IMPOSTOR (red) banner. If impostor, small crewmate icons for each teammate
// color are drawn on the left so you know your fellow impostors.
void showRoleCard(int colorR, int colorG, int colorB, bool isImpostor,
                  int nTeam, const uint8_t *teamR, const uint8_t *teamG,
                  const uint8_t *teamB);

// Lobby: AMONG US logo, crewmate in your color, player count, and the
// host-adjustable settings list with a cursor on row `sel`.
void showLobby(int players, int imp, int disc, int vote, int meet, int sel,
               int colorR, int colorG, int colorB);

// In-game status: your crewmate + color, alive count (or GHOST if dead).
// Role is NOT shown here -- hold START to see it. bodyNearby shows a
// "hold B to report" hint whenever a dead player is within report range.
void showHUD(bool alive, int aliveCount, int colorR, int colorG, int colorB,
             bool bodyNearby);

// Voting screen: the current pick (a color, or SKIP), countdown, and hints.
void showVote(const char *name, int colorR, int colorG, int colorB,
              bool isSkip, int secondsLeft, bool alreadyVoted);

// Result of a vote: who was ejected (+ impostor reveal, or skipped), plus a
// tally row of mini color icons with how many votes each color got.
void showResult(const char *ejName, int ejR, int ejG, int ejB,
                bool skipped, bool wasImpostor,
                int nTally, const uint8_t *talR, const uint8_t *talG,
                const uint8_t *talB, const int *talCounts, int skipCount);

// Winner screen.
void showGameOver(bool crewWon);
