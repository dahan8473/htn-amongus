#pragma once

// Solo task-test harness. Hold START while the badge boots to enter it. Runs a
// button menu that launches each minigame directly -- no WiFi, NFC, or other
// badges needed. Not part of the real game; it only activates on the boot-hold.
void taskTestLoop();
