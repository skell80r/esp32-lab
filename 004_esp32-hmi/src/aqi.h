#pragma once

#include <Arduino.h>

struct AqiResult
{
  bool ok;
  int usAqi;
  String category; // "Good", "Moderate", "Unhealthy", etc. (US EPA AQI bands)
};

AqiResult fetchAirQuality();
