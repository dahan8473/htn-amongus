#pragma once

// WiFi UDP broadcast transport: every badge on the game network can send a
// short tagged message that all the others (and itself) receive. This is the
// pipe the game rides on -- no laptop or broker in the middle.
void setupBroadcast();

// Send a message string to every badge on the subnet.
void broadcastMessage(const char *msg);

// Non-blocking receive. Copies one waiting message into buf (NUL-terminated)
// and returns its length, or 0 if nothing is waiting.
int pollMessage(char *buf, int maxLen);
