#include "display.h"

// Panel/pin/color config lives in platformio.ini's build_flags (ST7796_DRIVER,
// TFT_*, TFT_INVERSION_ON, SPI_FREQUENCY) rather than here - see TFT_eSPI's
// own docs for what each of those controls.
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <string.h>

#include "tiger_jpg.h"

static TFT_eSPI tft = TFT_eSPI();

// Allen-Bradley / PanelView-style palette: dark steel chrome, sunken faceplates,
// amber LED-style numerics. Semantic alarm colors (red=active, green=clear)
// double as the ISA-101 status convention this style is built around.
static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static constexpr uint16_t THEME_BG = rgb565(14, 18, 22);         // overall background, near-black steel
static constexpr uint16_t THEME_PANEL_BG = rgb565(26, 32, 38);   // faceplate interior
static constexpr uint16_t THEME_BEZEL_LO = rgb565(4, 5, 6);      // sunken bevel: dark edge (top/left)
static constexpr uint16_t THEME_BEZEL_HI = rgb565(84, 94, 104);  // sunken bevel: light edge (bottom/right)
static constexpr uint16_t THEME_HEADER_BG = rgb565(32, 52, 76);  // steel blue title bar
static constexpr uint16_t THEME_ACCENT = rgb565(255, 176, 0);    // amber LED
static constexpr uint16_t THEME_LABEL = rgb565(150, 168, 184);   // section label blue-grey
static constexpr uint16_t THEME_TEXT = rgb565(225, 230, 235);    // body text
static constexpr uint16_t THEME_DIM = rgb565(100, 110, 120);     // captions
static constexpr uint16_t THEME_OK = rgb565(40, 165, 70);        // pilot-light green
static constexpr uint16_t THEME_ALARM = rgb565(205, 30, 30);     // pilot-light red
static constexpr uint16_t THEME_COOL = rgb565(60, 140, 220);     // pilot-light blue (call-for-cool)

struct Rect
{
  int x, y, w, h;
};

static Rect headerRect;
static Rect indoorRect;
static Rect outsideRect;
static Rect outsideHitRect; // where taps for Outside actually register - see hitTestOutsidePanel
static Rect alertRect;
static Rect ackButtonRect; // where it's drawn
static Rect ackHitRect;    // where taps for it actually register on this panel - see hitTestAcknowledgeButton
static Rect newsRect;

// Header has two rows: title/net-status/time on top, date below it. Row 1 is
// sized tight to its 8px (size1) text - the title centers against the full
// header height separately, so shrinking this doesn't affect the title.
static const int HEADER_ROW1_H = 16;
static const int HEADER_ROW_GAP = 1;

static bool contains(const Rect &r, int x, int y)
{
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// Sunken (recessed) bevel - the standard look for read-only display faceplates
// in this style, as opposed to a raised bevel for interactive controls.
static void drawSunkenBevel(const Rect &r)
{
  tft.drawFastHLine(r.x, r.y, r.w, THEME_BEZEL_LO);
  tft.drawFastVLine(r.x, r.y, r.h, THEME_BEZEL_LO);
  tft.drawFastHLine(r.x, r.y + r.h - 1, r.w, THEME_BEZEL_HI);
  tft.drawFastVLine(r.x + r.w - 1, r.y, r.h, THEME_BEZEL_HI);
}

// Raised (embossed) bevel - signals a tappable control, mirroring the sunken
// faceplate look used for read-only displays.
static void drawRaisedBevel(const Rect &r)
{
  tft.drawFastHLine(r.x, r.y, r.w, THEME_BEZEL_HI);
  tft.drawFastVLine(r.x, r.y, r.h, THEME_BEZEL_HI);
  tft.drawFastHLine(r.x, r.y + r.h - 1, r.w, THEME_BEZEL_LO);
  tft.drawFastVLine(r.x + r.w - 1, r.y, r.h, THEME_BEZEL_LO);
}

static void drawPilotLight(int cx, int cy, bool on, uint16_t onColor)
{
  tft.fillCircle(cx, cy, 5, on ? onColor : THEME_PANEL_BG);
  tft.drawCircle(cx, cy, 5, THEME_BEZEL_LO);
}

// Fills a panel's interior, draws its sunken bevel, and prints the uppercase
// section label - the same three steps every faceplate below starts with.
static void beginFaceplate(const Rect &r, const char *label)
{
  tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, THEME_PANEL_BG);
  drawSunkenBevel(r);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(THEME_LABEL, THEME_PANEL_BG);
  tft.setCursor(r.x + 8, r.y + 6);
  tft.print(label);
  tft.drawFastHLine(r.x + 6, r.y + 16, r.w - 12, THEME_BEZEL_LO);
}

