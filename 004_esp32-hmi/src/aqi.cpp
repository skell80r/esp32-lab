#include "aqi.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

// US EPA AQI bands: https://www.airnow.gov/aqi/aqi-basics/
static String usAqiCategory(int aqi)
{
  if (aqi <= 50)
    return "Good";
  if (aqi <= 100)
    return "Moderate";
  if (aqi <= 150)
    return "Unhealthy (Sensitive)";
  if (aqi <= 200)
    return "Unhealthy";
  if (aqi <= 300)
    return "Very Unhealthy";
  return "Hazardous";
}

AqiResult fetchAirQuality()
{
  AqiResult result{};
  result.ok = false;

  // setInsecure() skips TLS certificate validation - see weather.cpp for the
  // trade-off this implies (fine here: public, non-sensitive data only).
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(LOCATION_LAT, 6) +
               "&longitude=" + String(LOCATION_LON, 6) +
               "&current=us_aqi";

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

  // Read the full body into a String rather than parsing http.getStream()
  // directly - streaming reads off WiFiClientSecure can hit premature EOF
  // (same issue found and fixed for the regular weather fetch).
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err)
  {
    return result;
  }

  JsonObject current = doc["current"];
  if (current.isNull())
  {
    return result;
  }

  result.usAqi = current["us_aqi"].as<int>();
  result.category = usAqiCategory(result.usAqi);
  result.ok = true;
  return result;
}
