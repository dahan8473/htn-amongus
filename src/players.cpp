#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include "players.h"
#include "broadcast.h"

// Among Us-style palette. Index is what badges exchange over the network.
static const PlayerColor COLORS[] = {
  { "Red",    197, 27,  27  },
  { "Blue",   40,  80,  230 },
  { "Green",  40,  180, 70  },
  { "Pink",   236, 120, 190 },
  { "Orange", 240, 140, 20  },
  { "Cyan",   60,  200, 210 },
  { "Yellow", 235, 220, 40  },
  { "Purple", 130, 60,  200 },
};
static const int NCOLORS = sizeof(COLORS) / sizeof(COLORS[0]);

static char s_myId[ID_LEN];
static int s_myColor = 0;

struct RosterEntry { char id[ID_LEN]; int colorIdx; unsigned long lastSeen; };
static RosterEntry roster[MAX_PLAYERS];
static int nRoster = 0;

void setupPlayers() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(s_myId, sizeof(s_myId), "%02X%02X", mac[4], mac[5]);
  // stable color from the whole MAC
  uint32_t h = 0;
  for (int i = 0; i < 6; i++) h = h * 31 + mac[i];
  s_myColor = h % NCOLORS;
  // seed our own entry so we always appear in the roster
  notePresence(s_myId, s_myColor);
}

const char *myId() { return s_myId; }
int myColorIndex() { return s_myColor; }
int colorCount() { return NCOLORS; }
const PlayerColor &colorByIndex(int i) { return COLORS[((i % NCOLORS) + NCOLORS) % NCOLORS]; }

void sendPresence() {
  char msg[24];
  snprintf(msg, sizeof(msg), "ID:%s:%d", s_myId, s_myColor);
  broadcastMessage(msg);
}

void notePresence(const char *id, int colorIdx) {
  for (int i = 0; i < nRoster; i++) {
    if (strncmp(roster[i].id, id, ID_LEN) == 0) {
      roster[i].colorIdx = colorIdx;
      roster[i].lastSeen = millis();
      return;
    }
  }
  if (nRoster < MAX_PLAYERS) {
    strncpy(roster[nRoster].id, id, ID_LEN);
    roster[nRoster].id[ID_LEN - 1] = '\0';
    roster[nRoster].colorIdx = colorIdx;
    roster[nRoster].lastSeen = millis();
    nRoster++;
  }
}

int rosterCount() { return nRoster; }
const char *rosterId(int i) { return roster[i].id; }
int rosterColorIndex(int i) { return roster[i].colorIdx; }

int rosterIndexOfId(const char *id) {
  for (int i = 0; i < nRoster; i++)
    if (strncmp(roster[i].id, id, ID_LEN) == 0) return i;
  return -1;
}
