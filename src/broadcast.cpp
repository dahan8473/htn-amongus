#include <Arduino.h>
#include "broadcast.h"
#include "espnow_prox.h"

// The game bus now rides ESP-NOW (see espnow_prox.cpp) instead of WiFi UDP, so
// no router/AP is needed. These are thin wrappers kept for the existing API;
// ESP-NOW itself is initialized in setupProximity().

void setupBroadcast() {
  // nothing to do -- ESP-NOW is brought up in setupProximity()
}

void broadcastMessage(const char *msg) {
  espnowSendMsg(msg);
}

int pollMessage(char *buf, int maxLen) {
  return espnowPollMsg(buf, maxLen);
}
