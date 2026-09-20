#pragma once

// ESP-NOW broadcast transport: every badge can send a short tagged message that
// all the others receive (peer-to-peer, no router/AP or broker in the middle).
void setupBroadcast();

// Broadcast a message string to every other badge over ESP-NOW.
void broadcastMessage(const char *msg);

// Non-blocking receive. Copies one waiting message into buf (NUL-terminated)
// and returns its length, or 0 if nothing is waiting.
int pollMessage(char *buf, int maxLen);
