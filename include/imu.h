#pragma once

bool setupIMU();
void getRollPitch(float &roll, float &pitch);

// Total acceleration magnitude in g (~1.0 at rest, spikes when shaken).
float getAccelMagnitude();

// Raw per-axis acceleration in g. When held upright, gravity lies in the x/y
// plane so atan2(ay,ax) tracks the fan/tilt angle; magnitude = sqrt(sum sq).
void getAccel(float &ax, float &ay, float &az);
