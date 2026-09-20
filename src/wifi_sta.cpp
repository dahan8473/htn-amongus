#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "wifi_sta.h"

// ESP-NOW needs no router. We only put the radio in station mode on a FIXED
// channel (every badge must agree) and never associate to an AP. This removes
// the dependency on the "imposter" network entirely -- no creds, no connect
// retries, no hang when the AP is down.
#define ESPNOW_CHANNEL 1

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();        // make sure we're not associated to anything
  WiFi.setSleep(false);     // keep the radio awake so ESP-NOW latency stays low
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

bool isWiFiConnected() {
  return true;  // the ESP-NOW bus is always "up" once the radio is in STA mode
}
