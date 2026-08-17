#include <Arduino.h>
#include <Wire.h>

#include "alerts.h"
#include "aqi.h"
#include "clock.h"
#include "display.h"
#include "forecast.h"
#include "net.h"
#include "news.h"
#include "sensors.h"
#include "thermostat.h"
#include "touch.h"
#include "weather.h"

/*
BME680:
  21 -> SDA
  22 -> SCL

LCD (ST7796U, SPI):
  18 -> SCK
  23 -> SDI (MOSI)
  19 -> SDO (MISO)
  13 -> LCD_RS (DC)
  27 -> LCD_CS
  14 -> LCD_RST
  25 -> LED backlight (PWM)

Touch (FT6336U, I2C - shares the BME680's bus):
  21 -> SDA
  22 -> SCL
  33 -> RST

Copy secrets.h.example to secrets.h and fill in WiFi/location/API key
values before building.

```
  pio run -t upload
  pio device monitor
```
*/

static const unsigned long INDOOR_INTERVAL_MS = 2000;
static const unsigned long WEATHER_INTERVAL_MS = 10UL * 60 * 1000;
static const unsigned long AQI_INTERVAL_MS = 10UL * 60 * 1000; // AQI changes slowly - same cadence as weather
static const unsigned long ALERTS_INTERVAL_MS = 5UL * 60 * 1000;
static const unsigned long NEWS_INTERVAL_MS = 30UL * 60 * 1000;
// Only redraws the "updated Xm ago" counter - separate from the fetch itself.
static const unsigned long OUTSIDE_REDRAW_INTERVAL_MS = 30UL * 1000;
// No seconds shown, so minute-level freshness is plenty - avoids a redraw every second.
static const unsigned long CLOCK_INTERVAL_MS = 5000;

static unsigned long lastIndoorAt = 0;
// Seeded so "now - lastXAt >= INTERVAL" is already true on the first loop()
// pass, giving an immediate fetch at boot instead of waiting a full interval.
static unsigned long lastWeatherAt = 0 - WEATHER_INTERVAL_MS;
static unsigned long lastAqiAt = 0 - AQI_INTERVAL_MS;
static unsigned long lastAlertsAt = 0 - ALERTS_INTERVAL_MS;
static unsigned long lastNewsAt = 0 - NEWS_INTERVAL_MS;
static unsigned long lastOutsideRedrawAt = 0 - OUTSIDE_REDRAW_INTERVAL_MS;
static unsigned long lastClockAt = 0 - CLOCK_INTERVAL_MS;
static unsigned long weatherFetchedAt = 0;

static WeatherResult lastWeather{};
static AqiResult lastAqi{};
static ForecastResult lastForecast{};
static AlertResult lastAlerts{};
static NewsResult lastNews{};
static IndoorReading lastIndoor{};
static bool lastWifiOk = false;
static bool wifiStatusShown = false;
// Headline the operator has acknowledged - silences that specific alert until
// it clears or a genuinely different one comes in.
static String acknowledgedHeadline;

static AlarmDisplayState currentAlarmDisplayState()
{
  if (!lastAlerts.ok)
  {
    return AlarmDisplayState::NoData;
  }
  if (!lastAlerts.active)
  {
    acknowledgedHeadline = ""; // condition cleared - reset for the next event
    return AlarmDisplayState::Clear;
  }
  if (lastAlerts.headline == acknowledgedHeadline)
  {
    return AlarmDisplayState::Acknowledged;
  }
  return AlarmDisplayState::Active;
}

enum class Page
{
  Dashboard,
  Thermostat,
  Forecast
};
static Page currentPage = Page::Dashboard;

static void redrawDashboard()
{
  redrawDashboardLayout();
  showWifiStatus(lastWifiOk);
  String dateStr, timeStr;
  if (getFormattedDateTime(dateStr, timeStr))
  {
    showDateTime(dateStr, timeStr);
  }
  if (lastIndoor.valid)
  {
    showIndoor(lastIndoor.tempF, lastIndoor.pressureMbar, lastIndoor.gasKOhm, lastIndoor.airQualityLabel);
  }
  showOutside(lastWeather.ok, lastWeather.tempF, lastWeather.condition,
              (millis() - weatherFetchedAt) / 1000, lastAqi);
  showAlert(currentAlarmDisplayState(), lastAlerts.headline);
  showNews(lastNews.ok, lastNews.headlines, lastNews.count);
}