static void drawStaticLayout()
{
  int w = tft.width();
  int h = tft.height();

  headerRect = {0, 0, w, 34};
  indoorRect = {0, 34, w / 2, 110};
  outsideRect = {w / 2, 34, w - w / 2, 110};
  // Taps on the visible panel consistently register ~230px left and ~180px
  // down from where it's drawn on this panel (confirmed via live coordinate
  // logging) - same class of quirk as the ACK button, see below.
  outsideHitRect = {50, 180, 100, 90};
  alertRect = {0, 144, w, 60};
  ackButtonRect = {alertRect.x + alertRect.w - 130, alertRect.y + 18, 120, 38};
  // Taps on the visible button consistently register ~165px left and ~98px
  // down from where it's drawn on this panel (confirmed via live coordinate
  // logging) - widened and offset to match reality rather than the drawing.
  // Shifted up 14px along with the header shrinking/alert bar moving up;
  // re-verify with live coordinate logging if ACK stops registering.
  ackHitRect = {180, 236, 180, 80};
  newsRect = {0, 204, w, h - 204};

  tft.fillScreen(THEME_BG);

  tft.fillRect(headerRect.x, headerRect.y, headerRect.w, headerRect.h, THEME_HEADER_BG);
  tft.drawFastHLine(headerRect.x, headerRect.y + headerRect.h - 2, headerRect.w, THEME_ACCENT);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setTextSize(3);
  // Vertically centered in the full header bar (24px glyph height) - the
  // title sits on the left with nothing else sharing its vertical space, so
  // it centers against the whole bar rather than either right-side sub-row.
  tft.setCursor(headerRect.x + 6, headerRect.y + (headerRect.h - 24) / 2);
  tft.print("HOME HMI");
}

void initDisplay()
{
  tft.init();
  tft.setRotation(1);
  // TFT_BL/TFT_BACKLIGHT_ON in build_flags handle turning the backlight on;
  // no PWM dimming here, matching the previous always-max-brightness behavior.
  drawStaticLayout();
}

void redrawDashboardLayout()
{
  drawStaticLayout();
}

bool hitTestIndoorPanel(int touchX, int touchY)
{
  return touchX >= indoorRect.x && touchX < indoorRect.x + indoorRect.w &&
         touchY >= indoorRect.y && touchY < indoorRect.y + indoorRect.h;
}

bool hitTestOutsidePanel(int touchX, int touchY)
{
  return contains(outsideRect, touchX, touchY) || contains(outsideHitRect, touchX, touchY);
}

// Row 1 layout, right to left: TIME, then NET OK/FAIL just to its left.
// Both at text size 1 - fixed widths below assume that.
static const int HEADER_RIGHT_MARGIN = 8;
static const int HEADER_TIME_W = 5 * 6;    // "HH:MM"
static const int HEADER_NET_TEXT_W = 8 * 6; // widest case, "NET FAIL"
static const int HEADER_GROUP_GAP = 12;

static int headerTimeX()
{
  return headerRect.x + headerRect.w - HEADER_RIGHT_MARGIN - HEADER_TIME_W;
}

