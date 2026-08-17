#pragma once

#include <Arduino.h>

struct AlertResult
{
  bool ok;
  bool active;
  String headline;
};

AlertResult fetchSevereAlerts();
