#include <Arduino.h>
#include <esp_random.h>
#include <string.h>
#include "tasks.h"
#include "buttons.h"
#include "imu.h"
#include "display.h"
#include "leds.h"

#define TASK_TIMEOUT_MS 20000

static bool doneTask[NUM_TASKS];
static int curTask = -1;          // which minigame is running (-1 = none)
static bool drewStatic = false;
static unsigned long taskStart = 0;
static int justCompleted = -1;

// per-minigame state
static int seq[6], seqPos, wireRound;      // 0: wires (multiple sequences)
// 1: window cleaning -- wipe the badge like a sponge; harder wipe = faster squeegee + faster clean
#define WIN_NCOLS 14
#define WIN_GMAX  3.0f
static float winGrime[WIN_NCOLS];   // remaining grime per column (0..WIN_GMAX)
static int   winShade[WIN_NCOLS];   // last-drawn shade per column (so we only repaint on change)
static float winSqX, winPrevSqX;    // squeegee left edge, px
static int   winSqDir;              // +1 / -1
static float winWipeSm;            // smoothed wipe force
static int   winPct;               // last-drawn percent
static float navX, navY, navVX, navVY, navPX, navPY;   // 2: navigate (tilt-roll)
static int navTX, navTY, navHits;
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
// 0 Wires, 1 Window wipe, 2 Garbage, 3 Calibrate.
#define FORCE_TASK 1

static int uidToTask(const char *uid) {
  if (FORCE_TASK >= 0) return FORCE_TASK;
  uint32_t h = 0;
  for (const char *p = uid; *p; p++) h = h * 131u + (uint8_t)*p;
  return h % NUM_TASKS;
}

static void newChute();  // defined with the garbage minigame below
static void genWalls();

static void enterTask(int t) {
  curTask = t;
  drewStatic = false;
  taskStart = millis();
  justCompleted = -1;
  if (t == 0) { for (int i = 0; i < 5; i++) seq[i] = esp_random() % 6; seqPos = 0; wireRound = 0; }
  else if (t == 1) {
    for (int i = 0; i < WIN_NCOLS; i++) { winGrime[i] = WIN_GMAX; winShade[i] = -1; }
    winSqX = winPrevSqX = 20; winSqDir = 1; winWipeSm = 0; winPct = -1;
  }
  else if (t == 2) {
    navX = navPX = 55; navY = navPY = 185; navVX = navVY = 0; navHits = 0;
    genWalls();
    newChute();
  }
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
#define WIRE_LEN    5   // glyphs per sequence
#define WIRE_ROUNDS 3   // sequences to complete for the whole task

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
    gfxText(90, 14, 3, WHITE, "WIRES");
    char r[16]; snprintf(r, sizeof(r), "Round %d/%d", wireRound + 1, WIRE_ROUNDS);
    gfxText(105, 190, 2, DIM, r);
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
  if (seqPos >= WIRE_LEN) {
    wireRound++;
    if (wireRound >= WIRE_ROUNDS) { finish(); return; }
    for (int i = 0; i < WIRE_LEN; i++) seq[i] = esp_random() % 6;  // next sequence
    seqPos = 0;
    drewStatic = false;   // redraw title + new round number
    return;
  }
  // arrow/button row (redraw on change)
  gfxFillRect(10, 95, 300, 60, NAVY);
  for (int i = 0; i < WIRE_LEN; i++) {
    uint16_t c = (i < seqPos) ? GREEN : (i == seqPos ? YELLOW : DIM);
    drawGlyph(35 + i * 58, 125, seq[i], c);
  }
}

// ---- window cleaning: hold the badge like a sponge and wipe the air ----
// The squeegee ping-pongs across the glass; the harder you wipe (accel force),
// the faster it sweeps AND the faster it wipes grime off the panes under it.
// Grime clears in shades so you literally see where you've wiped. Done at 85%.
#define WX     20
#define WY     44
#define WW     280
#define WH     150
#define WCOLW  (WW / WIN_NCOLS)   // 20 px
#define WSQW   24

