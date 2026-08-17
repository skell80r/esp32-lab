#pragma once

#include <Arduino.h>

static const size_t FORECAST_DAYS = 7;

struct ForecastDay
{
  String dayLabel;       // "MON", "TUE", ...
  String conditionShort; // "RAIN", "CLR", ... - abbreviated to fit a narrow column
  float highF;
  float lowF;
};

struct ForecastResult
{
  bool ok;
  ForecastDay days[FORECAST_DAYS];
  size_t count;
};

ForecastResult fetchForecast();