void showWifiStatus(bool connected)
{
  int netTextX = headerTimeX() - HEADER_GROUP_GAP - HEADER_NET_TEXT_W;
  int pilotCx = netTextX - 12;
  int cy = headerRect.y + HEADER_ROW1_H / 2;

  tft.fillRect(pilotCx - 8, headerRect.y, (netTextX + HEADER_NET_TEXT_W) - (pilotCx - 8), HEADER_ROW1_H, THEME_HEADER_BG);
  drawPilotLight(pilotCx, cy, true, connected ? THEME_OK : THEME_ALARM);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setCursor(netTextX, cy - 4);
  tft.print(connected ? "NET OK" : "NET FAIL");
}

void showDateTime(const String &dateStr, const String &timeStr)
{
  // Time: top-right of row 1.
  int timeX = headerTimeX();
  int row1Cy = headerRect.y + HEADER_ROW1_H / 2;
  tft.fillRect(timeX - 2, headerRect.y, headerRect.x + headerRect.w - (timeX - 2), HEADER_ROW1_H, THEME_HEADER_BG);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(THEME_ACCENT, THEME_HEADER_BG);
  tft.setCursor(timeX, row1Cy - 4);
  tft.print(timeStr);

  // Date: row 2, right-aligned under the time. Only clears its own area on
  // the right - the title (centered against the full header height) extends
  // down into this row's Y-range on the left side, and a full-width clear
  // here would erase the bottom of its glyphs on every date update.
  int rowY = headerRect.y + HEADER_ROW1_H + HEADER_ROW_GAP;
  int rowH = headerRect.h - HEADER_ROW1_H - HEADER_ROW_GAP - 2; // leaves the bottom 2px accent stripe alone
  int dateW = dateStr.length() * 6;
  int clearW = 140; // generous margin around the widest expected date string
  tft.fillRect(headerRect.x + headerRect.w - clearW, rowY, clearW, rowH, THEME_HEADER_BG);
  tft.setTextColor(THEME_ACCENT, THEME_HEADER_BG);
  tft.setCursor(headerRect.x + headerRect.w - HEADER_RIGHT_MARGIN - dateW, rowY + (rowH - 8) / 2);
  tft.print(dateStr);
}

// Big amber 7-segment readout (Font 7 has digits, '.', '-' and ':' only - the
// unit suffix is printed separately in the normal font right after it).
static void drawSegmentValue(int x, int y, float value, const char *unit)
{
  tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
  tft.setTextFont(7);
  tft.setCursor(x, y);
  tft.print(value, 1);
  int afterX = tft.getCursorX();
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
  tft.setCursor(afterX + 4, y + 26);
  tft.print(unit);
}

void showIndoor(float tempF, float pressureMbar, float gasKOhm, const char *airQualityLabel)
{
  beginFaceplate(indoorRect, "INDOOR");

  drawSegmentValue(indoorRect.x + 8, indoorRect.y + 20, tempF, "F");

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(THEME_TEXT, THEME_PANEL_BG);
  tft.setCursor(indoorRect.x + 8, indoorRect.y + 70);
  tft.print("PRESSURE ");
  tft.print(pressureMbar, 1);
  tft.print(" MBAR");

  tft.setCursor(indoorRect.x + 8, indoorRect.y + 84);
  tft.print("AIR QUALITY: ");
  tft.print(airQualityLabel);
  tft.print(" (");
  tft.print(gasKOhm, 0);
  tft.print(" KOHM)");

  tft.setCursor(indoorRect.x + 8, indoorRect.y + 98);
  tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
  tft.print("UNCALIBRATED VOC PROXY");
}

// US EPA AQI severity color, matching this theme's existing status convention.
static uint16_t aqiColor(int usAqi)
{
  if (usAqi <= 50)
    return THEME_OK;
  if (usAqi <= 100)
    return THEME_ACCENT;
  return THEME_ALARM;
}