static void enterThermostatPage()
{
  CallState call = computeCallState(lastIndoor.tempF);
  showThermostatPage(thermostat, call, lastIndoor.tempF);
}

static void refreshThermostatValues()
{
  CallState call = computeCallState(lastIndoor.tempF);
  updateThermostatValues(thermostat, call, lastIndoor.tempF);
}

static void enterForecastPage()
{
  // Fetched on demand rather than in the background - it's a rarely-visited
  // detail page, so a brief pause on open is fine and avoids another
  // periodic timer cluttering the main dashboard loop.
  lastForecast = fetchForecast();
  showForecastPage(lastForecast);
}

static void handleTouchTap(int tx, int ty)
{
  if (currentPage == Page::Dashboard)
  {
    if (hitTestAcknowledgeButton(tx, ty))
    {
      if (currentAlarmDisplayState() == AlarmDisplayState::Active)
      {
        acknowledgedHeadline = lastAlerts.headline;
        showAlert(currentAlarmDisplayState(), lastAlerts.headline);
      }
      return;
    }
    if (hitTestIndoorPanel(tx, ty))
    {
      currentPage = Page::Thermostat;
      enterThermostatPage();
      return;
    }
    if (hitTestOutsidePanel(tx, ty))
    {
      currentPage = Page::Forecast;
      enterForecastPage();
    }
    return;
  }

  if (currentPage == Page::Forecast)
  {
    if (hitTestForecastPage(tx, ty) == ForecastButton::Back)
    {
      currentPage = Page::Dashboard;
      redrawDashboard();
    }
    return;
  }

  ThermoButton btn = hitTestThermostatPage(tx, ty);
  switch (btn)
  {
  case ThermoButton::Back:
    currentPage = Page::Dashboard;
    redrawDashboard();
    return;
  case ThermoButton::ModeOff:
    setThermostatMode(ThermoMode::Off);
    break;
  case ThermoButton::ModeHeat:
    setThermostatMode(ThermoMode::Heat);
    break;
  case ThermoButton::ModeCool:
    setThermostatMode(ThermoMode::Cool);
    break;
  case ThermoButton::ModeAuto:
    setThermostatMode(ThermoMode::Auto);
    break;
  case ThermoButton::HeatUp:
    adjustHeatSetpoint(1.0f);
    break;
  case ThermoButton::HeatDown:
    adjustHeatSetpoint(-1.0f);
    break;
  case ThermoButton::CoolUp:
    adjustCoolSetpoint(1.0f);
    break;
  case ThermoButton::CoolDown:
    adjustCoolSetpoint(-1.0f);
    break;
  case ThermoButton::None:
    return;
  }
  refreshThermostatValues();
}

void setup()
{
  Serial.begin(115200);

  initDisplay();

  if (!initSensors())
  {
    Serial.println("Could not find a BME680 sensor, check wiring!");
    while (1)
      delay(10);
  }

  if (!initTouch())
  {
    Serial.println("Could not find FT6336U touch controller, check wiring!");
  }

  bool wifiOk = connectWiFi();
  showWifiStatus(wifiOk);
  lastWifiOk = wifiOk;
  wifiStatusShown = true;
  if (!wifiOk)
  {
    Serial.println("WiFi connect failed - outside weather/alerts/news will stay unavailable");
  }

  // Draw initial placeholders once so the panels aren't blank while the
  // first fetch (triggered on the first loop() pass) is still in flight.
  showOutside(lastWeather.ok, lastWeather.tempF, lastWeather.condition, 0, lastAqi);
  showAlert(currentAlarmDisplayState(), lastAlerts.headline);
  showNews(lastNews.ok, lastNews.headlines, lastNews.count);
}

