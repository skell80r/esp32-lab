#include "thermostat.h"

#include <Arduino.h>

ThermostatState thermostat;

static const float HYSTERESIS_F = 1.0f;         // temp swing below/above setpoint before calling
static const float MIN_SETPOINT_GAP_F = 3.0f;   // enforced between heat/cool setpoints in Auto
static const float MIN_SETPOINT_F = 45.0f;
static const float MAX_SETPOINT_F = 90.0f;

static CallState currentCall = CallState::Idle;

void setThermostatMode(ThermoMode mode)
{
  thermostat.mode = mode;
}

void adjustHeatSetpoint(float deltaF)
{
  float updated = thermostat.heatSetpointF + deltaF;
  updated = constrain(updated, MIN_SETPOINT_F, thermostat.coolSetpointF - MIN_SETPOINT_GAP_F);
  thermostat.heatSetpointF = updated;
}

void adjustCoolSetpoint(float deltaF)
{
  float updated = thermostat.coolSetpointF + deltaF;
  updated = constrain(updated, thermostat.heatSetpointF + MIN_SETPOINT_GAP_F, MAX_SETPOINT_F);
  thermostat.coolSetpointF = updated;
}

CallState computeCallState(float indoorTempF)
{
  bool heatAllowed = thermostat.mode == ThermoMode::Heat || thermostat.mode == ThermoMode::Auto;
  bool coolAllowed = thermostat.mode == ThermoMode::Cool || thermostat.mode == ThermoMode::Auto;

  if (currentCall == CallState::Heating && (!heatAllowed || indoorTempF >= thermostat.heatSetpointF))
  {
    currentCall = CallState::Idle;
  }
  else if (currentCall == CallState::Cooling && (!coolAllowed || indoorTempF <= thermostat.coolSetpointF))
  {
    currentCall = CallState::Idle;
  }

  if (currentCall == CallState::Idle)
  {
    if (heatAllowed && indoorTempF <= thermostat.heatSetpointF - HYSTERESIS_F)
    {
      currentCall = CallState::Heating;
    }
    else if (coolAllowed && indoorTempF >= thermostat.coolSetpointF + HYSTERESIS_F)
    {
      currentCall = CallState::Cooling;
    }
  }

  return currentCall;
}
