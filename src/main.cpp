#include <Arduino.h>
#include <string.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "wifi_sta.h"
#include "broadcast.h"
#include "buttons.h"
#include "nfc.h"
#include "players.h"
#include "game.h"
#include "espnow_prox.h"
#include "tasks.h"
#include "tasktest.h"

unsigned long lastPresence = 0;
unsigned long lastProxDebug = 0;
bool nfcEnabled = false;
bool testMode = false;   // solo task-test harness (hold START at boot)

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupButtons();

  // Hold A while booting -> solo task test mode; skip all networking.
  // (Not START: START is GPIO9, the boot strapping pin -- holding it at reset
  //  drops the chip into download mode instead of running firmware.)
  for (int i = 0; i < 6; i++) { updateButtons(); delay(12); }
  testMode = isButtonHeld(BTN_A);

  if (!testMode) {
    setupWiFi();
    setupBroadcast();
    setupPlayers();
    setupGame();
    setupProximity(myId());  // ESP-NOW ranging for kills / body reports
  } else {
    Serial.println("== TASK TEST MODE (hold A at boot to enter) ==");
  }

  setupTasks();
  powerDownNFC();  // NFC starts off to save power

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
  }
}

void loop() {
  updateLEDs();
  updateButtons();

  // solo test harness owns everything when active
  if (testMode) { taskTestLoop(); return; }

  // announce ourselves ~1/sec so every badge builds the same roster
  if (millis() - lastPresence > 1000) {
    lastPresence = millis();
    sendPresence();
  }

  // receive: presence goes to the roster, everything else to the game
  char msg[64];
  if (pollMessage(msg, sizeof(msg)) > 0) {
    if (strncmp(msg, "ID:", 3) == 0) {
      char id[ID_LEN]; int col = 0;
      if (sscanf(msg, "ID:%4[^:]:%d", id, &col) == 2) notePresence(id, col);
    } else {
      gameHandleMessage(msg);
    }
  }

  // NFC on during play AND lobby (lobby lets a single badge test tasks)
  bool wantNfc = (gamePhase() == G_PLAYING || gamePhase() == G_LOBBY);
  if (wantNfc != nfcEnabled) {
    nfcEnabled = wantNfc;
    if (nfcEnabled) beginNFCScan(); else powerDownNFC();
  }
  if (nfcEnabled) {
    String uid = scanNFC();
    if (uid != "") {
      Serial.print("Scanned tag UID: "); Serial.println(uid);  // for hard-mapping later
      gameOnNfc(uid.c_str());
    }
  }

  updateProximity();  // send the next ESP-NOW proximity beacon when due
  if (millis() - lastProxDebug > 1000) {
    lastProxDebug = millis();
    debugProximity();
  }

  // the state machine owns input handling and all rendering
  gameUpdate();
}
