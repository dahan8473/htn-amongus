#pragma once
#include <Arduino.h>

// Bluetooth proximity ranging. Each badge advertises its short id over BLE and
// passively scans for others, recording how strong each one's signal is (RSSI).
// The game uses this to find the nearest player for kills and body reports.
// Runs alongside WiFi (the C3 shares one radio; kept scan-only + low memory).

void setupProximity(const char *myId);
void updateProximity();

// Signal strength (dBm, higher = closer) for a badge id seen in the last few
// seconds, or -127 if not seen / too far.
int proximityRssi(const char *id);