void showOutside(bool ok, float tempF, const String &condition, unsigned long updatedAgoSeconds, const AqiResult &aqi)
{
  beginFaceplate(outsideRect, "OUTSIDE");

  if (!ok)
  {
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
    tft.setCursor(outsideRect.x + 8, outsideRect.y + 40);
    tft.print("NO DATA YET");
    return;
  }

  drawSegmentValue(outsideRect.x + 8, outsideRect.y + 20, tempF, "F");

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(THEME_TEXT, THEME_PANEL_BG);
  tft.setCursor(outsideRect.x + 8, outsideRect.y + 70);
  String upperCondition = condition;
  upperCondition.toUpperCase();
  tft.print(upperCondition);

  tft.setCursor(outsideRect.x + 8, outsideRect.y + 84);
  if (aqi.ok)
  {
    tft.setTextColor(aqiColor(aqi.usAqi), THEME_PANEL_BG);
    tft.print("AQI ");
    tft.print(aqi.usAqi);
    tft.print(" ");
    String upperCategory = aqi.category;
    upperCategory.toUpperCase();
    tft.print(upperCategory);
  }
  else
  {
    tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
    tft.print("AQI: NO DATA YET");
  }

  tft.setCursor(outsideRect.x + 8, outsideRect.y + 98);
  tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
  tft.print("UPDATED ");
  tft.print(updatedAgoSeconds / 60);
  tft.print("M AGO");
}

void showAlert(AlarmDisplayState state, const String &text)
{
  uint16_t bg, fg;
  bool pilotLit = state != AlarmDisplayState::NoData;
  switch (state)
  {
  case AlarmDisplayState::Clear:
    bg = THEME_OK;
    fg = TFT_BLACK;
    break;
  case AlarmDisplayState::Active:
    bg = THEME_ALARM;
    fg = TFT_WHITE;
    break;
  case AlarmDisplayState::Acknowledged:
    bg = THEME_ACCENT;
    fg = TFT_BLACK;
    break;
  case AlarmDisplayState::NoData:
  default:
    bg = THEME_PANEL_BG;
    fg = THEME_DIM;
    break;
  }

  tft.fillRect(alertRect.x + 1, alertRect.y + 1, alertRect.w - 2, alertRect.h - 2, bg);
  drawSunkenBevel(alertRect);

  // Section label, same convention as the other faceplates (small caption +
  // separator), just tinted to match this bar's current alarm color.
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(fg, bg);
  tft.setCursor(alertRect.x + 8, alertRect.y + 4);
  tft.print("ALARMS");
  tft.drawFastHLine(alertRect.x + 6, alertRect.y + 16, alertRect.w - 12, THEME_BEZEL_LO);

  int contentY = alertRect.y + 18;
  int contentH = alertRect.h - 20;
  drawPilotLight(alertRect.x + 18, contentY + contentH / 2, pilotLit, bg);

  bool showAck = state == AlarmDisplayState::Active;
  int textRightEdge = showAck ? ackButtonRect.x - 8 : alertRect.x + alertRect.w - 8;
  int maxChars = (textRightEdge - (alertRect.x + 34)) / 12; // ~12px/char at text size 2

  String line;
  switch (state)
  {
  case AlarmDisplayState::NoData:
    line = "NO DATA YET";
    break;
  case AlarmDisplayState::Clear:
    line = "NO ACTIVE ALARMS";
    break;
  case AlarmDisplayState::Active:
  case AlarmDisplayState::Acknowledged:
    line = text;
    line.toUpperCase();
    if (line.length() > static_cast<size_t>(maxChars))
    {
      line = line.substring(0, maxChars - 3) + "...";
    }
    break;
  }

  tft.setTextSize(2);
  tft.setTextColor(fg, bg);
  tft.setCursor(alertRect.x + 34, contentY + (contentH - 16) / 2);
  tft.print(line);

  if (showAck)
  {
    tft.fillRect(ackButtonRect.x + 1, ackButtonRect.y + 1, ackButtonRect.w - 2, ackButtonRect.h - 2, THEME_PANEL_BG);
    drawRaisedBevel(ackButtonRect);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
    tft.setCursor(ackButtonRect.x + 27, ackButtonRect.y + (ackButtonRect.h - 8) / 2);
    tft.print("ACKNOWLEDGE");
  }
  else
  {
    // Clear any previously-drawn button when it's no longer relevant.
    tft.fillRect(ackButtonRect.x, ackButtonRect.y, ackButtonRect.w, ackButtonRect.h, bg);
  }
}

