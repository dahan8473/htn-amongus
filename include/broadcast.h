#pragma once

// ESP-NOW TTL-flood mesh: every badge reaches every other badge through this,
// without a WiFi router in the picture at all. A message from any badge is
// relayed by whoever hears it (each hop decrements TTL, bounded well under
// the max player count so it can't outlive the mesh) until TTL hits zero. A
// small dedup cache stops each badge from re-relaying a message it's already
// forwarded, which is what keeps this from becoming a broadcast storm.
void setupBroadcast();

// Fires any relays that are due (jittered slightly after receipt so nearby
// badges don't all rebroadcast in the same instant and collide on air).
// Call every loop().
void updateBroadcast();

// Sends a message string to every badge in mesh range (possibly multiple
// hops away).
void broadcastMessage(const char *msg);

// Non-blocking receive. Copies one waiting message into buf (NUL-terminated)
// and returns its length, or 0 if nothing is waiting.
int pollMessage(char *buf, int maxLen);
