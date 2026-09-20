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

unsigned long lastPresence = 0;
unsigned long lastProxDebug = 0;
bool immortal = false;

void setup() {
  Serial.begin(115200);

  setupLEDs();
  setupDisplay();
  setupEspNowRadio();
  setupBroadcast();
  setupButtons();
  setupPlayers();
  setupGame();
  setupProximity(myId());  // ESP-NOW ranging for kills / body reports

  powerDownNFC();  // NFC reader stays off; AUX1 is repurposed for demo immortality

  if (!setupIMU()) {
    Serial.println("SC7A20 IMU not found at 0x19!");
  }
}

void loop() {
  updateLEDs();
  updateButtons();
  updateBroadcast();  // fires any due mesh relays

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

  // AUX1 maintained switch: demo-mode immortality toggle. While ON, this
  // badge can't be killed or even targeted, regardless of proximity to an
  // impostor -- synced out so the host (which validates every kill) knows.
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
