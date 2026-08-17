#include "touch.h"

#include <Adafruit_FT6206.h>
#include <Arduino.h>

static const int CTP_RST_PIN = 33;

static Adafruit_FT6206 ctp = Adafruit_FT6206();

bool initTouch()
{
  // FT6336U needs its active-low reset line driven to come out of reset;
  // left floating, it never responds on I2C at all.
  pinMode(CTP_RST_PIN, OUTPUT);
  digitalWrite(CTP_RST_PIN, LOW);
  delay(10);
  digitalWrite(CTP_RST_PIN, HIGH);
  delay(300); // datasheet-typical post-reset settle time

  return ctp.begin();
}

bool isTouching()
{
  return ctp.touched();
}

bool readTouchRaw(int &x, int &y)
{
  if (!ctp.touched())
  {
    return false;
  }
  TS_Point p = ctp.getPoint();
  x = p.x;
  y = p.y;
  return true;
}

// Calibrated by tapping all four corners: the touch chip's active sensing
// range doesn't span the full panel resolution, and both axes read inverted
// relative to the 480x320 landscape screen space. Adjust these four raw
// extremes here if hit-testing feels off in practice.
static const int RAW_X_AT_RIGHT_EDGE = 32;
static const int RAW_X_AT_LEFT_EDGE = 260;
static const int RAW_Y_AT_BOTTOM_EDGE = 36;
static const int RAW_Y_AT_TOP_EDGE = 446;

bool readTouchScreen(int &x, int &y)
{
  int rawX, rawY;
  if (!readTouchRaw(rawX, rawY))
  {
    return false;
  }
  x = map(rawX, RAW_X_AT_LEFT_EDGE, RAW_X_AT_RIGHT_EDGE, 0, 479);
  y = map(rawY, RAW_Y_AT_BOTTOM_EDGE, RAW_Y_AT_TOP_EDGE, 0, 319);
  x = constrain(x, 0, 479);
  y = constrain(y, 0, 319);
  return true;
}
