#include <Arduino.h>
#include <esp_random.h>
#include <string.h>
#include "tasks.h"
#include "buttons.h"
#include "imu.h"
#include "display.h"

#define TASK_TIMEOUT_MS 20000

static bool doneTask[NUM_TASKS];
static int curTask = -1;          // which minigame is running (-1 = none)
static bool drewStatic = false;
static unsigned long taskStart = 0;
static int justCompleted = -1;

// per-minigame state
static int seq[6], seqPos;                 // 0: wires
static int shakeFill; static unsigned long lastShake;  // 1: shake
static unsigned long levelSince;           // 2: stabilize
static int calRound; static float calPos, calDir, calZoneL, calZoneW;  // 3: calibrate

static uint16_t NAVY, WHITE, GREEN, RED, YELLOW, DIM;

void setupTasks() {
  NAVY = gfxColor(10, 12, 34); WHITE = gfxColor(255, 255, 255);
  GREEN = gfxColor(70, 210, 90); RED = gfxColor(220, 60, 60);
  YELLOW = gfxColor(240, 220, 60); DIM = gfxColor(150, 150, 170);
  resetTasks();
}

void resetTasks() {
  for (int i = 0; i < NUM_TASKS; i++) doneTask[i] = false;
  curTask = -1;
}

// -1 = normal (map each tag by hash). 0-3 = TEST: every tag launches this game.
// Set to 0 to make any tag open Wires for single-tag testing.
#define FORCE_TASK 0

static int uidToTask(const char *uid) {
  if (FORCE_TASK >= 0) return FORCE_TASK;
  uint32_t h = 0;
  for (const char *p = uid; *p; p++) h = h * 131u + (uint8_t)*p;
  return h % NUM_TASKS;
}

static void enterTask(int t) {
  curTask = t;
  drewStatic = false;
  taskStart = millis();
  justCompleted = -1;
  if (t == 0) { for (int i = 0; i < 5; i++) seq[i] = esp_random() % 6; seqPos = 0; }
  else if (t == 1) { shakeFill = 0; lastShake = 0; }
  else if (t == 2) { levelSince = 0; }
  else if (t == 3) { calRound = 0; calPos = 0; calDir = 2.2f; calZoneL = 62; calZoneW = 26; }
}

bool taskTryStart(const char *uid) {
  if (curTask >= 0) return false;
  int t = uidToTask(uid);
  if (doneTask[t]) return false;
  enterTask(t);
  return true;
}

bool taskActive() { return curTask >= 0; }

void taskCancel() { curTask = -1; }

int taskJustCompleted() { int j = justCompleted; justCompleted = -1; return j; }

static void finish() {
  doneTask[curTask] = true;
  justCompleted = curTask;
  curTask = -1;
}

// ---- minigame renderers/updaters ----
// input types: 0 UP, 1 DOWN, 2 LEFT, 3 RIGHT, 4 A, 5 B
#define WIRE_LEN 5

// draw one sequence glyph centered at (cx,cy): arrows for 0-3, letters for A/B
static void drawGlyph(int cx, int cy, int type, uint16_t c) {
  int s = 12;
  switch (type) {
    case 0: gfxFillTriangle(cx, cy - s, cx - s, cy + s, cx + s, cy + s, c); break;  // up
    case 1: gfxFillTriangle(cx, cy + s, cx - s, cy - s, cx + s, cy - s, c); break;  // down
    case 2: gfxFillTriangle(cx - s, cy, cx + s, cy - s, cx + s, cy + s, c); break;  // left
    case 3: gfxFillTriangle(cx + s, cy, cx - s, cy - s, cx - s, cy + s, c); break;  // right
    case 4: gfxText(cx - 8, cy - 10, 3, c, "A"); break;
    case 5: gfxText(cx - 8, cy - 10, 3, c, "B"); break;
  }
}

