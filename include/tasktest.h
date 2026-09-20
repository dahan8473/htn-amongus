#pragma once

// Solo task-test harness. Hold A while the badge boots to enter it (START is the
// GPIO9 boot strapping pin, so it can't be the trigger). Runs a button menu that
// launches each minigame directly and assigns NFC tags -- no WiFi or other badges
// needed. Not part of the real game; it only activates on the boot-hold.
void taskTestLoop();
