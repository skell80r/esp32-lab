#include "alerts.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

AlertResult fetchSevereAlerts()
{
  AlertResult result{};
  result.ok = false;
  result.active = false;

  WiFiClientSecure client;
  client.setInsecure(); // see weather.cpp for the trade-off this implies

  HTTPClient http;
  String url = "https://api.weather.gov/alerts/active?point=" + String(LOCATION_LAT, 6) +
               "," + String(LOCATION_LON, 6);

  if (!http.begin(client, url))
  {
    return result;
  }
  // NWS requires a descriptive User-Agent identifying the app and a contact:
  // https://www.weather.gov/documentation/services-web-api
  http.addHeader("User-Agent", NWS_USER_AGENT);
  http.addHeader("Accept", "application/geo+json");

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    return result;
  }

  // Alert polygons can be large; filter down to just the fields we display
  // so parsing doesn't need to hold the full geometry in memory.
  JsonDocument filter;
  filter["features"][0]["properties"]["event"] = true;
  filter["features"][0]["properties"]["headline"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err)
  {
    return result;
  }

  JsonArray features = doc["features"];
  result.ok = true;
  if (features.size() > 0)
  {
    JsonObject props = features[0]["properties"];
    String headline = props["headline"].as<String>();
    result.headline = headline.length() > 0 ? headline : props["event"].as<String>();
    result.active = result.headline.length() > 0;
  }

  return result;
}
