#include <Arduino.h>
#include "tasktest.h"
#include "tasks.h"
#include "buttons.h"
#include "display.h"
#include "leds.h"
#include "nfc.h"

static const char *TASK_NAMES[NUM_TASKS] = { "WIRES", "WINDOW WIPE", "GARBAGE", "RHYTHM" };
#define N_ROWS (NUM_TASKS + 1)   // the games + an "ASSIGN TAGS" row

enum Mode { MENU, TASK, ASSIGN, ASSIGN_DONE };
static Mode mode = MENU;
static int  sel = 0;
static bool drawn = false;
static int  assignStep = 0;
static unsigned long assignReadyAt = 0;

static void drawMenu() {
  uint16_t bg = gfxColor(12, 14, 30), white = gfxColor(255, 255, 255);
  uint16_t dim = gfxColor(140, 140, 160), hi = gfxColor(70, 210, 90);
  gfxClear(bg);
  gfxText(60, 10, 3, white, "TASK TEST");
  for (int i = 0; i < N_ROWS; i++) {
    int y = 54 + i * 30;
    bool s = (i == sel);
    const char *label = (i < NUM_TASKS) ? TASK_NAMES[i] : "ASSIGN TAGS";
    if (s) gfxFillRect(28, y - 3, 264, 26, gfxColor(40, 60, 40));
    gfxText(44, y, 3, s ? hi : dim, label);
  }
  gfxText(30, 224, 2, dim, "A=play  START=exit");
  drawn = true;
}

static void drawAssignPrompt() {
  uint16_t bg = gfxColor(12, 14, 30), white = gfxColor(255, 255, 255);
  uint16_t dim = gfxColor(140, 140, 160), hi = gfxColor(240, 210, 70);
  gfxClear(bg);
  gfxText(40, 12, 3, white, "ASSIGN TAGS");
  char step[16]; snprintf(step, sizeof(step), "Tag %d of %d", assignStep + 1, NUM_TASKS);
  gfxText(110, 52, 2, dim, step);
  gfxText(30, 98, 2, white, "Tap a tag for:");
  gfxText(30, 132, 3, hi, TASK_NAMES[assignStep]);
  gfxText(20, 220, 2, dim, "START = cancel");
  drawn = true;
}

static void drawAssignDone() {
  uint16_t bg = gfxColor(12, 14, 30), white = gfxColor(255, 255, 255);
  uint16_t hi = gfxColor(70, 210, 90), dim = gfxColor(140, 140, 160);
  gfxClear(bg);
  gfxText(80, 60, 3, hi, "ALL SET");
  gfxText(24, 120, 2, white, "4 tags saved to flash");
  gfxText(30, 200, 2, dim, "START = menu");
  drawn = true;
}

void taskTestLoop() {
  switch (mode) {
    case TASK:
      if (isButtonPressed(BTN_START)) taskCancel();
      taskUpdate();
      if (!taskActive()) {
        if (taskJustCompleted() >= 0) flashLEDs(0, 220, 0, 300);
        mode = MENU; drawn = false;
      }
      return;

    case ASSIGN:
      if (!drawn) drawAssignPrompt();
      if (isButtonPressed(BTN_START)) { powerDownNFC(); mode = MENU; drawn = false; return; }
      if (millis() < assignReadyAt) return;            // wait for you to lift the last tag
      {
        String uid = scanNFC();
        if (uid != "") {
          assignTag(assignStep, uid.c_str());
          flashLEDs(0, 200, 0, 150);
          assignStep++;
          assignReadyAt = millis() + 700;
          drawn = false;
          if (assignStep >= NUM_TASKS) { powerDownNFC(); mode = ASSIGN_DONE; }
        }
      }
      return;

    case ASSIGN_DONE:
      if (!drawn) drawAssignDone();
      if (isButtonPressed(BTN_START) || isButtonPressed(BTN_A)) { mode = MENU; drawn = false; }
      return;

    case MENU:
    default:
      if (!drawn) drawMenu();
      if (isButtonPressed(BTN_UP))   { sel = (sel + N_ROWS - 1) % N_ROWS; drawn = false; }
      if (isButtonPressed(BTN_DOWN)) { sel = (sel + 1) % N_ROWS; drawn = false; }
      if (isButtonPressed(BTN_A)) {
        if (sel < NUM_TASKS) { taskStartIndex(sel); mode = TASK; }
        else { assignStep = 0; assignReadyAt = 0; beginNFCScan(); mode = ASSIGN; drawn = false; }
      }
      return;
  }
}
