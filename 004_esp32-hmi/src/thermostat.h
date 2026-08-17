#pragma once

enum class ThermoMode
{
  Off,
  Heat,
  Cool,
  Auto
};

enum class CallState
{
  Idle,
  Heating,
  Cooling
};

struct ThermostatState
{
  ThermoMode mode = ThermoMode::Off;
  float heatSetpointF = 68.0f;
  float coolSetpointF = 76.0f;
};

extern ThermostatState thermostat;

void setThermostatMode(ThermoMode mode);
void adjustHeatSetpoint(float deltaF);
void adjustCoolSetpoint(float deltaF);

// Applies hysteresis around the current indoor temp and returns what the
// system should be doing right now. Does not drive any relay hardware yet -
// that's a later pass once the relay module is wired in.
CallState computeCallState(float indoorTempF);