static void runWires() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(90, 18, 3, WHITE, "WIRES");
    gfxText(30, 205, 2, DIM, "match the sequence");
    drewStatic = true;
  }
  int d = -1;
  if (isButtonPressed(BTN_UP)) d = 0;
  else if (isButtonPressed(BTN_DOWN)) d = 1;
  else if (isButtonPressed(BTN_LEFT)) d = 2;
  else if (isButtonPressed(BTN_RIGHT)) d = 3;
  else if (isButtonPressed(BTN_A)) d = 4;
  else if (isButtonPressed(BTN_B)) d = 5;
  if (d >= 0 && d == seq[seqPos]) seqPos++;
  if (seqPos >= WIRE_LEN) { finish(); return; }
  // arrow/button row (redraw on change)
  gfxFillRect(10, 90, 300, 60, NAVY);
  for (int i = 0; i < WIRE_LEN; i++) {
    uint16_t c = (i < seqPos) ? GREEN : (i == seqPos ? YELLOW : DIM);
    drawGlyph(35 + i * 58, 120, seq[i], c);
  }
}

static void runShake() {
  if (!drewStatic) { gfxClear(NAVY); gfxText(50, 20, 3, WHITE, "SHAKE IT!"); drewStatic = true; }
  float mag = getAccelMagnitude();
  if (mag > 1.8f && millis() - lastShake > 140) { shakeFill++; lastShake = millis(); }
  if (shakeFill >= 16) { finish(); return; }
  gfxFillRect(30, 110, 260, 30, gfxColor(40, 40, 60));
  gfxFillRect(30, 110, 260 * shakeFill / 16, 30, GREEN);
  gfxRectOutline(30, 110, 260, 30, WHITE);
}

static void runStabilize() {
  if (!drewStatic) { gfxClear(NAVY); gfxText(40, 16, 3, WHITE, "STABILIZE"); gfxText(30, 210, 2, DIM, "hold it level 3s"); drewStatic = true; }
  float r, p; getRollPitch(r, p);
  bool level = (fabs(r) < 12 && fabs(p) < 12);
  if (level) { if (levelSince == 0) levelSince = millis(); }
  else levelSince = 0;
  if (levelSince && millis() - levelSince >= 3000) { finish(); return; }
  int cx = 160, cy = 120;
  gfxFillRect(60, 60, 200, 120, NAVY);        // clear play area
  gfxRectOutline(cx - 22, cy - 22, 44, 44, level ? GREEN : DIM);  // target zone
  int bx = cx + (int)(p * 3.0f), by = cy + (int)(r * 3.0f);
  if (bx < 70) bx = 70; if (bx > 250) bx = 250; if (by < 70) by = 70; if (by > 170) by = 170;
  gfxFillCircle(bx, by, 10, level ? GREEN : YELLOW);
  int held = levelSince ? (int)(millis() - levelSince) : 0;
  gfxFillRect(60, 190, 200 * held / 3000, 8, GREEN);
}

static void runCalibrate() {
  if (!drewStatic) { gfxClear(NAVY); gfxText(40, 16, 3, WHITE, "CALIBRATE"); gfxText(20, 210, 2, DIM, "A in the green zone"); drewStatic = true; }
  calPos += calDir;
  if (calPos <= 0) { calPos = 0; calDir = -calDir; }
  if (calPos >= 100) { calPos = 100; calDir = -calDir; }
  if (isButtonPressed(BTN_A)) {
    if (calPos >= calZoneL && calPos <= calZoneL + calZoneW) {
      calRound++;
      if (calRound >= 3) { finish(); return; }
      calZoneW *= 0.65f; calZoneL = 15 + esp_random() % 60;  // shrink + move
    }
  }
  // bar 30..290 maps 0..100
  int barX = 30, barW = 260;
  gfxFillRect(barX, 110, barW, 30, gfxColor(40, 40, 60));
  gfxFillRect(barX + (int)(barW * calZoneL / 100), 110, (int)(barW * calZoneW / 100), 30, GREEN);
  gfxFillRect(barX + (int)(barW * calPos / 100) - 2, 104, 4, 42, WHITE);
  gfxRectOutline(barX, 110, barW, 30, WHITE);
  char b[16]; snprintf(b, sizeof(b), "Round %d/3", calRound + 1);
  gfxText(110, 160, 2, DIM, b);
}

void taskUpdate() {
  if (curTask < 0) return;
  // no button cancel: A/B/d-pad are all game inputs. Ends on completion,
  // a 20s timeout, or a meeting (game.cpp cancels on phase change).
  if (millis() - taskStart > TASK_TIMEOUT_MS) { taskCancel(); return; }
  switch (curTask) {
    case 0: runWires(); break;
    case 1: runShake(); break;
    case 2: runStabilize(); break;
    case 3: runCalibrate(); break;
  }
}
