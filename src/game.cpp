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

static unsigned long revealUntil = 0;   // role-card overlay
static bool revealDrawn = false;

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

static bool needRedraw = true;
static int lastCd = -1;
static int lastLobbyPlayers = -1;

// ---------- helpers ----------
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
  for (int i = 0; i < n; i++) {
    int role = imp[i] ? ROLE_IMP : ROLE_CREW;
    setRoleId(rosterId(i), role);
    char m[24];
    snprintf(m, sizeof(m), "ROLE:%s:%c", rosterId(i), imp[i] ? 'I' : 'C');
    broadcastMessage(m);
    if (strcmp(rosterId(i), myId()) == 0) myRole = role;
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
  broadcastAlive();
  char m[24];
  if (ejSkipped) snprintf(m, sizeof(m), "EJ:NONE");
  else snprintf(m, sizeof(m), "EJ:%s:%c", ejId, ejWasImp ? 'I' : 'C');
  broadcastMessage(m);
  applyPhase(G_RESULT, 0);
  phaseEnd = millis() + RESULT_MS;
  setPhaseHost(G_RESULT, 0);  // followers also enter result; ej info from EJ
}

static void hostCheckWinOrResume() {
  int impN = aliveRoleCount(ROLE_IMP);
  int crewN = aliveRoleCount(ROLE_CREW);
  if (impN == 0) winSide = 'C';
  else if (impN >= crewN) winSide = 'I';
  else winSide = 0;
  if (winSide) {
    char m[8]; snprintf(m, sizeof(m), "WIN:%c", winSide);
    broadcastMessage(m);
    setPhaseHost(G_OVER, 0);
  } else {
    setPhaseHost(G_PLAYING, 0);
  }
}

// ---------- message handling (all badges) ----------
void gameHandleMessage(const char *msg) {
  if (strncmp(msg, "ROLE:", 5) == 0) {
    char id[ID_LEN]; char c;
    if (sscanf(msg, "ROLE:%4[^:]:%c", id, &c) == 2) {
      int role = (c == 'I') ? ROLE_IMP : ROLE_CREW;
      setRoleId(id, role);
      if (strcmp(id, myId()) == 0) myRole = role;
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
  } else if (strncmp(msg, "WIN:", 4) == 0) {
    winSide = msg[4]; needRedraw = true;
  }
  // ---- host acts on requests ----
  else if (isHost && strncmp(msg, "RM:", 3) == 0) {
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

static void playingInput() {
  if (isButtonPressed(BTN_START)) {           // request a meeting
    if (isHost) {
      int i = rosterIndexOfId(myId());
      if (i >= 0 && aliveIdx(i) && meetingsIdx(i) < cfgMeet) { incMeetingsId(myId()); setPhaseHost(G_GATHER, 0); }
    } else {
      char m[12]; snprintf(m, sizeof(m), "RM:%s", myId()); req(m);
    }
  }
  if (isButtonPressed(BTN_A)) { revealUntil = millis() + REVEAL_MS; revealDrawn = false; }
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
  // hidden dev screen: hold HOME to see tilt + NFC readouts
  if (isButtonHeld(BTN_HOME)) {
    float x = 0, y = 0; getRollPitch(x, y);
    updateDisplay(x, y, false);
    needRedraw = true;
    return;
  }

  // role-card overlay (press A near the start)
  if (millis() < revealUntil) {
    if (!revealDrawn) {
      revealDrawn = true;
      const PlayerColor &c = colorByIndex(myColorIndex());
      showRoleCard(c.r, c.g, c.b, myRole == ROLE_IMP);
    }
    return;
  } else if (revealDrawn) {
    revealDrawn = false; needRedraw = true;
  }

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
    else if (phase == G_RESULT && millis() > phaseEnd) hostCheckWinOrResume();
  }

  // ---- LEDs during a meeting ----
  if (phase == G_GATHER) { bool on = (millis() / 400) % 2 == 0; flashLEDs(on ? 90 : 0, 0, 0, 500); }
  else if (phase == G_DISCUSS || phase == G_VOTING) flashLEDs(90, 0, 0, 250);

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
        int mi = rosterIndexOfId(myId());
        bool alive = (mi < 0) || aliveIdx(mi);
        showHUD(alive, myRole == ROLE_IMP, aliveCount(), c.r, c.g, c.b);
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
        if (ejSkipped) showEjectResult("", 0, 0, 0, true, false);
        else {
          int i = rosterIndexOfId(ejId);
          const PlayerColor &c = colorByIndex(i >= 0 ? rosterColorIndex(i) : 0);
          showEjectResult(c.name, c.r, c.g, c.b, false, ejWasImp);
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
