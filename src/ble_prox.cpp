#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>
#include "ble_prox.h"

#define PROX_MAX      16
#define PROX_STALE_MS 3000

struct Rec { char id[5]; int rssi; unsigned long ms; };
static Rec recs[PROX_MAX];
static int nRecs = 0;

static void record(const char *id, int rssi) {
  for (int i = 0; i < nRecs; i++) {
    if (strncmp(recs[i].id, id, 5) == 0) { recs[i].rssi = rssi; recs[i].ms = millis(); return; }
  }
  if (nRecs < PROX_MAX) {
    strncpy(recs[nRecs].id, id, 5); recs[nRecs].id[4] = 0;
    recs[nRecs].rssi = rssi; recs[nRecs].ms = millis(); nRecs++;
  }
}

class ProxCB : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *d) override {
    std::string n = d->getName();
    if (n.size() == 4) record(n.c_str(), d->getRSSI());
  }
};

void setupProximity(const char *myId) {
  NimBLEDevice::init(myId);

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData adData;
  adData.setName(myId);
  adv->setAdvertisementData(adData);
  adv->setScanResponse(false);
  adv->start();

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ProxCB(), true);  // duplicates -> RSSI updates
  scan->setActiveScan(false);                              // passive, low power
  scan->setInterval(160);
  scan->setWindow(160);
  scan->start(0, nullptr, false);                          // scan forever
}

void updateProximity() {
  NimBLEScan *scan = NimBLEDevice::getScan();
  if (!scan->isScanning()) scan->start(0, nullptr, false);
}

int proximityRssi(const char *id) {
  for (int i = 0; i < nRecs; i++) {
    if (strncmp(recs[i].id, id, 5) == 0)
      return (millis() - recs[i].ms < PROX_STALE_MS) ? recs[i].rssi : -127;
  }
  return -127;
}
