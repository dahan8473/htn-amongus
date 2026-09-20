#include <Arduino.h>
#include <Wire.h>
#include "MFRC522_I2C.h"
#include "nfc.h"

#define NFC_ADDRESS 0x26
#define RST_PIN -1 // Relying on I2C soft-reset, no dedicated GPIO

// In this specific library fork, the MFRC522 class handles I2C natively
MFRC522 mfrc522(NFC_ADDRESS, RST_PIN);

void beginNFCScan() {
  // Manual Soft Power Up: Clear the PowerDown bit (bit 4) in CommandReg (0x01)
  mfrc522.PCD_ClearRegisterBitMask(mfrc522.CommandReg, (1<<4));
  
  // The oscillator needs a moment to stabilize after waking up
  delay(50); 
  
  // Re-initialize registers after waking up
  mfrc522.PCD_Init();
}

void powerDownNFC() {
  // Turn off the antenna to save power
  mfrc522.PCD_AntennaOff();
  
  // Manual Soft Power Down: Set the PowerDown bit (bit 4) in CommandReg (0x01)
  // This drops the chip's current consumption to ~10uA
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
