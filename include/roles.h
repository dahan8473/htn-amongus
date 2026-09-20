#pragma once

// Game phase + secret roles + the start-of-game role reveal.
// One badge starts the game (host): it assigns roles randomly across the
// roster and sends each player their own role privately over the broadcast.
// At the start you press A to peek at your role card for 5 seconds.

enum GamePhase { PHASE_LOBBY, PHASE_PLAY };
enum Role { ROLE_NONE, ROLE_CREW, ROLE_IMP };

#define PLAY_MSG    "PLAY"     // game has started
#define ENDGAME_MSG "ENDGAME"  // back to lobby
// per-player role is sent as "ROLE:<id>:C" or "ROLE:<id>:I"

void setupRoles();
GamePhase gamePhase();
Role myRole();

void startGameAsHost();                    // assign + broadcast roles, begin play
void handleGameMessage(const char *msg);   // ROLE:/PLAY/ENDGAME dispatch
void requestRoleReveal();                  // A at start: show role card 5s
void updateRoles();                        // manage the reveal timer
bool isRevealing();
