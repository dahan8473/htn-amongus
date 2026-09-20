#include <Arduino.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include "roles.h"
#include "players.h"
#include "broadcast.h"
#include "display.h"

#define REVEAL_MS 5000

static GamePhase phase = PHASE_LOBBY;
static Role role = ROLE_NONE;
static unsigned long revealUntil = 0;
static bool revealDrawn = false;

void setupRoles() {
  phase = PHASE_LOBBY;
  role = ROLE_NONE;
}

GamePhase gamePhase() { return phase; }
Role myRole() { return role; }

void startGameAsHost() {
  int n = rosterCount();
  if (n < 1) return;
  int nImp = (n >= 5) ? (n / 5 + 1) : 1;  // ~1 impostor per 5 players, min 1
  if (nImp >= n) nImp = 1;                 // never all impostors

  // choose distinct impostor slots
  bool imp[MAX_PLAYERS] = { false };
  int chosen = 0;
  while (chosen < nImp) {
    int k = esp_random() % n;
    if (!imp[k]) { imp[k] = true; chosen++; }
  }

  // send each player their own role privately (they read only their own)
  for (int i = 0; i < n; i++) {
    char msg[24];
    snprintf(msg, sizeof(msg), "ROLE:%s:%c", rosterId(i), imp[i] ? 'I' : 'C');
    broadcastMessage(msg);
    if (strcmp(rosterId(i), myId()) == 0) {
      role = imp[i] ? ROLE_IMP : ROLE_CREW;  // set our own directly too
    }
  }
  broadcastMessage(PLAY_MSG);
  phase = PHASE_PLAY;
}

void handleGameMessage(const char *msg) {
  if (strncmp(msg, "ROLE:", 5) == 0) {
    // ROLE:<id>:<C|I>
    const char *p = msg + 5;
    const char *colon = strchr(p, ':');
    if (!colon) return;
    int idLen = colon - p;
    if (idLen == (int)strlen(myId()) && strncmp(p, myId(), idLen) == 0) {
      role = (colon[1] == 'I') ? ROLE_IMP : ROLE_CREW;
    }
  } else if (strcmp(msg, PLAY_MSG) == 0) {
    phase = PHASE_PLAY;
  } else if (strcmp(msg, ENDGAME_MSG) == 0) {
    phase = PHASE_LOBBY;
    role = ROLE_NONE;
  }
}

void requestRoleReveal() {
  if (phase != PHASE_PLAY || role == ROLE_NONE) return;
  revealUntil = millis() + REVEAL_MS;
  revealDrawn = false;
}

bool isRevealing() { return millis() < revealUntil; }

void updateRoles() {
  if (isRevealing()) {
    if (!revealDrawn) {
      revealDrawn = true;
      const PlayerColor &c = colorByIndex(myColorIndex());
      showRoleCard(c.r, c.g, c.b, role == ROLE_IMP);
    }
  } else if (revealDrawn) {
    revealDrawn = false;
    clearScreen();  // reveal expired; hand the screen back
  }
}
