#pragma once
#include <Arduino.h>
#include <stdint.h>

// Player identity + color + roster, plus per-player game state (alive, role,
// meetings used). Each badge derives a stable id + color from its MAC and
// beacons presence so every badge builds the same roster.

#define MAX_PLAYERS 12
#define ID_LEN      5   // 4 hex chars + NUL

#define ROLE_NONE 0
#define ROLE_CREW 1
#define ROLE_IMP  2

struct PlayerColor { const char *name; uint8_t r, g, b; };

void setupPlayers();
const char *myId();
int myColorIndex();
int colorCount();
const PlayerColor &colorByIndex(int i);

void sendPresence();
void notePresence(const char *id, int colorIdx);

int rosterCount();
const char *rosterId(int i);
int rosterColorIndex(int i);
int rosterIndexOfId(const char *id);   // -1 if unknown

// ---- per-player game state ----
void resetPlayerStates();              // all alive, role none, meetings 0
bool aliveIdx(int i);
void setAliveId(const char *id, bool a);
void setAliveList(const char *csv);    // ids in list -> alive, others -> dead
int roleIdx(int i);
void setRoleId(const char *id, int role);
int meetingsIdx(int i);
void incMeetingsId(const char *id);
int aliveCount();
int aliveRoleCount(int role);
