#include <Arduino.h>
#include <esp_random.h>
#include <string.h>
#include <stdio.h>
#include "game.h"
#include "players.h"
#include "buttons.h"
#include "broadcast.h"
#include "display.h"
#include "imu.h"
#include "leds.h"
#include "espnow_prox.h"

#define KILL_RSSI    -66    // ~within a couple meters; tune on hardware
#define REPORT_RSSI  -66
#define KILL_HOLD_MS 700
#define KILL_CD_MS   20000

#define REVEAL_MS 5000
#define RESULT_MS 4000

// ---- synced settings (host broadcasts CFG in the lobby) ----
static int cfgImp = 1, cfgDisc = 30, cfgVote = 20, cfgMeet = 1;
static const int SET_ROWS = 4;
static int setSel = 0;  // which setting the lobby cursor is on

// ---- state ----
static GPhase phase = G_LOBBY;
static bool isHost = false;
static unsigned long phaseEnd = 0;      // for timed phases (discuss/voting)
static int myRole = ROLE_NONE;
static char impTeam[MAX_PLAYERS][ID_LEN];  // my fellow impostors (if I'm one)
static int nImpTeam = 0;

// hold START to peek your role card
static unsigned long startDownAt = 0;
static bool startPrevHeld = false;
static bool startIsReveal = false;
static bool revealActive = false;

// HOME: quick tap calls a meeting; hold shows the dev debug screen
static unsigned long homeDownAt = 0;
static bool homePrevHeld = false;
static bool homeIsHold = false;

// hold B to kill (impostor) or report a nearby body (anyone)
static unsigned long bDownAt = 0;
static bool bActed = false;
static unsigned long lastKillMs[MAX_PLAYERS] = { 0 };  // host kill cooldown
static unsigned long killedFlashUntil = 0;             // red flash when I die

static int voteSel = 0;
static bool myVoted = false;

// host-only vote collection
static char voters[MAX_PLAYERS][ID_LEN];
static char targets[MAX_PLAYERS][ID_LEN];
static int nVotes = 0;

// result / winner
static char ejId[ID_LEN] = "";
static bool ejSkipped = true, ejWasImp = false;
static char winSide = 0;  // 'C' or 'I'

// vote tally for the result screen (per color)
static uint8_t talColor[MAX_PLAYERS];
static int talCount[MAX_PLAYERS];
static int nTal = 0, talSkip = 0;

