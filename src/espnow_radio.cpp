#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "espnow_radio.h"

static const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#define MAX_HANDLERS 4
struct HandlerEntry {
  uint8_t type;
  EspNowRecvHandler fn;
  bool used;
};
static HandlerEntry handlers[MAX_HANDLERS];

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!info || len < 1) return;
  uint8_t type = data[0];
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -127;
  for (int i = 0; i < MAX_HANDLERS; i++) {
    if (handlers[i].used && handlers[i].type == type) {
      handlers[i].fn(info->src_addr, rssi, data + 1, len - 1);
      return;
    }
  }
}

void setupEspNowRadio() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW radio init failed");
    return;
  }
  if (esp_now_register_recv_cb(onRecv) != ESP_OK) {
    Serial.println("ESP-NOW receive callback registration failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_err_t addResult = esp_now_add_peer(&peer);
  if (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("ESP-NOW broadcast peer failed: %d\n", addResult);
  }
}

void espNowSend(const uint8_t *data, int len) {
  esp_now_send(BROADCAST_MAC, data, len);
}

void espNowOnReceive(uint8_t packetType, EspNowRecvHandler handler) {
  for (int i = 0; i < MAX_HANDLERS; i++) {
    if (!handlers[i].used) {
      handlers[i].used = true;
      handlers[i].type = packetType;
      handlers[i].fn = handler;
      return;
    }
  }
}
