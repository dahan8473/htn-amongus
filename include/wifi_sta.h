#pragma once

// Puts the radio in station mode on a fixed channel for ESP-NOW. Does NOT
// connect to any router -- the badges talk peer-to-peer, so no credentials or
// access point are needed.
void setupWiFi();

// Always true now (kept for API compatibility): the ESP-NOW bus needs no AP.
bool isWiFiConnected();
