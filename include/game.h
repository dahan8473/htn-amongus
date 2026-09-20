#pragma once

// The whole game as one host-authoritative state machine. The badge that
// presses START in the lobby becomes host: it owns the phase, timers, role
// assignment, vote tally, ejections and win checks, and broadcasts results.
// Every other badge follows the host's messages and sends action requests.

enum GPhase {
  G_LOBBY,     // title + settings; START begins the game
  G_PLAYING,   // roles dealt; press A to peek your role card
  G_GATHER,    // meeting called, blinking red, waiting for A
  G_DISCUSS,   // discussion countdown
  G_VOTING,    // pick a color, A to vote
  G_RESULT,    // who was ejected (+ impostor reveal)
  G_OVER,      // winner screen
};

void setupGame();
void gameHandleMessage(const char *msg);  // apply host messages / act on requests
void gameUpdate();                         // per-loop: input, timers, rendering
void gameOnNfc(const char *uid);           // an NFC tag was scanned -> start its task
GPhase gamePhase();
