#include <Arduino.h>
#include <Wire.h>
#include "imu.h"

#define I2C_SDA 5
#define I2C_SCL 6
#define SC7A20_ADDRESS 0x19

bool setupIMU() {
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Configure CTRL_REG1 (0x20): 100Hz ODR, Normal mode, X/Y/Z axes enabled (0x57)
  Wire.beginTransmission(SC7A20_ADDRESS);
  Wire.write(0x20); 
  Wire.write(0x57); 
  if (Wire.endTransmission() != 0) {
    return false; // I2C communication failed
  }
  
  // Configure CTRL_REG4 (0x23): Block Data Update enabled, High Resolution mode (0x88)
  Wire.beginTransmission(SC7A20_ADDRESS);
  Wire.write(0x23);
  Wire.write(0x88);
  Wire.endTransmission();
  
  return true;
}

void getRollPitch(float &roll, float &pitch) {
  // Read 6 bytes starting from OUT_X_L (0x28). 
  // The MSB (0x80) must be set to auto-increment the register address during read.
  Wire.beginTransmission(SC7A20_ADDRESS);
  Wire.write(0x28 | 0x80); 
  Wire.endTransmission(false); // Send repeated start
  
  Wire.requestFrom((uint16_t)SC7A20_ADDRESS, (uint8_t)6);
  
  if (Wire.available() == 6) {
    // Read the 16-bit 2's complement left-justified data
    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);
    int16_t z = Wire.read() | (Wire.read() << 8);
    
    // Calculate angles based on raw vector ratios
    roll = atan2((float)y, (float)z) * 180.0 / PI;
    pitch = atan2(-(float)x, sqrt((float)y * y + (float)z * z)) * 180.0 / PI;
  }
}
