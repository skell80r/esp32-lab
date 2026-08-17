#pragma once

struct IndoorReading
{
  bool valid;
  float tempF;
  float pressureMbar;
  float gasKOhm;
  const char *airQualityLabel;
};

bool initSensors();
IndoorReading readIndoor();
