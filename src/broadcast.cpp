#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>
#include "broadcast.h"
#include "wifi_sta.h"

#define BROADCAST_PORT 4210

static WiFiUDP udp;

// Directed broadcast address for the current subnet (e.g. 192.168.2.255),
// since some routers drop the limited broadcast 255.255.255.255.
static IPAddress broadcastAddress() {
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress bcast;
  for (int i = 0; i < 4; i++) {
    bcast[i] = (ip[i] & mask[i]) | (~mask[i] & 0xFF);
  }
  return bcast;
}

void setupBroadcast() {
  udp.begin(BROADCAST_PORT);
}

void broadcastMessage(const char *msg) {
  if (!isWiFiConnected()) {
    return;
  }
  udp.beginPacket(broadcastAddress(), BROADCAST_PORT);
  udp.write((const uint8_t *)msg, strlen(msg));
  udp.endPacket();
}

int pollMessage(char *buf, int maxLen) {
  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return 0;
  }
  int len = udp.read(buf, maxLen - 1);
  if (len <= 0) {
    return 0;
  }
  buf[len] = '\0';
  return len;
}
