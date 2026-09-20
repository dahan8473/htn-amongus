#pragma once
#include <Arduino.h>
#include <stdint.h>

// Player identity + color + roster, plus per-player game state (alive, role,
// meetings used). Each badge derives a stable id and initial color from its
// MAC; the host assigns unique colors from the shared palette at game start.

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
bool setColorId(const char *id, int colorIdx);
bool assignUniqueColors();
void lockColorAssignments();
void unlockColorAssignments();

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

// Demo-mode immortality (AUX1 switch): an immortal player can't be killed or
// even targeted, regardless of role or proximity. Persists across rounds
// (it's a hardware switch state, not part of resetPlayerStates()).
bool immortalIdx(int i);
void setImmortalId(const char *id, bool immortal);
