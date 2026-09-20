#include <Arduino.h>
#include <string.h>
#include "leds.h"
#include "display.h"
#include "imu.h"
#include "espnow_radio.h"
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
bool testMode = false;   // solo task-test harness (hold A at boot)
bool immortal = false;

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupEspNowRadio();
  setupBroadcast();
  setupButtons();

  // Hold A while booting -> solo task test mode; skip game/roster/proximity.
  // (Not START: START is GPIO9, the boot strapping pin -- holding it at reset
  //  drops the chip into download mode instead of running firmware.)
  for (int i = 0; i < 6; i++) { updateButtons(); delay(12); }
  testMode = isButtonHeld(BTN_A);

  if (!testMode) {
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
  updateBroadcast();  // fires any due mesh relays

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
      Serial.print("Scanned tag UID: "); Serial.println(uid);
      gameOnNfc(uid.c_str());
    }
  }

  // AUX1 maintained switch: demo-mode immortality toggle. While ON, this
  // badge can't be killed or even targeted, regardless of proximity to an
  // imposter -- synced out so the host (which validates every kill) knows.
  bool sw = isButtonHeld(BTN_AUX1);
  if (sw != immortal) {
    immortal = sw;
    setImmortalId(myId(), immortal);
    char m[16]; snprintf(m, sizeof(m), "IMM:%s:%d", myId(), immortal ? 1 : 0);
    broadcastMessage(m);
    Serial.println(immortal ? "IMMORTAL" : "MORTAL");
  }

  updateProximity();  // send the next ESP-NOW proximity beacon when due
  if (millis() - lastProxDebug > 1000) {
    lastProxDebug = millis();
    debugProximity();
  }

  // the state machine owns input handling and all rendering
  gameUpdate();
}
