#pragma once
#include <stdint.h>

// Shared ESP-NOW radio bring-up. No WiFi AP connection at all -- badges no
// longer depend on any router/hotspot for communication. All badges must
// agree on the same fixed channel up front since there's no AP to inherit
// one from.
//
// ESP-NOW only supports one physical receive callback for the whole radio, so
// modules (proximity ranging, mesh broadcast) share it here, demuxed by a
// 1-byte packet-type tag prefixed to every payload.

#define ESPNOW_CHANNEL 6

void setupEspNowRadio();

// Sends a single-hop packet to every badge in range.
void espNowSend(const uint8_t *data, int len);

// mac: sender's MAC address (6 bytes). rssi: signal strength of this packet
// in dBm (read directly off the receive callback -- IDF 5.1+ exposes this via
// esp_now_recv_info_t, no promiscuous-mode sniffing needed). data/len: the
// payload with the packet-type tag already stripped.
typedef void (*EspNowRecvHandler)(const uint8_t *mac, int rssi, const uint8_t *data, int len);

// Registers a handler for packets whose first byte equals packetType.
void espNowOnReceive(uint8_t packetType, EspNowRecvHandler handler);
