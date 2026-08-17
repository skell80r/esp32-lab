#pragma once

#include <Arduino.h>

// Call once after WiFi connects - this board has no RTC, so time comes from NTP.
void initClock();

// Fills dateStr ("MM/DD/YY") and timeStr ("HH:MM", 24hr, no seconds) if NTP
// has synced yet; returns false (leaving both untouched) if not.
bool getFormattedDateTime(String &dateStr, String &timeStr);
