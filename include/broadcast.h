#pragma once

// UDP broadcast alert used to verify multiple badges can signal each other
// over WiFi. When any badge's START button is pressed, it broadcasts an
// alert onto the LAN; every badge that hears it (including the sender)
// flashes its LEDs red for a few seconds -- a visual, no-laptop-needed test
// that broadcast reaches every badge.
void setupBroadcast();
void updateBroadcast();