bool hitTestAcknowledgeButton(int touchX, int touchY)
{
  return contains(ackButtonRect, touchX, touchY) || contains(ackHitRect, touchX, touchY);
}

void showNews(bool ok, const String *headlines, size_t count)
{
  beginFaceplate(newsRect, "NEWS (NYT)");

  if (!ok || count == 0)
  {
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
    tft.setCursor(newsRect.x + 8, newsRect.y + 26);
    tft.print("NO DATA YET");
    return;
  }

  // Default font is ~6px wide at text size 1; truncate so a long headline
  // can't run off the edge of the panel (no wrapping/scrolling implemented).
  int maxChars = (newsRect.w - 16) / 6;

  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
  for (size_t i = 0; i < count; i++)
  {
    tft.setCursor(newsRect.x + 8, newsRect.y + 26 + static_cast<int>(i) * 22);
    String line = headlines[i];
    if (line.length() > static_cast<size_t>(maxChars))
    {
      line = line.substring(0, maxChars - 3) + "...";
    }
    tft.print(line);
  }
}

// --- Thermostat page ---------------------------------------------------

static Rect thermoBackRect;
static Rect thermoModeRects[4];
static Rect thermoHeatRect, thermoCoolRect;
static Rect thermoHeatMinusRect, thermoHeatPlusRect;
static Rect thermoCoolMinusRect, thermoCoolPlusRect;
static Rect thermoHeatNumberRect, thermoCoolNumberRect;
static Rect thermoStatusRect;

static void layoutThermostatPage()
{
  int w = tft.width();
  int h = tft.height();

  // Deliberately not in the top-right corner - that spot has proven to
  // misread touches on this panel (see touch.cpp history). Bottom-right of
  // the status faceplate has tested reliably instead.
  thermoBackRect = {w - 110, h - 60, 100, 50};
  for (int i = 0; i < 4; i++)
  {
    thermoModeRects[i] = {i * (w / 4), 28, w / 4, 40};
  }
  thermoHeatRect = {0, 68, w / 2, 152};
  thermoCoolRect = {w / 2, 68, w - w / 2, 152};
  thermoHeatMinusRect = {thermoHeatRect.x + 4, thermoHeatRect.y + 20, 50, 120};
  thermoHeatPlusRect = {thermoHeatRect.x + thermoHeatRect.w - 54, thermoHeatRect.y + 20, 50, 120};
  thermoCoolMinusRect = {thermoCoolRect.x + 4, thermoCoolRect.y + 20, 50, 120};
  thermoCoolPlusRect = {thermoCoolRect.x + thermoCoolRect.w - 54, thermoCoolRect.y + 20, 50, 120};
  // The space between each pair of +/- buttons, where the Font 7 number sits -
  // redrawing just this (not the whole faceplate) avoids a full-page flash
  // every time the value changes.
  thermoHeatNumberRect = {thermoHeatMinusRect.x + thermoHeatMinusRect.w, thermoHeatRect.y + 18,
                          thermoHeatPlusRect.x - (thermoHeatMinusRect.x + thermoHeatMinusRect.w), thermoHeatRect.h - 22};
  thermoCoolNumberRect = {thermoCoolMinusRect.x + thermoCoolMinusRect.w, thermoCoolRect.y + 18,
                          thermoCoolPlusRect.x - (thermoCoolMinusRect.x + thermoCoolMinusRect.w), thermoCoolRect.h - 22};
  thermoStatusRect = {0, 220, w, h - 220};
}

