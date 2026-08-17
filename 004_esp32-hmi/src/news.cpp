#include "news.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

NewsResult fetchTopHeadlines()
{
  NewsResult result{};
  result.ok = false;
  result.count = 0;

  WiFiClientSecure client;
  client.setInsecure(); // see weather.cpp for the trade-off this implies

  HTTPClient http;
  String url = "https://api.nytimes.com/svc/topstories/v2/home.json?api-key=" NYT_API_KEY;

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

  // The Top Stories response can be hundreds of KB (full article bodies,
  // media assets, etc). Filter down to just the titles we display so the
  // parser never has to hold the full payload in RAM at once.
  JsonDocument filter;
  filter["results"][0]["title"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err)
  {
    return result;
  }

  JsonArray results = doc["results"];
  for (JsonObject story : results)
  {
    if (result.count >= NEWS_HEADLINE_COUNT)
    {
      break;
    }
    result.headlines[result.count++] = story["title"].as<String>();
  }

  result.ok = true;
  return result;
}
