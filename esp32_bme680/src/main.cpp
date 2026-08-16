#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme;


/*
BME680:
  21 -> SDA
  22 -> SCL

pio run -t upload and pio device monitor
*/
void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("BME680 hello world");

  if (!bme.begin())
  {
    Serial.println("Could not find a BME680 sensor, check wiring!");
    while (1)
      delay(10);
  }

  // Oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop()
{
  if (!bme.performReading())
  {
    Serial.println("Failed to perform reading");
    delay(2000);
    return;
  }

  Serial.print("Temperature = ");
  Serial.print(bme.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println();
  delay(2000);
}