static uint16_t grimeColor(int shade) {
  switch (shade) {
    case 3: return gfxColor(74, 66, 40);      // caked grime
    case 2: return gfxColor(120, 110, 70);
    case 1: return gfxColor(170, 165, 130);   // filmy
    default: return gfxColor(150, 205, 235);  // 0 = clear glass
  }
}

static int grimeShade(float g) {
  int s = (int)floorf((g / WIN_GMAX) * 3.0f + 0.5f);
  if (s < 0) s = 0;
  if (s > 3) s = 3;
  return s;
}

static void runWindow() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(48, 12, 3, WHITE, "WINDOW WIPE");
    gfxRectOutline(WX - 3, WY - 3, WW + 6, WH + 6, gfxColor(90, 90, 110));
    for (int c = 0; c < WIN_NCOLS; c++) {
      int s = grimeShade(winGrime[c]);
      gfxFillRect(WX + c * WCOLW, WY, WCOLW, WH, grimeColor(s));
      winShade[c] = s;
    }
    gfxRectOutline(30, 208, 260, 18, WHITE);
    drewStatic = true;
  }

  // wipe force: accel magnitude is ~1.0 at rest, spikes when you wave the badge
  float mag = getAccelMagnitude();
  float wipe = fabsf(mag - 1.0f);
  winWipeSm = winWipeSm * 0.6f + wipe * 0.4f;

  // squeegee sweeps; speed scales with how hard you're wiping (creeps when still)
  winSqX += winSqDir * (0.4f + winWipeSm * 26.0f);
  if (winSqX <= WX) { winSqX = WX; winSqDir = 1; }
  if (winSqX >= WX + WW - WSQW) { winSqX = WX + WW - WSQW; winSqDir = -1; }

  // wipe grime off the columns under the squeegee, scaled by force
  int c0 = (int)((winSqX - WX) / WCOLW);
  int c1 = (int)((winSqX + WSQW - WX) / WCOLW);
  if (c0 < 0) c0 = 0;
  if (c1 > WIN_NCOLS - 1) c1 = WIN_NCOLS - 1;
  if (winWipeSm > 0.22f) {
    for (int c = c0; c <= c1; c++) {
      if (winGrime[c] > 0) {
        winGrime[c] -= winWipeSm * 0.9f;
        if (winGrime[c] < 0) winGrime[c] = 0;
      }
    }
  }

  // repaint any column whose shade changed -- this is the visible "wiped" trail
  for (int c = 0; c < WIN_NCOLS; c++) {
    int s = grimeShade(winGrime[c]);
    if (s != winShade[c]) {
      if (s == 0 && winShade[c] != 0) flashLEDs(0, 170, 0, 100);  // a pane came fully clean
      gfxFillRect(WX + c * WCOLW, WY, WCOLW, WH, grimeColor(s));
      winShade[c] = s;
    }
  }

  // erase old squeegee (repaint its columns at their current grime), draw new
  int pc0 = (int)((winPrevSqX - WX) / WCOLW);
  int pc1 = (int)((winPrevSqX + WSQW - WX) / WCOLW);
  if (pc0 < 0) pc0 = 0;
  if (pc1 > WIN_NCOLS - 1) pc1 = WIN_NCOLS - 1;
  for (int c = pc0; c <= pc1; c++)
    gfxFillRect(WX + c * WCOLW, WY, WCOLW, WH, grimeColor(winShade[c]));

  gfxFillRect((int)winSqX, WY, WSQW, WH, gfxColor(210, 225, 245));            // blade body
  gfxFillRect((int)winSqX, WY, WSQW, 8, gfxColor(240, 210, 70));             // handle
  gfxFillRect((int)winSqX, WY + WH - 6, WSQW, 6, gfxColor(70, 80, 100));     // rubber
  winPrevSqX = winSqX;

  // progress
  float sum = 0;
  for (int c = 0; c < WIN_NCOLS; c++) sum += winGrime[c];
  int pct = (int)((WIN_NCOLS * WIN_GMAX - sum) / (WIN_NCOLS * WIN_GMAX) * 100.0f);
  if (pct != winPct) {
    winPct = pct;
    gfxFillRect(31, 209, 258, 16, gfxColor(40, 40, 60));
    gfxFillRect(31, 209, 258 * pct / 100, 16, GREEN);
    gfxFillRect(250, 12, 70, 20, NAVY);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    gfxText(250, 12, 2, WHITE, b);
  }
  if (pct >= 85) { flashLEDs(0, 220, 0, 400); finish(); return; }
}