static void parseTally(const char *s) {
  nTal = 0; talSkip = 0;
  char buf[128]; strncpy(buf, s, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
  char *t = strtok(buf, ",");
  while (t) {
    if (t[0] == 'S') sscanf(t, "S=%d", &talSkip);
    else { int ci, n; if (sscanf(t, "%d=%d", &ci, &n) == 2 && nTal < MAX_PLAYERS) { talColor[nTal] = ci; talCount[nTal] = n; nTal++; } }
    t = strtok(NULL, ",");
  }
}

static bool needRedraw = true;
static int lastCd = -1;
static int lastLobbyPlayers = -1;

// ---------- helpers ----------
static void storeImpTeam(const char *csv) {
  nImpTeam = 0;
  char buf[128]; strncpy(buf, csv, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
  char *t = strtok(buf, ",");
  while (t) {
    if (strcmp(t, myId()) != 0 && nImpTeam < MAX_PLAYERS) {
      strncpy(impTeam[nImpTeam], t, ID_LEN); impTeam[nImpTeam][ID_LEN - 1] = 0; nImpTeam++;
    }
    t = strtok(NULL, ",");
  }
}

static int remainingSecs() {
  long r = (long)phaseEnd - (long)millis();
  return r <= 0 ? 0 : (int)((r + 999) / 1000);
}

static void applyPhase(GPhase p, int durSecs) {
  phase = p;
  phaseEnd = durSecs > 0 ? millis() + (unsigned long)durSecs * 1000 : 0;
  needRedraw = true;
  lastCd = -1;
  if (p == G_VOTING) { voteSel = 0; myVoted = false; }
}

static void setPhaseHost(GPhase p, int durSecs) {
  applyPhase(p, durSecs);
  char m[16];
  snprintf(m, sizeof(m), "PH:%d:%d", (int)p, durSecs);
  broadcastMessage(m);
}

static void broadcastAlive() {
  char csv[128] = "";
  for (int i = 0; i < rosterCount(); i++) {
    if (!aliveIdx(i)) continue;
    if (csv[0]) strncat(csv, ",", sizeof(csv) - strlen(csv) - 1);
    strncat(csv, rosterId(i), sizeof(csv) - strlen(csv) - 1);
  }
  char m[144];
  snprintf(m, sizeof(m), "AL:%s", csv);
  broadcastMessage(m);
}

static void broadcastCfg() {
  char m[32];
  snprintf(m, sizeof(m), "CFG:%d:%d:%d:%d", cfgImp, cfgDisc, cfgVote, cfgMeet);
  broadcastMessage(m);
}

// ---------- host actions ----------
static void hostStartGame() {
  isHost = true;
  resetPlayerStates();
  int n = rosterCount();
  int nImp = cfgImp; if (nImp >= n) nImp = 1; if (nImp < 1) nImp = 1;
  bool imp[MAX_PLAYERS] = { false };
  int chosen = 0;
  while (chosen < nImp && chosen < n) {
    int k = esp_random() % n;
    if (!imp[k]) { imp[k] = true; chosen++; }
  }
  // build the impostor id list so teammates can be shown on the role card
  char impCsv[128] = "";
  for (int i = 0; i < n; i++) {
    if (!imp[i]) continue;
    if (impCsv[0]) strncat(impCsv, ",", sizeof(impCsv) - strlen(impCsv) - 1);
    strncat(impCsv, rosterId(i), sizeof(impCsv) - strlen(impCsv) - 1);
  }
  for (int i = 0; i < n; i++) {
    int role = imp[i] ? ROLE_IMP : ROLE_CREW;
    setRoleId(rosterId(i), role);
    char m[160];
    if (imp[i]) snprintf(m, sizeof(m), "ROLE:%s:I:%s", rosterId(i), impCsv);
    else snprintf(m, sizeof(m), "ROLE:%s:C", rosterId(i));
    broadcastMessage(m);
    if (strcmp(rosterId(i), myId()) == 0) {
      myRole = role;
      if (role == ROLE_IMP) storeImpTeam(impCsv);
    }
  }
  broadcastCfg();
  broadcastAlive();
  setPhaseHost(G_PLAYING, 0);
}

static void hostRecordVote(const char *voter, const char *target) {
  for (int i = 0; i < nVotes; i++)
    if (strncmp(voters[i], voter, ID_LEN) == 0) return;
  if (nVotes < MAX_PLAYERS) {
    strncpy(voters[nVotes], voter, ID_LEN); voters[nVotes][ID_LEN - 1] = 0;
    strncpy(targets[nVotes], target, ID_LEN); targets[nVotes][ID_LEN - 1] = 0;
    nVotes++;
  }
}

static void hostTallyAndEject() {
  int counts[MAX_PLAYERS] = { 0 };
  for (int i = 0; i < nVotes; i++) {
    if (strncmp(targets[i], "SKIP", 4) == 0) continue;
    int idx = rosterIndexOfId(targets[i]);
    if (idx >= 0) counts[idx]++;
  }
  // majority of votes CAST: need > half of nVotes
  int best = -1, bestN = 0;
  for (int i = 0; i < rosterCount(); i++) if (counts[i] > bestN) { best = i; bestN = counts[i]; }
  ejSkipped = true; ejWasImp = false; ejId[0] = 0;
  if (best >= 0 && nVotes > 0 && bestN * 2 > nVotes) {
    ejSkipped = false;
    strncpy(ejId, rosterId(best), ID_LEN);
    ejWasImp = (roleIdx(best) == ROLE_IMP);
    setAliveId(ejId, false);
  }
  // build + broadcast the per-color tally for the result screen
  nTal = 0; talSkip = 0;
  int cc[16] = { 0 };
  for (int i = 0; i < nVotes; i++) {
    if (strncmp(targets[i], "SKIP", 4) == 0) { talSkip++; continue; }
    int idx = rosterIndexOfId(targets[i]);
    if (idx >= 0) cc[rosterColorIndex(idx)]++;
  }
  for (int ci = 0; ci < colorCount(); ci++)
    if (cc[ci] > 0 && nTal < MAX_PLAYERS) { talColor[nTal] = ci; talCount[nTal] = cc[ci]; nTal++; }
  char tm[128]; int off = snprintf(tm, sizeof(tm), "TAL:");
  for (int i = 0; i < nTal; i++) off += snprintf(tm + off, sizeof(tm) - off, "%d=%d,", talColor[i], talCount[i]);
  snprintf(tm + off, sizeof(tm) - off, "S=%d", talSkip);
  broadcastMessage(tm);

  broadcastAlive();
  char m[24];
  if (ejSkipped) snprintf(m, sizeof(m), "EJ:NONE");
  else snprintf(m, sizeof(m), "EJ:%s:%c", ejId, ejWasImp ? 'I' : 'C');
  broadcastMessage(m);
  // show the result for RESULT_MS on every badge (dur must be non-zero so the
  // phase timer isn't reset to "already expired")
  setPhaseHost(G_RESULT, RESULT_MS / 1000);
}

static bool hostCheckWin() {  // true if the game ended
  int impN = aliveRoleCount(ROLE_IMP);
  int crewN = aliveRoleCount(ROLE_CREW);
  if (impN == 0) winSide = 'C';
  else if (impN >= crewN) winSide = 'I';
  else { winSide = 0; return false; }
  char m[8]; snprintf(m, sizeof(m), "WIN:%c", winSide);
  broadcastMessage(m);
  setPhaseHost(G_OVER, 0);
  return true;
}

static void hostCheckWinOrResume() {
  if (!hostCheckWin()) setPhaseHost(G_PLAYING, 0);
}

// impostor kill (validated on the host)
static void hostKill(const char *killer, const char *target) {
  if (phase != G_PLAYING) return;
  int ki = rosterIndexOfId(killer), ti = rosterIndexOfId(target);
  if (ki < 0 || ti < 0) return;
  if (roleIdx(ki) != ROLE_IMP || !aliveIdx(ki)) return;
  if (!aliveIdx(ti) || roleIdx(ti) == ROLE_IMP) return;
  if (millis() - lastKillMs[ki] < KILL_CD_MS) return;
  lastKillMs[ki] = millis();
  setAliveId(target, false);
  broadcastAlive();
  char m[16]; snprintf(m, sizeof(m), "DEAD:%s", target);
  broadcastMessage(m);
  hostCheckWin();  // a kill can win it for the impostors
}

// body report -> meeting (validated on the host)
static void hostReport(const char *reporter) {
  if (phase != G_PLAYING) return;
  int ri = rosterIndexOfId(reporter);
  if (ri < 0 || !aliveIdx(ri)) return;
  setPhaseHost(G_GATHER, 0);
}

// ---------- message handling (all badges) ----------
void gameHandleMessage(const char *msg) {
  if (strncmp(msg, "ROLE:", 5) == 0) {
    char id[ID_LEN]; char c; char csv[128] = { 0 };
    int got = sscanf(msg, "ROLE:%4[^:]:%c:%127s", id, &c, csv);
    if (got >= 2) {
      int role = (c == 'I') ? ROLE_IMP : ROLE_CREW;
      setRoleId(id, role);
      if (strcmp(id, myId()) == 0) {
        myRole = role;
        if (role == ROLE_IMP && got == 3) storeImpTeam(csv);
      }
    }
  } else if (strncmp(msg, "CFG:", 4) == 0) {
    sscanf(msg, "CFG:%d:%d:%d:%d", &cfgImp, &cfgDisc, &cfgVote, &cfgMeet);
    needRedraw = true;
  } else if (strncmp(msg, "PH:", 3) == 0) {
    int p, dur; if (sscanf(msg, "PH:%d:%d", &p, &dur) == 2) applyPhase((GPhase)p, dur);
  } else if (strncmp(msg, "AL:", 3) == 0) {
    setAliveList(msg + 3);
  } else if (strncmp(msg, "EJ:", 3) == 0) {
    if (strcmp(msg, "EJ:NONE") == 0) { ejSkipped = true; ejId[0] = 0; }
    else {
      char id[ID_LEN]; char c;
      if (sscanf(msg, "EJ:%4[^:]:%c", id, &c) == 2) {
        ejSkipped = false; strncpy(ejId, id, ID_LEN); ejId[ID_LEN - 1] = 0;
        ejWasImp = (c == 'I'); setAliveId(ejId, false);
      }
    }
    needRedraw = true;
  } else if (strncmp(msg, "TAL:", 4) == 0) {
    parseTally(msg + 4); needRedraw = true;
  } else if (strncmp(msg, "WIN:", 4) == 0) {
    winSide = msg[4]; needRedraw = true;
  } else if (strncmp(msg, "DEAD:", 5) == 0) {
    const char *id = msg + 5;
    setAliveId(id, false);
    if (strcmp(id, myId()) == 0) killedFlashUntil = millis() + 1500;
    needRedraw = true;
  }
  // ---- host acts on requests ----
  else if (isHost && strncmp(msg, "KILL:", 5) == 0) {
    char k[ID_LEN], t[ID_LEN];
    if (sscanf(msg, "KILL:%4[^:]:%4s", k, t) == 2) hostKill(k, t);
  } else if (isHost && strncmp(msg, "RPT:", 4) == 0) {
    hostReport(msg + 4);
  } else if (isHost && strncmp(msg, "RM:", 3) == 0) {
    if (phase == G_PLAYING) {
      const char *id = msg + 3;
      int i = rosterIndexOfId(id);
      if (i >= 0 && aliveIdx(i) && meetingsIdx(i) < cfgMeet) {
        incMeetingsId(id);
        setPhaseHost(G_GATHER, 0);
      }
    }
  } else if (isHost && strcmp(msg, "RD") == 0) {
    if (phase == G_GATHER) setPhaseHost(G_DISCUSS, cfgDisc);
  } else if (isHost && strcmp(msg, "RE") == 0) {
    if (phase == G_DISCUSS) { nVotes = 0; setPhaseHost(G_VOTING, cfgVote); }
  } else if (isHost && strcmp(msg, "RC") == 0) {
    if (phase == G_GATHER) setPhaseHost(G_PLAYING, 0);
  } else if (isHost && strncmp(msg, "V:", 2) == 0) {
    if (phase == G_VOTING) {
      char voter[ID_LEN], target[ID_LEN];
      if (sscanf(msg, "V:%4[^:]:%4s", voter, target) == 2) hostRecordVote(voter, target);
    }
  }
}

// ---------- per-phase input + rendering ----------
static void req(const char *m) { broadcastMessage(m); }

static void lobbyInput() {
  if (isButtonPressed(BTN_UP))   { setSel = (setSel + SET_ROWS - 1) % SET_ROWS; needRedraw = true; }
  if (isButtonPressed(BTN_DOWN)) { setSel = (setSel + 1) % SET_ROWS; needRedraw = true; }
  int d = isButtonPressed(BTN_RIGHT) ? 1 : (isButtonPressed(BTN_LEFT) ? -1 : 0);
  if (d) {
    if (setSel == 0) cfgImp  = constrain(cfgImp + d, 1, 3);
    if (setSel == 1) cfgDisc = constrain(cfgDisc + d * 5, 10, 90);
    if (setSel == 2) cfgVote = constrain(cfgVote + d * 5, 10, 60);
    if (setSel == 3) cfgMeet = constrain(cfgMeet + d, 0, 5);
    broadcastCfg();
    needRedraw = true;
  }
  if (isButtonPressed(BTN_START)) hostStartGame();
}

static void callMeeting() {
  if (isHost) {
    int i = rosterIndexOfId(myId());
    if (i >= 0 && aliveIdx(i) && meetingsIdx(i) < cfgMeet) { incMeetingsId(myId()); setPhaseHost(G_GATHER, 0); }
  } else {
    char m[12]; snprintf(m, sizeof(m), "RM:%s", myId()); req(m);
  }
}

static bool isTeammate(const char *id) {
  for (int i = 0; i < nImpTeam; i++) if (strcmp(impTeam[i], id) == 0) return true;
  return false;
}

// nearest living non-teammate (i.e. a crewmate) within kill range
static const char *nearestKillTarget() {
  const char *best = nullptr; int bestR = KILL_RSSI - 1;
  for (int i = 0; i < rosterCount(); i++) {
    const char *id = rosterId(i);
    if (strcmp(id, myId()) == 0 || !aliveIdx(i) || isTeammate(id)) continue;
    int r = proximityRssi(id);
    if (r >= KILL_RSSI && r > bestR) { bestR = r; best = id; }
  }
  return best;
}

// nearest dead player (body) within report range
static const char *nearestBody() {
  const char *best = nullptr; int bestR = REPORT_RSSI - 1;
  for (int i = 0; i < rosterCount(); i++) {
    const char *id = rosterId(i);
    if (strcmp(id, myId()) == 0 || aliveIdx(i)) continue;
    int r = proximityRssi(id);
    if (r >= REPORT_RSSI && r > bestR) { bestR = r; best = id; }
  }
  return best;
}

static void playingInput() {
  // START: hold to reveal your role card.
  bool held = isButtonHeld(BTN_START);
  if (isButtonPressed(BTN_START)) { startDownAt = millis(); startIsReveal = false; }
  if (held && !startIsReveal && millis() - startDownAt > 350) { startIsReveal = true; needRedraw = true; }
  if (startPrevHeld && !held) { startIsReveal = false; needRedraw = true; }
  startPrevHeld = held;
  revealActive = startIsReveal && held;

  // B: hold to kill (impostor, nearest crew in range) or report a nearby body.
  int mi = rosterIndexOfId(myId());
  bool alive = (mi < 0) || aliveIdx(mi);
  bool bheld = isButtonHeld(BTN_B);
  if (isButtonPressed(BTN_B)) { bDownAt = millis(); bActed = false; }
  if (alive && bheld && !bActed && millis() - bDownAt > KILL_HOLD_MS) {
    bActed = true;
    const char *tgt = (myRole == ROLE_IMP) ? nearestKillTarget() : nullptr;
    if (tgt) {
      if (isHost) hostKill(myId(), tgt);
      else { char m[24]; snprintf(m, sizeof(m), "KILL:%s:%s", myId(), tgt); req(m); }
    } else {
      const char *body = nearestBody();
      if (body) {
        if (isHost) hostReport(myId());
        else { char m[16]; snprintf(m, sizeof(m), "RPT:%s", myId()); req(m); }
      }
    }
  }
  if (!bheld) bActed = false;
}

static void gatherInput() {
  if (isButtonPressed(BTN_A)) { if (isHost) setPhaseHost(G_DISCUSS, cfgDisc); else req("RD"); }
  if (isButtonPressed(BTN_B)) { if (isHost) setPhaseHost(G_PLAYING, 0); else req("RC"); }
}

static void discussInput() {
  if (isButtonPressed(BTN_B)) { if (isHost) { nVotes = 0; setPhaseHost(G_VOTING, cfgVote); } else req("RE"); }
}

static void votingInput() {
  int choices = rosterCount() + 1;  // SKIP + players
  if (!myVoted) {
    if (isButtonPressed(BTN_LEFT))  { voteSel = (voteSel + choices - 1) % choices; needRedraw = true; }
    if (isButtonPressed(BTN_RIGHT)) { voteSel = (voteSel + 1) % choices; needRedraw = true; }
    if (isButtonPressed(BTN_A)) {
      const char *target = (voteSel == 0) ? "SKIP" : rosterId(voteSel - 1);
      char m[24]; snprintf(m, sizeof(m), "V:%s:%s", myId(), target);
      if (isHost) hostRecordVote(myId(), target); else req(m);
      myVoted = true; needRedraw = true;
    }
  }
}

static void renderVote() {
  const char *name; int r, g, b; bool isSkip;
  if (voteSel == 0) { name = "SKIP"; isSkip = true; r = g = b = 0; }
  else { const PlayerColor &c = colorByIndex(rosterColorIndex(voteSel - 1)); name = c.name; isSkip = false; r = c.r; g = c.g; b = c.b; }
  showVote(name, r, g, b, isSkip, remainingSecs(), myVoted);
}

GPhase gamePhase() { return phase; }

void setupGame() {
  phase = G_LOBBY;
  isHost = false;
  myRole = ROLE_NONE;
}

void gameUpdate() {
  // HOME: quick tap = emergency meeting; hold = dev debug screen.
  bool homeHeld = isButtonHeld(BTN_HOME);
  if (isButtonPressed(BTN_HOME)) { homeDownAt = millis(); homeIsHold = false; }
  if (homeHeld && !homeIsHold && millis() - homeDownAt > 350) homeIsHold = true;
  bool homeTapped = false;
  if (homePrevHeld && !homeHeld) { if (!homeIsHold) homeTapped = true; }
  homePrevHeld = homeHeld;

  if (homeHeld && homeIsHold) {           // hold HOME -> debug readouts
    float x = 0, y = 0; getRollPitch(x, y);
    updateDisplay(x, y, false);
    needRedraw = true;
    return;
  }
  if (homeTapped && phase == G_PLAYING) callMeeting();  // tap HOME -> meeting

  // ---- input ----
  switch (phase) {
    case G_LOBBY:   lobbyInput();   break;
    case G_PLAYING: playingInput(); break;
    case G_GATHER:  gatherInput();  break;
    case G_DISCUSS: discussInput(); break;
    case G_VOTING:  votingInput();  break;
    case G_OVER:    if (isButtonPressed(BTN_START) && isHost) {
                      resetPlayerStates(); winSide = 0; myRole = ROLE_NONE;
                      setPhaseHost(G_LOBBY, 0);
                    } break;
    default: break;
  }

  // ---- host timers ----
  if (isHost) {
    if (phase == G_DISCUSS && remainingSecs() == 0) { nVotes = 0; setPhaseHost(G_VOTING, cfgVote); }
    else if (phase == G_VOTING && (remainingSecs() == 0 || nVotes >= aliveCount())) hostTallyAndEject();
    else if (phase == G_RESULT && remainingSecs() == 0) hostCheckWinOrResume();
  }

  // ---- LEDs: death flash > meeting red > your profile color ----
  if (millis() < killedFlashUntil) flashLEDs(120, 0, 0, 200);
  else if (phase == G_GATHER) { bool on = (millis() / 400) % 2 == 0; flashLEDs(on ? 90 : 0, 0, 0, 500); }
  else if (phase == G_DISCUSS || phase == G_VOTING) flashLEDs(90, 0, 0, 250);
  else { const PlayerColor &c = colorByIndex(myColorIndex()); flashLEDs(c.r, c.g, c.b, 250); }

  // ---- rendering ----
  int cd = remainingSecs();
  bool tick = (cd != lastCd);
  switch (phase) {
    case G_LOBBY:
      if (needRedraw || rosterCount() != lastLobbyPlayers) {
        lastLobbyPlayers = rosterCount();
        const PlayerColor &c = colorByIndex(myColorIndex());
        showLobby(rosterCount(), cfgImp, cfgDisc, cfgVote, cfgMeet, setSel, c.r, c.g, c.b);
      }
      break;
    case G_PLAYING:
      if (needRedraw) {
        const PlayerColor &c = colorByIndex(myColorIndex());
        if (revealActive) {
          uint8_t tr[MAX_PLAYERS], tg[MAX_PLAYERS], tb[MAX_PLAYERS];
          int nt = 0;
          if (myRole == ROLE_IMP) {
            for (int i = 0; i < nImpTeam; i++) {
              int ri = rosterIndexOfId(impTeam[i]);
              if (ri >= 0) { const PlayerColor &tc = colorByIndex(rosterColorIndex(ri)); tr[nt] = tc.r; tg[nt] = tc.g; tb[nt] = tc.b; nt++; }
            }
          }
          showRoleCard(c.r, c.g, c.b, myRole == ROLE_IMP, nt, tr, tg, tb);  // held START
        } else {
          int mi = rosterIndexOfId(myId());
          bool alive = (mi < 0) || aliveIdx(mi);
          showHUD(alive, aliveCount(), c.r, c.g, c.b);      // color only
        }
      }
      break;
    case G_GATHER:
      if (needRedraw) { showMeetingScreen(); showMeetingWaiting(); }
      break;
    case G_DISCUSS:
      if (needRedraw) showMeetingScreen();
      if (needRedraw || tick) showMeetingCountdown(cd);
      break;
    case G_VOTING:
      if (needRedraw || tick) renderVote();
      break;
    case G_RESULT:
      if (needRedraw) {
        uint8_t tR[MAX_PLAYERS], tG[MAX_PLAYERS], tB[MAX_PLAYERS];
        for (int i = 0; i < nTal; i++) { const PlayerColor &tc = colorByIndex(talColor[i]); tR[i] = tc.r; tG[i] = tc.g; tB[i] = tc.b; }
        if (ejSkipped) showResult("", 0, 0, 0, true, false, nTal, tR, tG, tB, talCount, talSkip);
        else {
          int i = rosterIndexOfId(ejId);
          const PlayerColor &c = colorByIndex(i >= 0 ? rosterColorIndex(i) : 0);
          showResult(c.name, c.r, c.g, c.b, false, ejWasImp, nTal, tR, tG, tB, talCount, talSkip);
        }
      }
      break;
    case G_OVER:
      if (needRedraw) showGameOver(winSide == 'C');
      break;
  }
  lastCd = cd;
  needRedraw = false;
}
