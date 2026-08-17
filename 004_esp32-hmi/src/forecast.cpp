#include "forecast.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "secrets.h"

// Same WMO code table as weather.cpp, condensed to fit a ~68px-wide column.
static String shortWeatherCode(int code)
{
  switch (code)
  {
  case 0:
    return "CLR";
  case 1:
  case 2:
    return "P.CLDY";
  case 3:
    return "CLOUDY";
  case 45:
  case 48:
    return "FOG";
  case 51:
  case 53:
  case 55:
    return "DRZL";
  case 56:
  case 57:
    return "F.DRZL";
  case 61:
  case 63:
  case 65:
    return "RAIN";
  case 66:
  case 67:
    return "F.RAIN";
  case 71:
  case 73:
  case 75:
  case 77:
    return "SNOW";
  case 80:
  case 81:
  case 82:
    return "SHWRS";
  case 85:
  case 86:
    return "SNSHWR";
  case 95:
  case 96:
  case 99:
    return "TSTRM";
  default:
    return "?";
  }
}

static String dayOfWeekAbbrev(int year, int month, int day)
{
  struct tm timeinfo = {};
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  timeinfo.tm_hour = 12; // noon - avoids any DST/midnight-boundary date shift
  time_t t = mktime(&timeinfo);
  struct tm *result = localtime(&t);

  char buf[4];
  strftime(buf, sizeof(buf), "%a", result);
  String s = buf;
  s.toUpperCase();
  return s;
}

ForecastResult fetchForecast()
{
  ForecastResult result{};
  result.ok = false;
  result.count = 0;

  // setInsecure() skips TLS certificate validation - see weather.cpp for the
  // trade-off this implies (fine here: public, non-sensitive data only).
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LOCATION_LAT, 6) +
               "&longitude=" + String(LOCATION_LON, 6) +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min" +
               "&temperature_unit=fahrenheit&forecast_days=7&timezone=auto";

  if (!http.begin(client, url))
  {
    return result;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    return result;
  }

  // Read the full body into a String first - streaming reads off
  // WiFiClientSecure can hit premature EOF (same fix as the current-weather fetch).
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err)
  {
    return result;
  }

  JsonObject daily = doc["daily"];
  if (daily.isNull())
  {
    return result;
  }

  JsonArray times = daily["time"];
  JsonArray codes = daily["weather_code"];
  JsonArray highs = daily["temperature_2m_max"];
  JsonArray lows = daily["temperature_2m_min"];

  size_t n = times.size();
  if (n > FORECAST_DAYS)
  {
    n = FORECAST_DAYS;
  }

  for (size_t i = 0; i < n; i++)
  {
    String dateStr = times[i].as<String>(); // "YYYY-MM-DD"
    int year = dateStr.substring(0, 4).toInt();
    int month = dateStr.substring(5, 7).toInt();
    int day = dateStr.substring(8, 10).toInt();

    result.days[i].dayLabel = dayOfWeekAbbrev(year, month, day);
    result.days[i].conditionShort = shortWeatherCode(codes[i].as<int>());
    result.days[i].highF = highs[i].as<float>();
    result.days[i].lowF = lows[i].as<float>();
  }

  result.count = n;
  result.ok = true;
  return result;
}
