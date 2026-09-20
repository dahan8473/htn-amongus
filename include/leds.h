#pragma once

#include <stdint.h>

void setupLEDs();
void updateLEDs();

// Solid-color flash held for durationMs, overriding the rainbow animation,
// after which updateLEDs() resumes it automatically.
void flashLEDs(uint8_t r, uint8_t g, uint8_t b, unsigned long durationMs);
