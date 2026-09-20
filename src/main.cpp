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

unsigned long lastPresence = 0;
bool nfcEnabled = false;

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupWiFi();
  setupBroadcast();
  setupButtons();
  setupPlayers();
  setupGame();
  setupProximity(myId());  // ESP-NOW ranging for kills / body reports

  powerDownNFC();  // NFC starts off to save power

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
  }
}

void loop() {
  updateLEDs();
  updateButtons();

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

  // AUX1 maintained switch toggles the NFC reader for task stickers
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

  updateProximity();  // send the next ESP-NOW proximity beacon when due

  // the state machine owns input handling and all rendering
  gameUpdate();
}
