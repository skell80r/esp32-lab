#include "net.h"

#include <WiFi.h>

#include "secrets.h"

static const char *wifiStatusName(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID_AVAIL (network not found - check SSID spelling and that it's 2.4GHz; ESP32 cannot join 5GHz-only networks)";
  case WL_CONNECT_FAILED:
    return "CONNECT_FAILED (likely wrong password)";
  case WL_CONNECTION_LOST:
    return "CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

bool connectWiFi(unsigned long timeoutMs)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.print("Connecting to WiFi SSID \"");
  Serial.print(WIFI_SSID);
  Serial.println("\"...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected)
  {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.print("WiFi connect failed, status: ");
    Serial.println(wifiStatusName(WiFi.status()));
  }

  return connected;
}

bool wifiIsConnected()
{
  return WiFi.status() == WL_CONNECTED;
}
