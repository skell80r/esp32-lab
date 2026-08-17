#include "clock.h"

#include <time.h>

void initClock()
{
  // US Eastern with automatic DST (matches the project's configured location
  // in secrets.h - Durham, NC). Adjust this string if the HMI is relocated.
  configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
}

bool getFormattedDateTime(String &dateStr, String &timeStr)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100))
  {
    return false; // NTP hasn't synced yet
  }

  char dateBuf[9]; // "MM/DD/YY\0"
  char timeBuf[6]; // "HH:MM\0" - no seconds
  strftime(dateBuf, sizeof(dateBuf), "%m/%d/%y", &timeinfo);
  strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);

  dateStr = dateBuf;
  timeStr = timeBuf;
  return true;
}
