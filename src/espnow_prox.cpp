#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "espnow_prox.h"
#include "espnow_radio.h"

#define PROX_MAX                16
#define PROX_STALE_MS           3000
#define PROX_BEACON_INTERVAL_MS 200
#define PKT_PROX                0xA1

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

static void onProxRecv(const uint8_t *mac, int rssi, const uint8_t *data, int len) {
  if (len != 4) return;
  char id[5];
  memcpy(id, data, 4);
  id[4] = '\0';
  record(id, rssi);
}

void setupProximity(const char *myId) {
  strncpy(ownId, myId, sizeof(ownId));
  ownId[sizeof(ownId) - 1] = '\0';

  espNowOnReceive(PKT_PROX, onProxRecv);
}

void updateProximity() {
  unsigned long now = millis();
  if (now - lastBeaconMs < PROX_BEACON_INTERVAL_MS) return;
  lastBeaconMs = now;

  uint8_t buf[5];
  buf[0] = PKT_PROX;
  memcpy(buf + 1, ownId, 4);
  espNowSend(buf, 5);
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
