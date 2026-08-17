#include "weather.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

// WMO weather code table (4677), as used by Open-Meteo's `weather_code` field:
// https://open-meteo.com/en/docs
static String weatherCodeToText(int code)
{
  switch (code)
  {
  case 0:
    return "Clear";
  case 1:
  case 2:
    return "Partly cloudy";
  case 3:
    return "Overcast";
  case 45:
  case 48:
    return "Fog";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 56:
  case 57:
    return "Freezing drizzle";
  case 61:
  case 63:
  case 65:
    return "Rain";
  case 66:
  case 67:
    return "Freezing rain";
  case 71:
  case 73:
  case 75:
  case 77:
    return "Snow";
  case 80:
  case 81:
  case 82:
    return "Rain showers";
  case 85:
  case 86:
    return "Snow showers";
  case 95:
  case 96:
  case 99:
    return "Thunderstorm";
  default:
    return "Unknown";
  }
}

WeatherResult fetchOutsideWeather()
{
  WeatherResult result{};
  result.ok = false;

  // setInsecure() skips TLS certificate validation. Acceptable trade-off here
  // since this only reads public, non-sensitive weather data - but it does
  // mean an on-path attacker could tamper with the response undetected.
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LOCATION_LAT, 6) +
               "&longitude=" + String(LOCATION_LON, 6) +
               "&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timezone=auto";

  Serial.print("Weather: GET ");
  Serial.println(url);

  if (!http.begin(client, url))
  {
    Serial.println("Weather: http.begin() failed");
    return result;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.print("Weather: HTTP GET failed, code ");
    Serial.println(httpCode); // negative values are HTTPClient/connection errors, see HTTPClient.h
    http.end();
    return result;
  }

  // Read the full (small) body into a String rather than parsing http.getStream()
  // directly - streaming reads off WiFiClientSecure can hit premature EOF here.
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err)
  {
    Serial.print("Weather: JSON parse failed: ");
    Serial.println(err.c_str());
    return result;
  }

  JsonObject current = doc["current"];
  if (current.isNull())
  {
    Serial.println("Weather: response had no \"current\" object");
    return result;
  }

  result.tempF = current["temperature_2m"].as<float>();
  result.condition = weatherCodeToText(current["weather_code"].as<int>());
  result.ok = true;
  Serial.print("Weather: ok, ");
  Serial.print(result.tempF);
  Serial.print("F, ");
  Serial.println(result.condition);
  return result;
}
