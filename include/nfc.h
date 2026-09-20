#pragma once
#include <Arduino.h>

// Call this ONLY when the device is ready to accept a "task completion" scan
void beginNFCScan();

// Poll this in your main loop while scanning is active
String scanNFC();

// Call this to power down the MFRC522 immediately after a successful scan or timeout
void powerDownNFC();
