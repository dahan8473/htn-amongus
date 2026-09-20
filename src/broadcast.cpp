#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>
#include "broadcast.h"
#include "wifi_sta.h"
#include "leds.h"
#include "buttons.h"

#define BROADCAST_PORT 4210
#define ALERT_MSG "ALERT"
#define ALERT_FLASH_MS 5000

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

static void sendAlert() {
  udp.beginPacket(broadcastAddress(), BROADCAST_PORT);
  udp.write((const uint8_t *)ALERT_MSG, strlen(ALERT_MSG));
  udp.endPacket();
}

void setupBroadcast() {
  udp.begin(BROADCAST_PORT);
}

void updateBroadcast() {
  if (!isWiFiConnected()) {
    return;
  }

  if (isButtonPressed(BTN_START)) {
    Serial.println("START pressed, broadcasting alert to all badges");
    sendAlert();
    flashLEDs(80, 0, 0, ALERT_FLASH_MS); // flash our own LEDs immediately too
  }

  int packetSize = udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }
  char buf[16];
  int len = udp.read(buf, sizeof(buf) - 1);
  if (len <= 0) {
    return;
  }
  buf[len] = '\0';
  if (strcmp(buf, ALERT_MSG) == 0) {
    Serial.println("alert received, flashing red");
    flashLEDs(80, 0, 0, ALERT_FLASH_MS);
  }
}