static void drawModeButtons(ThermoMode mode)
{
  static const char *modeLabels[4] = {"OFF", "HEAT", "COOL", "AUTO"};
  ThermoMode modes[4] = {ThermoMode::Off, ThermoMode::Heat, ThermoMode::Cool, ThermoMode::Auto};
  for (int i = 0; i < 4; i++)
  {
    bool active = mode == modes[i];
    uint16_t bg = active ? THEME_ACCENT : THEME_PANEL_BG;
    const Rect &r = thermoModeRects[i];
    tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, bg);
    drawRaisedBevel(r);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.setTextColor(active ? TFT_BLACK : THEME_TEXT, bg);
    int textW = strlen(modeLabels[i]) * 12; // 6px/char at size 2
    tft.setCursor(r.x + (r.w - textW) / 2, r.y + (r.h - 16) / 2);
    tft.print(modeLabels[i]);
  }
}

static void drawPlusMinusButton(const Rect &r, const char *label)
{
  tft.fillRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, THEME_PANEL_BG);
  drawRaisedBevel(r);
  tft.setTextFont(1);
  tft.setTextSize(3);
  tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
  tft.setCursor(r.x + (r.w - 18) / 2, r.y + (r.h - 24) / 2);
  tft.print(label);
}

// Redraws just the numeric readout - safe to call on every value change
// without touching the surrounding faceplate chrome.
static void drawSetpointNumber(const Rect &numberRect, const Rect &r, float value)
{
  tft.fillRect(numberRect.x, numberRect.y, numberRect.w, numberRect.h, THEME_PANEL_BG);
  tft.setTextColor(THEME_ACCENT, THEME_PANEL_BG);
  tft.setTextFont(7);
  tft.setTextSize(1); // Font 7 is already ~48px tall natively - do not scale further
  int numW = 3 * 32 + 20; // rough width of "NN.N" at Font 7 to help center it
  tft.setCursor(r.x + (r.w - numW) / 2, r.y + 50);
  tft.print(value, 1);
}

static void drawSetpointFaceplate(const Rect &r, const Rect &numberRect, const char *label, float value,
                                   const Rect &minusRect, const Rect &plusRect)
{
  beginFaceplate(r, label);
  drawPlusMinusButton(minusRect, "-");
  drawPlusMinusButton(plusRect, "+");
  drawSetpointNumber(numberRect, r, value);
}

static void drawStatusArea(CallState callState, float indoorTempF)
{
  uint16_t statusColor = callState == CallState::Heating ? THEME_ALARM : callState == CallState::Cooling ? THEME_COOL
                                                                                                          : THEME_OK;
  const char *statusText = callState == CallState::Heating ? "HEATING" : callState == CallState::Cooling ? "COOLING"
                                                                                                           : "IDLE";
  tft.fillRect(thermoStatusRect.x + 1, thermoStatusRect.y + 1, thermoStatusRect.w - 2, thermoStatusRect.h - 2, THEME_PANEL_BG);
  drawSunkenBevel(thermoStatusRect);
  drawPilotLight(thermoStatusRect.x + 24, thermoStatusRect.y + thermoStatusRect.h / 2, true, statusColor);

  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(statusColor, THEME_PANEL_BG);
  tft.setCursor(thermoStatusRect.x + 48, thermoStatusRect.y + 20);
  tft.print(statusText);

  tft.setTextSize(1);
  tft.setTextColor(THEME_TEXT, THEME_PANEL_BG);
  tft.setCursor(thermoStatusRect.x + 48, thermoStatusRect.y + 48);
  tft.print("INDOOR ");
  tft.print(indoorTempF, 1);
  tft.print(" F");

  tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
  tft.setCursor(thermoStatusRect.x + 48, thermoStatusRect.y + 62);
  tft.print("RELAY NOT YET WIRED - DISPLAY ONLY");

  tft.fillRect(thermoBackRect.x + 1, thermoBackRect.y + 1, thermoBackRect.w - 2, thermoBackRect.h - 2, THEME_HEADER_BG);
  drawRaisedBevel(thermoBackRect);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setCursor(thermoBackRect.x + 12, thermoBackRect.y + 16);
  tft.print("< BACK");
}

