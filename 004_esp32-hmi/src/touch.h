#pragma once

bool initTouch();
// Pure "is a finger on the glass right now" check - use this to edge-detect
// press/release, since it's independent of whatever the touch-point read
// below is doing.
bool isTouching();
// Raw FT6336U coordinates, in the panel's native (unrotated) orientation -
// not yet mapped to the landscape screen space main.cpp/display.cpp use.
bool readTouchRaw(int &x, int &y);
// Calibrated to the 480x320 landscape screen space (matches tft.setRotation(1)).
bool readTouchScreen(int &x, int &y);
