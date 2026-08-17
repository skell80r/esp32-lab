#pragma once

#include <Arduino.h>

struct WeatherResult
{
  bool ok;
  float tempF;
  String condition;
};

WeatherResult fetchOutsideWeather();