// Full draw - only call when first entering the page. Everything else
// (value changes, periodic refresh) should use updateThermostatValues()
// instead, or this repaints the whole screen and causes a visible flash.
void showThermostatPage(const ThermostatState &state, CallState callState, float indoorTempF)
{
  layoutThermostatPage();

  tft.fillScreen(THEME_BG);

  tft.fillRect(0, 0, tft.width(), 28, THEME_HEADER_BG);
  tft.drawFastHLine(0, 26, tft.width(), THEME_ACCENT);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setCursor(6, 5);
  tft.print("THERMOSTAT");

  drawModeButtons(state.mode);
  drawSetpointFaceplate(thermoHeatRect, thermoHeatNumberRect, "HEAT SETPOINT", state.heatSetpointF, thermoHeatMinusRect, thermoHeatPlusRect);
  drawSetpointFaceplate(thermoCoolRect, thermoCoolNumberRect, "COOL SETPOINT", state.coolSetpointF, thermoCoolMinusRect, thermoCoolPlusRect);
  drawStatusArea(callState, indoorTempF);
}

// Lightweight refresh for value changes (button presses, periodic indoor
// updates) - touches only the parts that can actually change, not the
// static chrome (header, bevels, labels, +/- buttons, back button).
void updateThermostatValues(const ThermostatState &state, CallState callState, float indoorTempF)
{
  drawModeButtons(state.mode);
  drawSetpointNumber(thermoHeatNumberRect, thermoHeatRect, state.heatSetpointF);
  drawSetpointNumber(thermoCoolNumberRect, thermoCoolRect, state.coolSetpointF);
  drawStatusArea(callState, indoorTempF);
}

ThermoButton hitTestThermostatPage(int touchX, int touchY)
{
  if (contains(thermoBackRect, touchX, touchY))
  {
    return ThermoButton::Back;
  }
  if (contains(thermoModeRects[0], touchX, touchY))
  {
    return ThermoButton::ModeOff;
  }
  if (contains(thermoModeRects[1], touchX, touchY))
  {
    return ThermoButton::ModeHeat;
  }
  if (contains(thermoModeRects[2], touchX, touchY))
  {
    return ThermoButton::ModeCool;
  }
  if (contains(thermoModeRects[3], touchX, touchY))
  {
    return ThermoButton::ModeAuto;
  }
  if (contains(thermoHeatMinusRect, touchX, touchY))
  {
    return ThermoButton::HeatDown;
  }
  if (contains(thermoHeatPlusRect, touchX, touchY))
  {
    return ThermoButton::HeatUp;
  }
  if (contains(thermoCoolMinusRect, touchX, touchY))
  {
    return ThermoButton::CoolDown;
  }
  if (contains(thermoCoolPlusRect, touchX, touchY))
  {
    return ThermoButton::CoolUp;
  }
  return ThermoButton::None;
}

// --- Forecast page -------------------------------------------------------

static Rect forecastBackRect;
static Rect forecastContentRect;

static void layoutForecastPage()
{
  int w = tft.width();
  int h = tft.height();
  forecastContentRect = {0, 32, w, 220};
  // Reuses the same screen position confirmed reliable for the Thermostat
  // page's BACK button - this panel has localized touch-sensing quirks in
  // some regions (see touch.cpp/project notes), so stick to a proven spot.
  forecastBackRect = {w - 110, h - 60, 100, 50};
}

