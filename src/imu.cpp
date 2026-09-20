#include <Arduino.h>
#include <Wire.h>
#include "imu.h"

#define I2C_SDA 5
#define I2C_SCL 6
#define SC7A20_ADDRESS 0x19

// Helper function for safe I2C writes with retries
bool writeRegister(uint8_t reg, uint8_t val) {
  for (int i = 0; i < 3; i++) { // Max 3 attempts
    Wire.beginTransmission(SC7A20_ADDRESS);
    Wire.write(reg);
    Wire.write(val);
    if (Wire.endTransmission() == 0) return true;
    delay(2); // Short backoff before retry
  }
  return false;
}

// Helper function for safe I2C single-byte reads with retries
bool readRegister(uint8_t reg, uint8_t &val) {
  for (int i = 0; i < 3; i++) {
    Wire.beginTransmission(SC7A20_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) continue;
    
    if (Wire.requestFrom((uint16_t)SC7A20_ADDRESS, (uint8_t)1) == 1) {
      val = Wire.read();
      return true;
    }
    delay(2);
  }
  return false;
}

bool setupIMU() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // Set to 400 kHz Fast Mode
  Wire.setTimeOut(15);   // Enforce a 15ms hardware timeout to prevent infinite hanging
  
  // Verify WHO_AM_I (0x0F) reads 0x11
  uint8_t whoAmI = 0;
  if (!readRegister(0x0F, whoAmI) || whoAmI != 0x11) {
    return false; 
  }
  
  // CTRL_REG1 (0x20) = 0x57 -> 100 Hz, all axes on
  if (!writeRegister(0x20, 0x57)) return false;
  
  // CTRL_REG4 (0x23) = 0x80 -> +/- 2g scale, continuous update
  if (!writeRegister(0x23, 0x80)) return false;
  
  return true;
}

void getRollPitch(float &roll, float &pitch) {
  uint8_t status = 0;
  bool dataReady = false;
  
  // Poll STATUS (0x27) for ZYXDA (bit 3) with a strict 5ms bounded timeout
  uint32_t startPoll = millis();
  while (millis() - startPoll < 5) {
    if (readRegister(0x27, status) && (status & 0x08)) {
      dataReady = true;
      break;
    }
  }
  
  // If data isn't ready or bus is busy, abort and retain the previous roll/pitch values
  if (!dataReady) return; 
  
  // Read 6 bytes from OUT_X_L (0x28) with auto-increment (MSB 0x80 set)
  for (int i = 0; i < 3; i++) {
    Wire.beginTransmission(SC7A20_ADDRESS);
    Wire.write(0x28 | 0x80); 
    if (Wire.endTransmission(false) != 0) continue;
    
    if (Wire.requestFrom((uint16_t)SC7A20_ADDRESS, (uint8_t)6) == 6) {
      // Reconstruct 16-bit left-justified data
      int16_t x_raw = Wire.read() | (Wire.read() << 8);
      int16_t y_raw = Wire.read() | (Wire.read() << 8);
      int16_t z_raw = Wire.read() | (Wire.read() << 8);
      
      // Shift right by 4 bits to extract the 12-bit payload.
      // Casting to signed int16_t first ensures the sign bit is preserved during shift.
      // At +/- 2g, 1 count = 1 mg.
      float x_mg = (float)(x_raw >> 4);
      float y_mg = (float)(y_raw >> 4);
      float z_mg = (float)(z_raw >> 4);
      
      // Calculate final angles
      roll = atan2(y_mg, z_mg) * 180.0 / PI;
      pitch = atan2(-x_mg, sqrt(y_mg * y_mg + z_mg * z_mg)) * 180.0 / PI;
      
      break; // Success, exit retry loop
    }
    delay(1); // Brief backoff if requestFrom fails
  }
}
