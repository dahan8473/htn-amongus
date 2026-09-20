#include <Arduino.h>
#include <string.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"
#include "meeting.h"
#include "nfc.h"
#include "players.h"
#include "roles.h"
#include "voting.h"

unsigned long lastDisplayUpdate = 0;
unsigned long lastPresence = 0;
bool nfcEnabled = false;
int lastTitlePlayers = -1;  // so the title only redraws when the count changes

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupWiFi();
  setupBroadcast();
  setupButtons();
  setupPlayers();
  setupRoles();

  powerDownNFC();  // start with NFC off to save power

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
  }
}

void loop() {
  updateLEDs();
  updateButtons();

  // announce ourselves ~1/sec so everyone builds the roster
  if (millis() - lastPresence > 1000) {
    lastPresence = millis();
    sendPresence();
  }

  // ---- controls (context-sensitive) ----
  if (isButtonPressed(BTN_START) && !isVotingActive()) {
    if (gamePhase() == PHASE_LOBBY) startGameAsHost();  // lobby: start game
    else triggerEmergencyMeeting();                     // in game: call meeting
  }
  if (isButtonPressed(BTN_A)) {
    if (isVotingActive()) castVote();
    else if (isMeetingGathering()) confirmEveryoneHere();
    else if (gamePhase() == PHASE_PLAY) requestRoleReveal();
  }
  if (isButtonPressed(BTN_B) && isMeetingActive()) {
    endMeetingEarly();
  }
  if (isVotingActive()) {
    if (isButtonPressed(BTN_LEFT)) voteSelect(-1);
    if (isButtonPressed(BTN_RIGHT)) voteSelect(1);
  }

  // ---- receive broadcasts and dispatch ----
  char msg[48];
  if (pollMessage(msg, sizeof(msg)) > 0) {
    if (strncmp(msg, "ID:", 3) == 0) {
      char id[ID_LEN]; int colorIdx = 0;
      if (sscanf(msg, "ID:%4[^:]:%d", id, &colorIdx) == 2) notePresence(id, colorIdx);
    } else if (strcmp(msg, MEETING_MSG) == 0) {
      startMeeting();
    } else if (strcmp(msg, DISCUSS_MSG) == 0) {
      beginDiscussion();
    } else if (strcmp(msg, ENDMTG_MSG) == 0) {
      endMeeting();
    } else if (strcmp(msg, VOTESTART_MSG) == 0) {
      startVoting();
    } else if (strncmp(msg, "VOTE:", 5) == 0) {
      handleVoteMessage(msg);
    } else {
      handleGameMessage(msg);  // ROLE:/PLAY/ENDGAME
    }
  }

  // NFC toggle via the AUX1 maintained switch
  bool sw = isButtonHeld(BTN_AUX1);
  if (sw != nfcEnabled) {
    nfcEnabled = sw;
    if (nfcEnabled) { beginNFCScan(); Serial.println("NFC ON"); }
    else { powerDownNFC(); Serial.println("NFC OFF"); }
  }
  if (nfcEnabled) {
    String uid = scanNFC();
    if (uid != "") { Serial.print("Task UID: "); Serial.println(uid); }
  }

  // ---- screen ownership: meeting > voting > role reveal > title/tilt ----
  updateRoles();
  if (isMeetingActive()) {
    updateMeeting();
    lastTitlePlayers = -1;
  } else if (isVotingActive()) {
    updateVoting();
    lastTitlePlayers = -1;
  } else if (isRevealing()) {
    // role card owns the screen
    lastTitlePlayers = -1;
  } else if (gamePhase() == PHASE_LOBBY) {
    // title screen; redraw only when the player count changes
    if (rosterCount() != lastTitlePlayers) {
      lastTitlePlayers = rosterCount();
      const PlayerColor &c = colorByIndex(myColorIndex());
      showTitleScreen(rosterCount(), c.r, c.g, c.b);
    }
  } else if (millis() - lastDisplayUpdate > 100) {
    // in-game idle: tilt + NFC view
    lastTitlePlayers = -1;
    lastDisplayUpdate = millis();
    float x_angle = 0, y_angle = 0;
    getRollPitch(x_angle, y_angle);
    updateDisplay(x_angle, y_angle, nfcEnabled);
  }
}