void showForecastPage(const ForecastResult &forecast)
{
  layoutForecastPage();

  tft.fillScreen(THEME_BG);

  tft.fillRect(0, 0, tft.width(), 30, THEME_HEADER_BG);
  tft.drawFastHLine(0, 28, tft.width(), THEME_ACCENT);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setCursor(6, 6);
  tft.print("7-DAY FORECAST - DURHAM, NC");

  tft.fillRect(forecastContentRect.x + 1, forecastContentRect.y + 1, forecastContentRect.w - 2, forecastContentRect.h - 2, THEME_PANEL_BG);
  drawSunkenBevel(forecastContentRect);

  if (!forecast.ok || forecast.count == 0)
  {
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setTextColor(THEME_DIM, THEME_PANEL_BG);
    tft.setCursor(forecastContentRect.x + 10, forecastContentRect.y + 10);
    tft.print("NO DATA YET");
  }
  else
  {
    int colW = forecastContentRect.w / static_cast<int>(FORECAST_DAYS);
    for (size_t i = 0; i < forecast.count; i++)
    {
      int colX = forecastContentRect.x + static_cast<int>(i) * colW;
      if (i > 0)
      {
        tft.drawFastVLine(colX, forecastContentRect.y + 1, forecastContentRect.h - 2, THEME_BEZEL_LO);
      }

      const ForecastDay &day = forecast.days[i];
      int textCenterX = colX + colW / 2;

      tft.setTextFont(1);
      tft.setTextSize(2);
      tft.setTextColor(THEME_LABEL, THEME_PANEL_BG);
      int dayW = day.dayLabel.length() * 12;
      tft.setCursor(textCenterX - dayW / 2, forecastContentRect.y + 14);
      tft.print(day.dayLabel);

      tft.setTextSize(1);
      tft.setTextColor(THEME_TEXT, THEME_PANEL_BG);
      int condW = day.conditionShort.length() * 6;
      tft.setCursor(textCenterX - condW / 2, forecastContentRect.y + 44);
      tft.print(day.conditionShort);

      String highStr = String(day.highF, 0) + "F";
      tft.setTextColor(THEME_ALARM, THEME_PANEL_BG);
      int highW = highStr.length() * 6;
      tft.setCursor(textCenterX - highW / 2, forecastContentRect.y + 70);
      tft.print(highStr);

      String lowStr = String(day.lowF, 0) + "F";
      tft.setTextColor(THEME_COOL, THEME_PANEL_BG);
      int lowW = lowStr.length() * 6;
      tft.setCursor(textCenterX - lowW / 2, forecastContentRect.y + 84);
      tft.print(lowStr);
    }
  }

  tft.fillRect(forecastBackRect.x + 1, forecastBackRect.y + 1, forecastBackRect.w - 2, forecastBackRect.h - 2, THEME_HEADER_BG);
  drawRaisedBevel(forecastBackRect);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, THEME_HEADER_BG);
  tft.setCursor(forecastBackRect.x + 12, forecastBackRect.y + 16);
  tft.print("< BACK");
}

ForecastButton hitTestForecastPage(int touchX, int touchY)
{
  if (contains(forecastBackRect, touchX, touchY))
  {
    return ForecastButton::Back;
  }
  return ForecastButton::None;
}

// --- Screensaver ---------------------------------------------------------

static bool screensaverJpegBlockReady(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void showScreensaver()
{
  static bool decoderInitialized = false;
  if (!decoderInitialized)
  {
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true); // TFT_eSPI expects big-endian RGB565
    TJpgDec.setCallback(screensaverJpegBlockReady);
    decoderInitialized = true;
  }

  tft.fillScreen(TFT_BLACK);
  TJpgDec.drawJpg(0, 0, TIGER_JPG, TIGER_JPG_LEN);
}
