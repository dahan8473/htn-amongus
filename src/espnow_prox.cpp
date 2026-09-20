#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <math.h>
#include <string.h>
#include "espnow_prox.h"

#define PROX_MAX                16
#define PROX_STALE_MS           3000
#define PROX_BEACON_INTERVAL_MS 200

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
  if (!info || !info->rx_ctrl || len != (int)sizeof(ProxBeacon)) return;

  ProxBeacon beacon;
  memcpy(&beacon, data, sizeof(beacon));
  beacon.id[sizeof(beacon.id) - 1] = '\0';
  if (strlen(beacon.id) != 4) return;

  record(beacon.id, info->rx_ctrl->rssi);
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
  peer.channel = 0;  // follow the channel selected by the connected Wi-Fi AP
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
  // ESP-NOW is direct radio traffic; it can keep working while the badge is
  // still trying to join the router used by the UDP game bus.
  if (!espNowReady) return;
  unsigned long now = millis();
  if (now - lastBeaconMs < PROX_BEACON_INTERVAL_MS) return;
  lastBeaconMs = now;

  ProxBeacon beacon = {};
  strncpy(beacon.id, ownId, sizeof(beacon.id));
  static const uint8_t broadcastMac[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
  };
  esp_now_send(broadcastMac,
               reinterpret_cast<const uint8_t *>(&beacon), sizeof(beacon));
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
  if (count == 0) {
    Serial.println(" none");
    return;
  }
  unsigned long now = millis();
  for (int i = 0; i < count; i++) {
    if (!snapshot[i].initialized) continue;
    Serial.print(" ");
    Serial.print(snapshot[i].id);
    Serial.print("=");
    if (now - snapshot[i].ms < PROX_STALE_MS) {
      Serial.print(snapshot[i].rssi);
      // Very rough free-space/path-loss estimate. RSSI is not a tape measure;
      // these constants are only useful as a starting point for calibration.
      const float rssiAtOneMeter = -55.0f;
      const float pathLossExponent = 2.5f;
      float metres = powf(10.0f,
        (rssiAtOneMeter - snapshot[i].rssi) / (10.0f * pathLossExponent));
      Serial.print("dBm(~");
      Serial.print(metres, 1);
      Serial.print("m)");
    } else Serial.print("stale");
  }
  Serial.println();
}
