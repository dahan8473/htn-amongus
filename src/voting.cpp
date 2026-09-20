#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include "voting.h"
#include "players.h"
#include "broadcast.h"
#include "display.h"

#define VOTE_MS   20000
#define RESULT_MS 4000

enum { V_OFF, V_VOTE, V_RESULT };
static int vstate = V_OFF;
static unsigned long vEnd = 0, rEnd = 0;
static int sel = 0;              // 0 = SKIP, 1..n = roster index (sel-1)
static bool myVoted = false;
static int lastSecs = -1;
static bool needRedraw = false;

// who has voted (dedupe) and for whom
static char voters[MAX_PLAYERS][ID_LEN];
static char targets[MAX_PLAYERS][ID_LEN];  // "SKIP" or a player id
static int nVotes = 0;

// result
static bool ejectSkipped = true;
static int ejectRoster = -1;

static void selName(int s, const char **name, int *r, int *g, int *b, bool *isSkip) {
  if (s == 0) { *name = "SKIP"; *isSkip = true; *r = *g = *b = 0; return; }
  int idx = s - 1;
  const PlayerColor &c = colorByIndex(rosterColorIndex(idx));
  *name = c.name; *isSkip = false; *r = c.r; *g = c.g; *b = c.b;
}

void startVoting() {
  if (vstate != V_OFF) return;
  vstate = V_VOTE;
  vEnd = millis() + VOTE_MS;
  sel = 0;
  myVoted = false;
  lastSecs = -1;
  nVotes = 0;
  needRedraw = true;
}

bool isVotingActive() { return vstate != V_OFF; }

void voteSelect(int dir) {
  if (vstate != V_VOTE || myVoted) return;
  int choices = rosterCount() + 1;      // SKIP + players
  sel = (sel + dir + choices) % choices;
  needRedraw = true;
}

static void recordVote(const char *voter, const char *target) {
  for (int i = 0; i < nVotes; i++)
    if (strncmp(voters[i], voter, ID_LEN) == 0) return;  // already counted
  if (nVotes < MAX_PLAYERS) {
    strncpy(voters[nVotes], voter, ID_LEN); voters[nVotes][ID_LEN - 1] = 0;
    strncpy(targets[nVotes], target, ID_LEN); targets[nVotes][ID_LEN - 1] = 0;
    nVotes++;
  }
}

void castVote() {
  if (vstate != V_VOTE || myVoted) return;
  const char *name; int r, g, b; bool isSkip;
  selName(sel, &name, &r, &g, &b, &isSkip);
  const char *target = isSkip ? "SKIP" : rosterId(sel - 1);
  char msg[24];
  snprintf(msg, sizeof(msg), "VOTE:%s:%s", myId(), target);
  broadcastMessage(msg);
  recordVote(myId(), target);
  myVoted = true;
  needRedraw = true;
}

void handleVoteMessage(const char *msg) {
  if (strncmp(msg, "VOTE:", 5) != 0) return;
  char voter[ID_LEN], target[ID_LEN];
  if (sscanf(msg, "VOTE:%4[^:]:%4s", voter, target) == 2) recordVote(voter, target);
}

static void tally() {
  // count votes per player id (SKIP ignored for the winner)
  int counts[MAX_PLAYERS] = { 0 };
  for (int i = 0; i < nVotes; i++) {
    if (strncmp(targets[i], "SKIP", 4) == 0) continue;
    int idx = rosterIndexOfId(targets[i]);
    if (idx >= 0) counts[idx]++;
  }
  int best = -1, bestN = 0; bool tie = false;
  for (int i = 0; i < rosterCount(); i++) {
    if (counts[i] > bestN) { best = i; bestN = counts[i]; tie = false; }
    else if (counts[i] == bestN && bestN > 0) tie = true;
  }
  if (best >= 0 && bestN > 0 && !tie) { ejectSkipped = false; ejectRoster = best; }
  else { ejectSkipped = true; ejectRoster = -1; }
}

static void drawVote() {
  const char *name; int r, g, b; bool isSkip;
  selName(sel, &name, &r, &g, &b, &isSkip);
  int secs = (int)(((long)vEnd - (long)millis() + 999) / 1000);
  if (secs < 0) secs = 0;
  showVote(name, r, g, b, isSkip, secs, myVoted);
}

void updateVoting() {
  if (vstate == V_OFF) return;

  if (vstate == V_VOTE) {
    int secs = (int)(((long)vEnd - (long)millis() + 999) / 1000);
    if (needRedraw || secs != lastSecs) {
      needRedraw = false;
      lastSecs = secs;
      drawVote();
    }
    bool everyoneVoted = (nVotes >= rosterCount() && rosterCount() > 0);
    if (millis() > vEnd || everyoneVoted) {
      tally();
      vstate = V_RESULT;
      rEnd = millis() + RESULT_MS;
      if (ejectSkipped) {
        showEjectResult("", 0, 0, 0, true);
      } else {
        const PlayerColor &c = colorByIndex(rosterColorIndex(ejectRoster));
        showEjectResult(c.name, c.r, c.g, c.b, false);
      }
    }
    return;
  }

  // V_RESULT
  if (millis() > rEnd) {
    vstate = V_OFF;
    clearScreen();  // back to the game
  }
}
