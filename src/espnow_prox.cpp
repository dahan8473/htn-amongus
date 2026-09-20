#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "espnow_prox.h"

#define PROX_MAX                16
#define PROX_STALE_MS           3000
#define PROX_BEACON_INTERVAL_MS 200

// First byte of every ESP-NOW frame tells proximity beacons and game messages
// apart, so both can share the one broadcast + one receive callback.
#define NET_T_PROX 0x01
#define NET_T_MSG  0x02

// Received game messages wait here until the main loop polls them.
#define MSGQ_N   10
#define MSGQ_LEN 64
static char msgq[MSGQ_N][MSGQ_LEN];
static int msgHead = 0, msgTail = 0;
static portMUX_TYPE msgMux = portMUX_INITIALIZER_UNLOCKED;

static const uint8_t BCAST_MAC[ESP_NOW_ETH_ALEN] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

struct ProxBeacon {
  char id[5];
};

struct Rec {
  char id[5];
  int rssi;
  unsigned long ms;
  bool initialized;
};

static Rec recs[PROX_MAX];
static int nRecs = 0;
static char ownId[5] = "";
static unsigned long lastBeaconMs = 0;
static bool espNowReady = false;
static portMUX_TYPE recMux = portMUX_INITIALIZER_UNLOCKED;

static void record(const char *id, int rssi) {
  if (strncmp(id, ownId, sizeof(ownId)) == 0) return;

  portENTER_CRITICAL(&recMux);
  int slot = -1;
  for (int i = 0; i < nRecs; i++) {
    if (strncmp(recs[i].id, id, sizeof(recs[i].id)) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0 && nRecs < PROX_MAX) {
    slot = nRecs++;
    strncpy(recs[slot].id, id, sizeof(recs[slot].id));
    recs[slot].id[sizeof(recs[slot].id) - 1] = '\0';
  }
  if (slot >= 0) {
    // Integer exponential moving average: 75% history, 25% new sample.
    recs[slot].rssi = recs[slot].initialized
      ? (recs[slot].rssi * 3 + rssi) / 4
      : rssi;
    recs[slot].initialized = true;
    recs[slot].ms = millis();
  }
  portEXIT_CRITICAL(&recMux);
}

static void onReceive(const esp_now_recv_info_t *info,
                      const uint8_t *data, int len) {
  if (!info || len < 1) return;

  if (data[0] == NET_T_PROX) {
    if (!info->rx_ctrl || len != 1 + (int)sizeof(ProxBeacon)) return;
    ProxBeacon beacon;
    memcpy(&beacon, data + 1, sizeof(beacon));
    beacon.id[sizeof(beacon.id) - 1] = '\0';
    if (strlen(beacon.id) != 4) return;
    record(beacon.id, info->rx_ctrl->rssi);
    return;
  }

  if (data[0] == NET_T_MSG) {
    int n = len - 1;
    if (n <= 0) return;
    if (n > MSGQ_LEN - 1) n = MSGQ_LEN - 1;
    portENTER_CRITICAL(&msgMux);
    int next = (msgHead + 1) % MSGQ_N;
    if (next != msgTail) {             // drop if the queue is full
      memcpy(msgq[msgHead], data + 1, n);
      msgq[msgHead][n] = '\0';
      msgHead = next;
    }
    portEXIT_CRITICAL(&msgMux);
  }
}

void setupProximity(const char *myId) {
  strncpy(ownId, myId, sizeof(ownId));
  ownId[sizeof(ownId) - 1] = '\0';

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW proximity init failed");
    return;
  }
  if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
    Serial.println("ESP-NOW receive callback failed");
    esp_now_deinit();
    return;
  }

  esp_now_peer_info_t peer = {};
  memset(peer.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);
  peer.channel = 0;  // 0 = use the radio's current (fixed) channel, set in setupWiFi
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_err_t addResult = esp_now_add_peer(&peer);
  if (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("ESP-NOW broadcast peer failed: %d\n", addResult);
    esp_now_deinit();
    return;
  }

  espNowReady = true;
  Serial.println("ESP-NOW proximity ready");
}

void updateProximity() {
  if (!espNowReady) return;
  unsigned long now = millis();
  if (now - lastBeaconMs < PROX_BEACON_INTERVAL_MS) return;
  lastBeaconMs = now;

  uint8_t buf[1 + sizeof(ProxBeacon)] = {0};
  buf[0] = NET_T_PROX;
  ProxBeacon beacon = {};
  strncpy(beacon.id, ownId, sizeof(beacon.id));
  memcpy(buf + 1, &beacon, sizeof(beacon));
  esp_now_send(BCAST_MAC, buf, sizeof(buf));
}

// ---- game message bus (rides the same ESP-NOW broadcast) ----

void espnowSendMsg(const char *msg) {
  if (!espNowReady) return;
  int n = strlen(msg);
  if (n > MSGQ_LEN - 1) n = MSGQ_LEN - 1;
  uint8_t buf[1 + MSGQ_LEN];
  buf[0] = NET_T_MSG;
  memcpy(buf + 1, msg, n);
  esp_now_send(BCAST_MAC, buf, 1 + n);
}

int espnowPollMsg(char *out, int maxLen) {
  portENTER_CRITICAL(&msgMux);
  if (msgTail == msgHead) { portEXIT_CRITICAL(&msgMux); return 0; }
  strncpy(out, msgq[msgTail], maxLen - 1);
  out[maxLen - 1] = '\0';
  msgTail = (msgTail + 1) % MSGQ_N;
  portEXIT_CRITICAL(&msgMux);
  return strlen(out);
}

int proximityRssi(const char *id) {
  int result = -127;
  unsigned long now = millis();
  portENTER_CRITICAL(&recMux);
  for (int i = 0; i < nRecs; i++) {
    if (strncmp(recs[i].id, id, sizeof(recs[i].id)) == 0) {
      if (now - recs[i].ms < PROX_STALE_MS) result = recs[i].rssi;
      break;
    }
  }
  portEXIT_CRITICAL(&recMux);
  return result;
}

void debugProximity() {
  Rec snapshot[PROX_MAX];
  int count;
  portENTER_CRITICAL(&recMux);
  count = nRecs;
  memcpy(snapshot, recs, sizeof(Rec) * count);
  portEXIT_CRITICAL(&recMux);

  Serial.print("PROX");
  unsigned long now = millis();
  for (int i = 0; i < count; i++) {
    if (!snapshot[i].initialized) continue;
    Serial.print(" ");
    Serial.print(snapshot[i].id);
    Serial.print("=");
    if (now - snapshot[i].ms < PROX_STALE_MS) Serial.print(snapshot[i].rssi);
    else Serial.print("stale");
  }
  Serial.println();
}
