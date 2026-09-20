#include <Arduino.h>
#include "tasktest.h"
#include "tasks.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"

static const char *TASK_NAMES[NUM_TASKS] = { "WIRES", "WINDOW WIPE", "GARBAGE", "CALIBRATE" };

static int  sel = 0;
static bool inTask = false;
static bool menuDrawn = false;

static void drawMenu() {
  uint16_t bg    = gfxColor(12, 14, 30);
  uint16_t white = gfxColor(255, 255, 255);
  uint16_t dim   = gfxColor(140, 140, 160);
  uint16_t hi    = gfxColor(70, 210, 90);

  gfxClear(bg);
  gfxText(60, 14, 3, white, "TASK TEST");
  for (int i = 0; i < NUM_TASKS; i++) {
    int y = 64 + i * 34;
    bool s = (i == sel);
    if (s) gfxFillRect(28, y - 4, 264, 30, gfxColor(40, 60, 40));
    gfxText(44, y, 3, s ? hi : dim, TASK_NAMES[i]);
  }
  gfxText(30, 214, 2, dim, "A=play  START=exit");
  menuDrawn = true;
}

void taskTestLoop() {
  // running a minigame: START aborts back to the menu
  if (inTask) {
    if (isButtonPressed(BTN_START)) taskCancel();
    taskUpdate();
    if (!taskActive()) {
      if (taskJustCompleted() >= 0) flashLEDs(0, 220, 0, 300);  // completed, not aborted
      inTask = false;
      menuDrawn = false;
    }
    return;
  }

  // menu
  if (!menuDrawn) drawMenu();
  if (isButtonPressed(BTN_UP))   { sel = (sel + NUM_TASKS - 1) % NUM_TASKS; menuDrawn = false; }
  if (isButtonPressed(BTN_DOWN)) { sel = (sel + 1) % NUM_TASKS; menuDrawn = false; }
  if (isButtonPressed(BTN_A))    { taskStartIndex(sel); inTask = true; }
}
