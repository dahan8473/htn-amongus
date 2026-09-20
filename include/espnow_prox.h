#pragma once

// ESP-NOW proximity ranging. Each badge broadcasts its short player ID and
// records the RSSI of beacons received from other badges. ESP-NOW uses the
// current Wi-Fi channel, so it can run beside the router-backed UDP game bus.

void setupProximity(const char *myId);
void updateProximity();

// Smoothed signal strength (dBm, higher means a stronger signal) for a badge
// seen recently, or -127 if it has not been seen for three seconds.
int proximityRssi(const char *id);

// Print the currently tracked badge IDs and smoothed RSSI values to Serial.
// Intended for low-rate diagnostics while tuning proximity thresholds.
void debugProximity();
