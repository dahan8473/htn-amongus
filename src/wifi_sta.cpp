#include <Arduino.h>
#include <WiFi.h>
#include "wifi_sta.h"
#include "wifi_credentials.h"

static void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      WiFi.setSleep(false); // stay awake between beacons so game-message latency stays low
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi disconnected, retrying");
      WiFi.reconnect();
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("WiFi connected, IP=");
      Serial.println(WiFi.localIP());
      break;
    default:
      break;
  }
}

void setupWiFi() {
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}