// walls the trash must navigate around (randomized each attempt)
struct Wall { int x, y, w, h; };
#define NWALLS 3
#define BALL_R 8
static Wall walls[NWALLS];

static void genWalls() {
  for (int i = 0; i < NWALLS; i++) {
    for (int tries = 0; tries < 12; tries++) {
      bool horiz = esp_random() % 2;
      int w = horiz ? (80 + esp_random() % 60) : 14;
      int h = horiz ? 14 : (50 + esp_random() % 50);
      int x = 15 + esp_random() % (306 - w - 15);
      int y = 52 + esp_random() % (196 - h - 52);
      if (x < 100 && y > 150) continue;  // keep the ball's start corner clear
      walls[i] = { x, y, w, h };
      break;
    }
  }
}

static bool hitsWall(float x, float y) {
  for (int i = 0; i < NWALLS; i++) {
    const Wall &w = walls[i];
    if (x + BALL_R > w.x && x - BALL_R < w.x + w.w &&
        y + BALL_R > w.y && y - BALL_R < w.y + w.h) return true;
  }
  return false;
}

static void newChute() {
  do {
    navTX = 40 + esp_random() % 240;
    navTY = 60 + esp_random() % 140;
  } while (hitsWall(navTX, navTY) || (abs(navTX - (int)navX) < 40 && abs(navTY - (int)navY) < 40));
}

static void runGarbage() {
  if (!drewStatic) {
    gfxClear(NAVY);
    gfxText(30, 8, 3, WHITE, "GARBAGE");
    gfxText(10, 222, 2, DIM, "tilt trash to the chute");
    drewStatic = true;
  }
  float r, p; getRollPitch(r, p);
  navVX += p * 0.06f; navVY += r * 0.06f;   // tilt accelerates the trash
  navVX *= 0.90f; navVY *= 0.90f;           // friction

  // move per-axis so the trash slides along walls instead of sticking
  float nx = navX + navVX;
  if (nx < 18) { nx = 18; navVX = -navVX * 0.5f; }
  if (nx > 302) { nx = 302; navVX = -navVX * 0.5f; }
  if (!hitsWall(nx, navY)) navX = nx; else navVX = -navVX * 0.3f;
  float ny = navY + navVY;
  if (ny < 40) { ny = 40; navVY = -navVY * 0.5f; }
  if (ny > 210) { ny = 210; navVY = -navVY * 0.5f; }
  if (!hitsWall(navX, ny)) navY = ny; else navVY = -navVY * 0.3f;

  float dx = navX - navTX, dy = navY - navTY;
  if (dx * dx + dy * dy < 22 * 22) {        // trash reached the chute
    navHits++;
    flashLEDs(0, 200, 0, 250);
    if (navHits >= 3) { finish(); return; }
    newChute();
    drewStatic = false;
    return;
  }

  gfxFillCircle((int)navPX, (int)navPY, 9, NAVY);          // erase old trash
  for (int i = 0; i < NWALLS; i++) gfxFillRect(walls[i].x, walls[i].y, walls[i].w, walls[i].h, gfxColor(110, 110, 125));
  gfxFillRect(navTX - 14, navTY - 14, 28, 28, gfxColor(30, 120, 40));  // chute
  gfxRectOutline(navTX - 14, navTY - 14, 28, 28, GREEN);
  gfxFillCircle((int)navX, (int)navY, BALL_R, gfxColor(150, 120, 80));  // trash
  navPX = navX; navPY = navY;
  char h[12]; snprintf(h, sizeof(h), "%d/3", navHits);
  gfxFillRect(282, 34, 36, 20, NAVY);
  gfxText(284, 36, 2, GREEN, h);
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
    case 1: runWindow(); break;
    case 2: runGarbage(); break;
    case 3: runCalibrate(); break;
  }
}
