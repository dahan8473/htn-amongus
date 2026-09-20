#pragma once
#include <Arduino.h>
#include <stdint.h>

// Player identity + color profiles + a roster of everyone we've heard from.
// Each badge derives a stable short ID and a color from its own MAC, then
// announces itself over the WiFi broadcast so every badge can list players
// and know who's who by color (used by role reveal and voting).

#define MAX_PLAYERS   12
#define ID_LEN        5   // 4 hex chars + NUL

struct PlayerColor { const char *name; uint8_t r, g, b; };

void setupPlayers();               // derive my id + color from the MAC
const char *myId();                // e.g. "A3F2"
int myColorIndex();
int colorCount();
const PlayerColor &colorByIndex(int i);

void sendPresence();               // broadcast "ID:<id>:<colorIdx>"
void notePresence(const char *id, int colorIdx);  // record a heard player

// Roster of distinct players heard recently (includes ourselves).
int rosterCount();
const char *rosterId(int i);
int rosterColorIndex(int i);
int rosterIndexOfId(const char *id);  // -1 if unknown