void loop()
{
  unsigned long now = millis();

  {
    // Edge-detect on physical presence, not on whether a fresh coordinate
    // happened to be ready - readTouchScreen() can transiently return false
    // between the chip's periodic refreshes even while still pressed.
    static bool wasPressed = false;
    bool pressed = isTouching();
    if (pressed && !wasPressed)
    {
      // Require two consecutive reads to agree (within 40px) before trusting
      // the coordinate - filters out one-off garbled reads without needing
      // any hardware interrupt signal.
      const int AGREEMENT_TOLERANCE_PX = 40;
      int tx, ty;
      bool gotPoint = false;
      if (readTouchScreen(tx, ty))
      {
        for (int attempt = 0; attempt < 5 && !gotPoint; attempt++)
        {
          delay(10);
          int tx2, ty2;
          if (!readTouchScreen(tx2, ty2))
          {
            continue; // no read this cycle - keep comparing against last known point
          }
          if (abs(tx2 - tx) <= AGREEMENT_TOLERANCE_PX && abs(ty2 - ty) <= AGREEMENT_TOLERANCE_PX)
          {
            gotPoint = true;
          }
          else
          {
            tx = tx2;
            ty = ty2;
          }
        }
      }
      if (gotPoint)
      {
        handleTouchTap(tx, ty);
      }
    }
    wasPressed = pressed;
  }

  if (now - lastIndoorAt >= INDOOR_INTERVAL_MS)
  {
    lastIndoorAt = now;
    IndoorReading reading = readIndoor();
    if (reading.valid)
    {
      lastIndoor = reading;
      if (currentPage == Page::Dashboard)
      {
        showIndoor(reading.tempF, reading.pressureMbar, reading.gasKOhm, reading.airQualityLabel);
      }
      else if (currentPage == Page::Thermostat)
      {
        refreshThermostatValues();
      }
      // Forecast page has nothing indoor-related to refresh.
    }
  }

  bool wifiOk = wifiIsConnected();
  if (!wifiOk)
  {
    wifiOk = connectWiFi(5000);
  }
  if (currentPage == Page::Dashboard && (!wifiStatusShown || wifiOk != lastWifiOk))
  {
    showWifiStatus(wifiOk);
  }
  lastWifiOk = wifiOk;
  wifiStatusShown = true;

  {
    static bool clockInitialized = false;
    if (wifiOk && !clockInitialized)
    {
      initClock();
      clockInitialized = true;
    }
  }
  if (wifiOk && currentPage == Page::Dashboard && now - lastClockAt >= CLOCK_INTERVAL_MS)
  {
    lastClockAt = now;
    String dateStr, timeStr;
    if (getFormattedDateTime(dateStr, timeStr))
    {
      showDateTime(dateStr, timeStr);
    }
  }

  if (wifiOk && now - lastWeatherAt >= WEATHER_INTERVAL_MS)
  {
    lastWeatherAt = now;
    lastWeather = fetchOutsideWeather();
    weatherFetchedAt = now;
    lastOutsideRedrawAt = now;
    if (currentPage == Page::Dashboard)
    {
      showOutside(lastWeather.ok, lastWeather.tempF, lastWeather.condition,
                  (now - weatherFetchedAt) / 1000, lastAqi);
    }
  }
  else if (currentPage == Page::Dashboard && now - lastOutsideRedrawAt >= OUTSIDE_REDRAW_INTERVAL_MS)
  {
    lastOutsideRedrawAt = now;
    showOutside(lastWeather.ok, lastWeather.tempF, lastWeather.condition,
                (now - weatherFetchedAt) / 1000, lastAqi);
  }

  if (wifiOk && now - lastAqiAt >= AQI_INTERVAL_MS)
  {
    lastAqiAt = now;
    lastAqi = fetchAirQuality();
    if (currentPage == Page::Dashboard)
    {
      showOutside(lastWeather.ok, lastWeather.tempF, lastWeather.condition,
                  (now - weatherFetchedAt) / 1000, lastAqi);
    }
  }

  if (wifiOk && now - lastAlertsAt >= ALERTS_INTERVAL_MS)
  {
    lastAlertsAt = now;
    lastAlerts = fetchSevereAlerts();
    if (currentPage == Page::Dashboard)
    {
      showAlert(currentAlarmDisplayState(), lastAlerts.headline);
    }
  }

  if (wifiOk && now - lastNewsAt >= NEWS_INTERVAL_MS)
  {
    lastNewsAt = now;
    lastNews = fetchTopHeadlines();
    if (currentPage == Page::Dashboard)
    {
      showNews(lastNews.ok, lastNews.headlines, lastNews.count);
    }
  }
}
