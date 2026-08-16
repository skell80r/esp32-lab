#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME680.h>
#include <LovyanGFX.hpp>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme;

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void)
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = VSPI_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read = 16000000;
    cfg.spi_3wire = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = 18; // denky32 default VSPI SCK
    cfg.pin_mosi = 23; // denky32 default VSPI MOSI (SDI)
    cfg.pin_miso = 19; // denky32 default VSPI MISO (SDO)
    cfg.pin_dc = 13;   // TODO: set to LCD_RS wiring
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    auto pcfg = _panel_instance.config();
    pcfg.pin_cs = 27;  // TODO: set to LCD_CS wiring
    pcfg.pin_rst = 14; // TODO: set to LCD_RST wiring
    pcfg.pin_busy = -1;  // only necessary for e-ink displays, per claude
    pcfg.panel_width = 320;
    pcfg.panel_height = 480;
    _panel_instance.config(pcfg);

    auto bl_cfg = _light_instance.config();
    bl_cfg.pin_bl = 25; // TODO: set to LED backlight wiring
    bl_cfg.freq = 44100;
    bl_cfg.pwm_channel = 7;
    _light_instance.config(bl_cfg);
    _panel_instance.setLight(&_light_instance);

    setPanel(&_panel_instance);
  }
};

LGFX tft;

/*
BME680:
  21 -> SDA
  22 -> SCL

LCD (ST7796U, SPI):
  18 -> SCK
  23 -> SDI (MOSI)
  19 -> SDO (MISO)
  LCD_CS / LCD_RS / LCD_RST / LED -> TODO, fill in tft config above once wired

```
  pio run -t upload
  pio device monitor
```
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

  tft.init();
  tft.setRotation(1);
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
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

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setCursor(20, 20);
  tft.print("Temp: ");
  tft.print(bme.temperature, 1);
  tft.println(" C");

  tft.setCursor(20, 40);
  tft.print("Pressure = ");
  tft.print(bme.pressure / 100.0);
  tft.println(" hPa");


  delay(2000);
}
