#pragma once

// Crew tasks. Four NFC tags each map to a different minigame. Scanning a tag
// you haven't finished opens its minigame; completing it marks that task done.
// Everyone plays the same four; impostors can play as cover (the game just
// doesn't count theirs).

#define NUM_TASKS 4

void setupTasks();
void resetTasks();                    // new game: clear my completed tasks

// Scan a tag: starts its minigame if I haven't done it. Returns true if started.
bool taskTryStart(const char *uid);
bool taskActive();
void taskUpdate();                    // run + render the current minigame
void taskCancel();                    // abort (meeting called, B pressed, etc.)

int taskJustCompleted();              // task index finished this frame, else -1
