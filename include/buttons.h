#pragma once
#include <Arduino.h>

// Button bitmasks aligned with the shift register read order
#define BTN_A      (1 << 0)
#define BTN_B      (1 << 1)
#define BTN_HOME   (1 << 2)
#define BTN_DOWN   (1 << 3)
#define BTN_LEFT   (1 << 4)
#define BTN_RIGHT  (1 << 5)
#define BTN_UP     (1 << 6)
#define BTN_AUX1   (1 << 7) // Maintained switch
#define BTN_START  (1 << 8) // Dedicated GPIO 9

void setupButtons();
void updateButtons();

// Returns the raw bitmask of all currently debounced buttons
uint16_t getButtons();

// Returns true only once when the button is first pressed down
bool isButtonPressed(uint16_t buttonMask);

// Returns true as long as the button is held down (or maintained switch is ON)
bool isButtonHeld(uint16_t buttonMask);
