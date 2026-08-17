#pragma once

#include <Arduino.h>

#include "aqi.h"
#include "forecast.h"
#include "thermostat.h"

void initDisplay();
// Redraws the dashboard's chrome (background + header) - call after returning
// from the thermostat page, then re-call the showX() functions with your last
// known values to repopulate the panels.
void redrawDashboardLayout();
void showWifiStatus(bool connected);
// dateStr "MM/DD/YY", timeStr "HH:MM" (24hr) - formatting is main.cpp's job.
void showDateTime(const String &dateStr, const String &timeStr);
void showIndoor(float tempF, float pressureMbar, float gasKOhm, const char *airQualityLabel);
void showOutside(bool ok, float tempF, const String &condition, unsigned long updatedAgoSeconds, const AqiResult &aqi);

// Tap the Outside faceplate on the dashboard to open the 7-day forecast page.
bool hitTestOutsidePanel(int touchX, int touchY);

enum class AlarmDisplayState
{
  NoData,
  Clear,
  Active,      // unacknowledged - shows the ACKNOWLEDGE button
  Acknowledged // operator has seen it, but the underlying condition hasn't cleared yet
};
void showAlert(AlarmDisplayState state, const String &text);
// Only hits when an Active (unacknowledged) alarm is actually showing the button.
bool hitTestAcknowledgeButton(int touchX, int touchY);

void showNews(bool ok, const String *headlines, size_t count);

// Tap the Indoor faceplate on the dashboard to open the thermostat page.
bool hitTestIndoorPanel(int touchX, int touchY);

enum class ThermoButton
{
  None,
  Back,
  ModeOff,
  ModeHeat,
  ModeCool,
  ModeAuto,
  HeatUp,
  HeatDown,
  CoolUp,
  CoolDown
};

// Full redraw - call only when first entering the thermostat page.
void showThermostatPage(const ThermostatState &state, CallState callState, float indoorTempF);
// Lightweight refresh for value changes - use this instead of showThermostatPage()
// once already on the page, or every update flashes the whole screen.
void updateThermostatValues(const ThermostatState &state, CallState callState, float indoorTempF);
ThermoButton hitTestThermostatPage(int touchX, int touchY);

enum class ForecastButton
{
  None,
  Back
};

void showForecastPage(const ForecastResult &forecast);
ForecastButton hitTestForecastPage(int touchX, int touchY);
