#pragma once

bool setupIMU();
void getRollPitch(float &roll, float &pitch);

// Total acceleration magnitude in g (~1.0 at rest, spikes when shaken).
float getAccelMagnitude();
