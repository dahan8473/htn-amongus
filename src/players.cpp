#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include "players.h"
#include "broadcast.h"

static const PlayerColor COLORS[] = {
  { "Red",    197, 27,  27  },
  { "Blue",   40,  80,  230 },
  { "Green",  40,  180, 70  },
  { "Pink",   236, 120, 190 },
  { "Orange", 240, 140, 20  },
  { "Cyan",   60,  200, 210 },
  { "Yellow", 235, 220, 40  },
  { "Purple", 130, 60,  200 },
  { "White",  245, 245, 245 },
  { "Lime",   130, 230, 60  },
  { "Brown",  150, 90,  45  },
  { "Gray",   140, 140, 150 },
};
static const int NCOLORS = sizeof(COLORS) / sizeof(COLORS[0]);

static char s_myId[ID_LEN];
static int s_myColor = 0;
static bool colorAssignmentsLocked = false;

struct RosterEntry {
  char id[ID_LEN];
  int colorIdx;
  bool alive;
  int role;
  int meetings;
  bool immortal;
  unsigned long lastSeen;
};
static RosterEntry roster[MAX_PLAYERS];
static int nRoster = 0;

void setupPlayers() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(s_myId, sizeof(s_myId), "%02X%02X", mac[4], mac[5]);
  uint32_t h = 0;
  for (int i = 0; i < 6; i++) h = h * 31 + mac[i];
  s_myColor = h % NCOLORS;
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
      roster[i].lastSeen = millis();
      if (!colorAssignmentsLocked) assignUniqueColors();
      return;
    }
  }
  // After the host locks a game, only IDs in its complete color map may join.
  if (colorAssignmentsLocked) return;
  if (nRoster < MAX_PLAYERS) {
    strncpy(roster[nRoster].id, id, ID_LEN);
    roster[nRoster].id[ID_LEN - 1] = '\0';
    roster[nRoster].colorIdx = colorIdx;
    roster[nRoster].alive = true;
    roster[nRoster].role = ROLE_NONE;
    roster[nRoster].meetings = 0;
    roster[nRoster].immortal = false;
    roster[nRoster].lastSeen = millis();
    nRoster++;
    assignUniqueColors();
  }
}

static int preferredColorForId(const char *id) {
  unsigned int value = 0;
  for (int i = 0; i < ID_LEN - 1 && id[i]; i++) {
    value <<= 4;
    if (id[i] >= '0' && id[i] <= '9') value |= id[i] - '0';
    else if (id[i] >= 'A' && id[i] <= 'F') value |= id[i] - 'A' + 10;
    else if (id[i] >= 'a' && id[i] <= 'f') value |= id[i] - 'a' + 10;
  }
  return value % NCOLORS;
}

static int takeColor(int *bank, int *bankSize, int preferred) {
  int index = -1;
  for (int i = 0; i < *bankSize; i++) {
    if (bank[i] == preferred) { index = i; break; }
  }
  if (index < 0) index = 0;
  if (*bankSize <= 0) return -1;
  int chosen = bank[index];
  for (int i = index; i + 1 < *bankSize; i++) bank[i] = bank[i + 1];
  (*bankSize)--;
  return chosen;
}

bool setColorId(const char *id, int colorIdx) {
  if (!id || colorIdx < 0 || colorIdx >= NCOLORS) return false;
  for (int i = 0; i < nRoster; i++) {
    if (strncmp(roster[i].id, id, ID_LEN) == 0) {
      roster[i].colorIdx = colorIdx;
      roster[i].lastSeen = millis();
      if (strcmp(id, s_myId) == 0) s_myColor = colorIdx;
      return true;
    }
  }
  if (nRoster >= MAX_PLAYERS) return false;
  strncpy(roster[nRoster].id, id, ID_LEN);
  roster[nRoster].id[ID_LEN - 1] = '\0';
  roster[nRoster].colorIdx = colorIdx;
  roster[nRoster].alive = true;
  roster[nRoster].role = ROLE_NONE;
  roster[nRoster].meetings = 0;
  roster[nRoster].immortal = false;
  roster[nRoster].lastSeen = millis();
  if (strcmp(id, s_myId) == 0) s_myColor = colorIdx;
  nRoster++;
  return true;
}

bool assignUniqueColors() {
  if (nRoster > NCOLORS) return false;
  int bank[NCOLORS];
  int bankSize = NCOLORS;
  for (int i = 0; i < NCOLORS; i++) bank[i] = i;

  // Sort by stable badge ID so every badge produces the same map even if
  // presence packets arrived in a different order.
  int order[MAX_PLAYERS];
  for (int i = 0; i < nRoster; i++) order[i] = i;
  for (int i = 0; i < nRoster; i++) {
    for (int j = i + 1; j < nRoster; j++) {
      if (strcmp(roster[order[j]].id, roster[order[i]].id) < 0) {
        int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
      }
    }
  }

  for (int p = 0; p < nRoster; p++) {
    int i = order[p];
    int chosen = takeColor(bank, &bankSize, preferredColorForId(roster[i].id));
    if (chosen < 0) return false;
    roster[i].colorIdx = chosen;
    if (strcmp(roster[i].id, s_myId) == 0) s_myColor = chosen;
  }
  return true;
}

void lockColorAssignments() { colorAssignmentsLocked = true; }
void unlockColorAssignments() { colorAssignmentsLocked = false; }

int rosterCount() { return nRoster; }
const char *rosterId(int i) { return roster[i].id; }
int rosterColorIndex(int i) { return roster[i].colorIdx; }
int rosterIndexOfId(const char *id) {
  for (int i = 0; i < nRoster; i++)
    if (strncmp(roster[i].id, id, ID_LEN) == 0) return i;
  return -1;
}

void resetPlayerStates() {
  for (int i = 0; i < nRoster; i++) {
    roster[i].alive = true;
    roster[i].role = ROLE_NONE;
    roster[i].meetings = 0;
  }
}

bool aliveIdx(int i) { return roster[i].alive; }

void setAliveId(const char *id, bool a) {
  int i = rosterIndexOfId(id);
  if (i >= 0) roster[i].alive = a;
}

void setAliveList(const char *csv) {
  // everyone not named in the list becomes dead
  for (int i = 0; i < nRoster; i++) roster[i].alive = false;
  char buf[128];
  strncpy(buf, csv, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char *tok = strtok(buf, ",");
  while (tok) {
    int i = rosterIndexOfId(tok);
    if (i >= 0) roster[i].alive = true;
    tok = strtok(NULL, ",");
  }
}

int roleIdx(int i) { return roster[i].role; }
void setRoleId(const char *id, int role) {
  int i = rosterIndexOfId(id);
  if (i >= 0) roster[i].role = role;
}

int meetingsIdx(int i) { return roster[i].meetings; }
void incMeetingsId(const char *id) {
  int i = rosterIndexOfId(id);
  if (i >= 0) roster[i].meetings++;
}

int aliveCount() {
  int n = 0;
  for (int i = 0; i < nRoster; i++) if (roster[i].alive) n++;
  return n;
}
int aliveRoleCount(int role) {
  int n = 0;
  for (int i = 0; i < nRoster; i++) if (roster[i].alive && roster[i].role == role) n++;
  return n;
}

bool immortalIdx(int i) { return roster[i].immortal; }

void setImmortalId(const char *id, bool immortal) {
  int i = rosterIndexOfId(id);
  if (i >= 0) roster[i].immortal = immortal;
}
