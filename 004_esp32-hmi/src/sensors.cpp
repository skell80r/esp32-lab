#include "sensors.h"

#include <Adafruit_BME680.h>

static Adafruit_BME680 bme;

bool initSensors()
{
  if (!bme.begin())
  {
    return false;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
  return true;
}

// BME680 gas resistance is a raw, uncalibrated proxy for VOC levels, not a
// true AQI - it needs a multi-day burn-in and a per-device baseline (e.g. via
// Bosch's BSEC library) before the numbers mean anything absolute. These
// thresholds are illustrative only; treat the label as "better/worse than a
// moment ago", not a certified air quality reading.
static const char *classifyGasResistance(float gasKOhm)
{
  if (gasKOhm > 50.0f)
    return "Good";
  if (gasKOhm > 20.0f)
    return "Moderate";
  return "Poor";
}

IndoorReading readIndoor()
{
  IndoorReading reading{};
  reading.valid = bme.performReading();
  if (!reading.valid)
  {
    return reading;
  }

  reading.tempF = bme.temperature * 9.0f / 5.0f + 32.0f;
  reading.pressureMbar = bme.pressure / 100.0f; // 1 hPa == 1 mbar, no conversion needed
  reading.gasKOhm = bme.gas_resistance / 1000.0f;
  reading.airQualityLabel = classifyGasResistance(reading.gasKOhm);
  return reading;
}
