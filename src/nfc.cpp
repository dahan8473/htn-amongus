#include <Arduino.h>
#include <Wire.h>
#include "MFRC522_I2C.h"
#include "nfc.h"

#define NFC_ADDRESS 0x26
#define RST_PIN -1 // Relying on I2C soft-reset, no dedicated GPIO

// In this specific library fork, the MFRC522 class handles I2C natively
MFRC522 mfrc522(NFC_ADDRESS, RST_PIN);
static bool nfcInitialized = false;

void setupNFC() {
  // Configure the reader once. Later AUX1 toggles use the retained soft
  // power-down state instead of resetting/reinitializing the chip in-game.
  mfrc522.PCD_Init();
  nfcInitialized = true;
  powerDownNFC();

  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print("NFC reader VersionReg=0x");
  Serial.println(version, HEX);
}

void beginNFCScan() {
  if (!nfcInitialized) {
    setupNFC();
  }

  // Manual soft wake: clear PowerDown while retaining the configured reader
  // registers, then enable the RF driver. No hardware reset or full PCD_Init
  // is performed when the switch changes during a game.
  mfrc522.PCD_ClearRegisterBitMask(mfrc522.CommandReg, (1<<4));
  delay(2);
  mfrc522.PCD_AntennaOn();
}

void powerDownNFC() {
  // Put the reader into low-power standby without cutting its supply. The
  // antenna is disabled first, then the MFRC522's retained PowerDown bit
  // stops its oscillator. Clearing that bit in beginNFCScan() wakes it
  // quickly without a full hardware power-cycle.
  mfrc522.PCD_AntennaOff();
  mfrc522.PCD_SetRegisterBitMask(mfrc522.CommandReg, (1<<4));
}

String scanNFC() {
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return ""; 
  }

  // Extract the UID into a hex string
  String uidStr = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      uidStr += "0";
    }
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();

  // Halt PICC so it doesn't repeatedly trigger while the sticker is still near
  mfrc522.PICC_HaltA();
  
  return uidStr;
}
