/*
 * E-Paper Dashboard — turn-key edition
 * ----------------------------------------------------------------------
 * ESP32 + any GxEPD2-supported SPI e-paper panel (pick panel & wiring in
 * config.h). First boot (or BOOT held after reset, or repeated failures)
 * opens a captive-portal setup wizard (Bootstrap UI at http://192.168.4.1
 * on the "EPaper-Dashboard" WiFi). The wizard configures WiFi, IMAP
 * email, CalDAV/ICS calendar, weather, clock and refresh — testing each
 * live — then the device runs the dashboard loop: wake → fetch (IMAP +
 * CalDAV/ICS + Open-Meteo + blocks) → render → deep sleep. Last-good
 * data survives in RTC memory; failures are flagged per section instead
 * of blanking the screen.
 *
 * Libraries (Library Manager): GxEPD2 (+deps), ArduinoJson v7.
 * Board: "ESP32 Dev Module". Partition scheme: "Minimal SPIFFS" (OTA).
 */

#include "config.h"
#include "settings.h"
#include "dashboard_data.h"
#include "imap.h"
#include "caldav.h"
#include "weather.h"
#include "portal.h"
#include "blocks.h"
#include "fsstore.h"

#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>   // esp_reset_reason()

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include "style.h"       // fonts and rule weights of the look in force

// ---------------- display ----------------
// The panel preset in config.h resolves to a GxEPD2 driver class
// (EPD_DRIVER) and a color capability flag (EPD_IS_3C). Rendering is
// paged in two half-screen passes so the framebuffer fits static RAM
// on every supported resolution.
#if !defined(EPD_DRIVER)
#error "Select a panel in config.h"
#endif
#if EPD_IS_3C
GxEPD2_3C<EPD_DRIVER, EPD_DRIVER::HEIGHT / 2>
  display(EPD_DRIVER(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
#else
GxEPD2_BW<EPD_DRIVER, EPD_DRIVER::HEIGHT / 2>
  display(EPD_DRIVER(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
// On black&white panels GxEPD2 draws any non-black color as white, which
// would make the red accents invisible — fold every red to black instead.
// (Zero changes needed at the ~50 call sites: GxEPD_RED is a macro.)
#undef GxEPD_RED
#define GxEPD_RED GxEPD_BLACK
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
SPIClass hspi(FSPI);   // single/renamed SPI host on S2/S3/C3/C6
#else
SPIClass hspi(HSPI);
#endif

// ---------------- cached state (survives deep sleep) ----------------
RTC_DATA_ATTR EventT   g_events[MAXE];
RTC_DATA_ATTR uint8_t  g_nEvents = 0;
RTC_DATA_ATTR EmailT   g_emails[MAXM];
RTC_DATA_ATTR uint8_t  g_nEmails = 0;
RTC_DATA_ATTR WeatherT g_wx;
RTC_DATA_ATTR int16_t  g_unread = -1;
RTC_DATA_ATTR uint8_t  g_haveMail = 0, g_haveCal = 0, g_haveWx = 0;
RTC_DATA_ATTR uint32_t g_mailTs = 0, g_calTs = 0, g_wxTs = 0;   // last success
RTC_DATA_ATTR uint32_t g_bootCount = 0;
RTC_DATA_ATTR uint8_t  g_wifiFails = 0;
RTC_DATA_ATTR char     g_lastIp[16] = "";   // DHCP lease from the router

// Crash-loop guard: g_phase marks where the cycle is (1 = fetching,
// 2 = rendering, 0 = completed). If a boot starts and the previous phase
// never reached 0, that cycle died mid-way — count it and degrade instead
// of crash-looping with a frozen screen.
RTC_DATA_ATTR uint8_t g_phase = 0;
RTC_DATA_ATTR uint8_t g_fetchCrashes = 0;
RTC_DATA_ATTR uint8_t g_renderCrashes = 0;
static bool s_skipCal = false, s_skipAll = false;

// this-cycle status
static bool s_mailOk, s_calOk, s_wxOk, s_wifiOk;
static char s_mailErr[96], s_calErr[96];
static time_t g_now;
static struct tm g_tm;

// Weather icon kinds. MUST stay above the FIRST function definition in this
// file: the Arduino IDE hoists auto-generated prototypes there, and
// iconForCode()'s prototype returns IconKind. (Guarded by the
// tools/arduino_proto_check.py emulation in CI.)
enum IconKind { IC_SUN, IC_PART, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };

// ---------------- layout + contributed blocks ----------------
#define MAX_CONTRIB 6
struct ContribSlot {
  char inst[14];
  int layoutIdx = -1;   // index into the layout array (for params)
  BlockDef def;
  BlockData data;
  bool loaded = false;
};
static JsonDocument s_layoutDoc;         // parsed layout (loaded once per boot)
// Contributed-block slots are ~5 KB each (BlockDef + BlockData), so they are
// heap-allocated once instead of static: the ESP32's dram0 static segment is
// only ~180 KB and the 48 KB display buffer already lives there.
static ContribSlot* s_contrib = nullptr;
static int s_nContrib = 0;

// Load layout + installed contributed defs. Safe to call in any mode.
static void prepareLayout() {
  if (!s_contrib) s_contrib = new ContribSlot[MAX_CONTRIB];
  layoutLoad(s_layoutDoc);
  s_nContrib = 0;
  int idx = -1;
  for (JsonObjectConst it : s_layoutDoc.as<JsonArrayConst>()) {
    idx++;
    const char* id = it["block"] | "";
    if (strncmp(id, "core-", 5) == 0) continue;
    if (s_nContrib >= MAX_CONTRIB) continue;
    ContribSlot& s = s_contrib[s_nContrib];
    strlcpy(s.inst, it["inst"] | "", sizeof(s.inst));
    s.layoutIdx = idx;
    s.loaded = blockLoadDef(id, s.def);
    s.data = BlockData();
    if (!s.loaded)
      Serial.printf("layout: block '%s' missing/corrupt\n", id);
    s_nContrib++;
  }
}

// ---------------- small draw helpers ----------------
// The look in force. Every setFont() asks this instead of naming a face,
// so one setting restyles built-ins and contributed widgets alike.
static const Style& S() { return styleFor(lookFromName(g_set.look)); }
// A label the way the look sets it (Riposte: capitals).
static String labelText(const char* s) {
  String t(s);
  if (S().upper) {
    t.toUpperCase();
  }
  return t;
}
static uint16_t textWidth(const String& s) {
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}
static void printAt(int16_t x, int16_t yBase, const String& s, uint16_t color) {
  display.setTextColor(color);
  display.setCursor(x, yBase);
  display.print(s);
}
static void printRight(int16_t xRight, int16_t yBase, const String& s, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  display.setTextColor(color);
  display.setCursor(xRight - w - x1, yBase);
  display.print(s);
}
static void printCentered(int16_t xc, int16_t yBase, const String& s, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  display.setTextColor(color);
  display.setCursor(xc - w / 2 - x1, yBase);
  display.print(s);
}
static String fitStr(String s, uint16_t maxW) {
  if (textWidth(s) <= maxW) return s;
  while (s.length() > 1 && textWidth(s + "...") > maxW) s.remove(s.length() - 1);
  return s + "...";
}
static void thickHLine(int16_t x, int16_t y, int16_t w, int16_t t, uint16_t color) {
  display.fillRect(x, y, w, t, color);
}
// Tracked print: GFX has no letter-spacing, so a tracked label is printed
// one glyph at a time with S().tracking px added to each advance.
static uint16_t trackedWidth(const String& s) {
  if (!S().tracking || s.length() == 0) {
    return textWidth(s);
  }
  return textWidth(s) + (uint16_t)(S().tracking * (s.length() - 1));
}
static void printTracked(int16_t x, int16_t yBase, const String& s, uint16_t color) {
  if (!S().tracking) {
    printAt(x, yBase, s, color);
    return;
  }
  display.setTextColor(color);
  for (size_t i = 0; i < s.length(); i++) {
    display.setCursor(x, yBase);
    display.print(s[i]);
    x = display.getCursorX() + S().tracking;
  }
}
// A row marker: a disc in classic, a square (radius 0) in Riposte.
static void bullet(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  if (S().square) {
    display.fillRect(cx - r, cy - r, 2 * r, 2 * r, color);
    return;
  }
  display.fillCircle(cx, cy, r, color);
}

// ---------------- weather icons ----------------
static IconKind iconForCode(int code) {
  if (code == 0 || code == 1) return IC_SUN;
  if (code == 2)              return IC_PART;
  if (code == 3)              return IC_CLOUD;
  if (code == 45 || code == 48) return IC_FOG;
  if (code >= 51 && code <= 67) return IC_RAIN;
  if (code >= 71 && code <= 77) return IC_SNOW;
  if (code >= 80 && code <= 82) return IC_RAIN;
  if (code == 85 || code == 86) return IC_SNOW;
  if (code >= 95)               return IC_STORM;
  return IC_CLOUD;
}

static void drawSun(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
  display.fillCircle(cx, cy, r, color);
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4.0f;
    float c = cosf(a), s = sinf(a);
    int16_t x0 = cx + (int16_t)((r + r * 0.35f) * c);
    int16_t y0 = cy + (int16_t)((r + r * 0.35f) * s);
    int16_t x1 = cx + (int16_t)((r + r * 0.85f) * c);
    int16_t y1 = cy + (int16_t)((r + r * 0.85f) * s);
    display.drawLine(x0, y0, x1, y1, color);
    display.drawLine(x0 + 1, y0, x1 + 1, y1, color);
    display.drawLine(x0, y0 + 1, x1, y1 + 1, color);
  }
}
static void drawMoon(int16_t cx, int16_t cy, int16_t r) {
  display.fillCircle(cx, cy, r, GxEPD_BLACK);
  display.fillCircle(cx + r / 2, cy - r / 3, r, GxEPD_WHITE);
}
static void drawCloud(int16_t x, int16_t y, int16_t w, uint16_t color) {
  int16_t h = (int16_t)(w * 0.62f);
  display.fillCircle(x + (int16_t)(w * 0.30f), y + (int16_t)(h * 0.52f), (int16_t)(w * 0.20f), color);
  display.fillCircle(x + (int16_t)(w * 0.58f), y + (int16_t)(h * 0.40f), (int16_t)(w * 0.26f), color);
  display.fillRoundRect(x + (int16_t)(w * 0.10f), y + (int16_t)(h * 0.52f),
                        (int16_t)(w * 0.80f), (int16_t)(h * 0.40f), (int16_t)(w * 0.12f), color);
}
static void thickSeg(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t t, uint16_t color) {
  for (int i = 0; i < t; i++) display.drawLine(x0 + i, y0, x1 + i, y1, color);
}
static void drawWeatherIcon(int16_t x, int16_t y, int16_t s, int code, bool isDay) {
  IconKind k = iconForCode(code);
  int16_t cx = x + s / 2, cy = y + s / 2;
  float u = s / 48.0f;
  switch (k) {
    case IC_SUN:
      if (isDay) drawSun(cx, cy, (int16_t)(s * 0.28f), GxEPD_RED);
      else drawMoon(cx, cy, (int16_t)(s * 0.30f));
      break;
    case IC_PART:
      if (isDay) drawSun(x + (int16_t)(s * 0.64f), y + (int16_t)(s * 0.30f), (int16_t)(s * 0.16f), GxEPD_RED);
      else drawMoon(x + (int16_t)(s * 0.66f), y + (int16_t)(s * 0.28f), (int16_t)(s * 0.16f));
      drawCloud(x, y + (int16_t)(s * 0.22f), (int16_t)(s * 0.78f), GxEPD_BLACK);
      break;
    case IC_CLOUD:
      drawCloud(x + (int16_t)(s * 0.05f), y + (int16_t)(s * 0.12f), (int16_t)(s * 0.90f), GxEPD_BLACK);
      break;
    case IC_FOG:
      drawCloud(x + (int16_t)(s * 0.10f), y, (int16_t)(s * 0.80f), GxEPD_BLACK);
      for (int i = 0; i < 3; i++)
        display.fillRect(x + (int16_t)(s * 0.10f), y + (int16_t)(s * (0.66f + i * 0.12f)),
                         (int16_t)(s * 0.80f), (int16_t)(u * 2), GxEPD_BLACK);
      break;
    case IC_RAIN:
      drawCloud(x + (int16_t)(s * 0.08f), y, (int16_t)(s * 0.84f), GxEPD_BLACK);
      for (int i = 0; i < 4; i++) {
        int16_t x0 = x + (int16_t)(s * (0.22f + i * 0.18f));
        thickSeg(x0, y + (int16_t)(s * 0.66f), x0 - (int16_t)(s * 0.08f),
                 y + (int16_t)(s * 0.92f), (int16_t)(u * 2), GxEPD_BLACK);
      }
      break;
    case IC_SNOW:
      drawCloud(x + (int16_t)(s * 0.08f), y, (int16_t)(s * 0.84f), GxEPD_BLACK);
      for (int i = 0; i < 3; i++) {
        int16_t fx = x + (int16_t)(s * (0.22f + i * 0.26f));
        int16_t fy = y + (int16_t)(s * 0.82f);
        int16_t r = (int16_t)(u * 3.2f);
        display.drawLine(fx - r, fy, fx + r, fy, GxEPD_BLACK);
        display.drawLine(fx, fy - r, fx, fy + r, GxEPD_BLACK);
        display.drawLine(fx - r + 1, fy - r + 1, fx + r - 1, fy + r - 1, GxEPD_BLACK);
        display.drawLine(fx - r + 1, fy + r - 1, fx + r - 1, fy - r + 1, GxEPD_BLACK);
      }
      break;
    case IC_STORM:
      drawCloud(x + (int16_t)(s * 0.08f), y, (int16_t)(s * 0.84f), GxEPD_BLACK);
      thickSeg(cx + (int16_t)(3 * u), y + (int16_t)(s * 0.60f),
               cx - (int16_t)(4 * u), y + (int16_t)(s * 0.78f), (int16_t)(u * 3), GxEPD_RED);
      thickSeg(cx - (int16_t)(4 * u) + (int16_t)(u * 3), y + (int16_t)(s * 0.78f),
               cx + (int16_t)(2 * u), y + (int16_t)(s * 0.74f), (int16_t)(u * 3), GxEPD_RED);
      thickSeg(cx + (int16_t)(2 * u), y + (int16_t)(s * 0.74f),
               cx - (int16_t)(2 * u), y + (int16_t)(s * 0.96f), (int16_t)(u * 3), GxEPD_RED);
      break;
  }
}
static void drawDegree(int16_t x, int16_t y, int16_t r, uint16_t color) {
  display.drawCircle(x, y, r, color);
  display.drawCircle(x, y, r - 1, color);
}

// section header with red accent; adds a red "!" if that source is stale
// Title plus rule. Classic underlines the word in red; Riposte rules the
// whole column (w) at the look's weight. ruleColor is the accent the caller
// wants under the title (built-ins: red; contributed: red only if accented).
// fitStr for a tracked label: a 16-char title in 9 pt mono is 186 px, a
// 4-column block 176, and GFX wraps the overflow onto a second line.
static String fitTracked(String s, uint16_t maxW) {
  if (trackedWidth(s) <= maxW) return s;
  while (s.length() > 1 && trackedWidth(s + "...") > maxW) s.remove(s.length() - 1);
  return s + "...";
}
static void sectionHeader(int16_t x, int16_t yBase, int16_t w, const char* label,
                          uint16_t ruleColor, bool stale) {
  display.setFont(S().label);
  String text = fitTracked(labelText(label), w - (stale ? 20 : 0));
  printTracked(x, yBase, text, GxEPD_BLACK);
  uint16_t tw = trackedWidth(text);
  thickHLine(x, yBase + 8, S().fullRule ? w : (int16_t)tw, S().rule, ruleColor);
  if (stale) printAt(x + tw + 10, yBase, "!", GxEPD_RED);
}

static String shortAge(uint32_t ts) {
  if (!ts) return "";
  long mins = ((long)g_now - (long)ts) / 60;
  if (mins < 0) mins = 0;
  if (mins < 60) return String(mins) + "m";
  if (mins < 1440) return String(mins / 60) + "h";
  struct tm t;
  time_t tt = (time_t)ts;
  localtime_r(&tt, &t);
  return String(t.tm_mon + 1) + "/" + String(t.tm_mday);
}

static String fmtClock(const struct tm& t, bool withAmPm) {
  char buf[12];
  if (g_set.h24) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  else {
    int h = t.tm_hour % 12;
    if (h == 0) h = 12;
    if (withAmPm) snprintf(buf, sizeof(buf), "%d:%02d %s", h, t.tm_min, t.tm_hour < 12 ? "AM" : "PM");
    else snprintf(buf, sizeof(buf), "%d:%02d", h, t.tm_min);
  }
  return String(buf);
}

// ---------------- dashboard sections (grid blocks) ----------------
// Every renderer draws into its own rect (x, y, w, h in pixels).

static void drawClockBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  String timeStr = fmtClock(g_tm, false);
  // Adaptive size: the big DejaVu digits are sized for ~350 px of width
  // (7 grid columns on 800x480). On smaller panels or narrower blocks,
  // step down so the time never spills out of its block.
  int16_t x1, y1; uint16_t tw, th;
  display.setFont(S().clock);
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
  int16_t amRoom = g_set.h24 ? 0 : 64;
  // Height counts too: an 84 px digit in a 2-row (80 px) block drew over
  // whatever sat above it, since only the width was ever checked.
  const int16_t vPad = 20;
  if ((int16_t)tw + 20 + amRoom > w || (int16_t)th + vPad > h) {
    display.setFont(S().big);          // mid-size digits
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
    if ((int16_t)tw + 20 + amRoom > w || (int16_t)th + vPad > h) {
      display.setFont(S().display);  // last resort, always fits
      display.getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
    }
  }
  int16_t tyBase = y + h - 14;
  printAt(x + 20, tyBase, timeStr, GxEPD_BLACK);
  if (!g_set.h24) {
    display.setFont(S().labelL);
    printAt(x + 20 + tw + x1 + 12, tyBase, g_tm.tm_hour < 12 ? "AM" : "PM", GxEPD_RED);
  }
  thickHLine(x, y + h - S().rule, w, S().rule, GxEPD_BLACK);
}

static void drawDateStatusBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  char buf[40];
  strftime(buf, sizeof(buf), S().upper ? "%a %d %b" : "%A, %B %e", &g_tm);
  String dateStr = labelText(buf);
  dateStr.replace("  ", " ");
  display.setFont(S().display);
  printRight(x + w - 20, y + (int16_t)(h * 0.47f), dateStr, GxEPD_BLACK);

  display.setFont(S().body);
  String sub = "";
  if (g_haveMail && g_unread > 0)
    sub += String(g_unread > 99 ? String("99+") : String(g_unread)) + " unread   ";
  sub += "updated " + fmtClock(g_tm, true);
  bool anyStale = (!s_mailOk && g_set.imUser[0]) ||
                  (!s_calOk && strcmp(g_set.calMode, "none") != 0) || !s_wxOk;
  printRight(x + w - 20, y + (int16_t)(h * 0.75f), sub, anyStale ? GxEPD_RED : GxEPD_BLACK);

  // tiny health/status line
  display.setFont(nullptr);
  String line = "";
  if (g_set.imUser[0]) line += String("mail ") + (s_mailOk ? "ok" : "FAIL") + " ";
  if (strcmp(g_set.calMode, "none") != 0) line += String("cal ") + (s_calOk ? "ok" : "FAIL") + " ";
  line += String("wx ") + (s_wxOk ? "ok" : "FAIL");
  if (!s_wifiOk) line = "OFFLINE - showing last data";
  else if (g_lastIp[0]) line += String("  ") + g_lastIp + "  #" + String(g_bootCount);
  int16_t lw = line.length() * 6;
  display.setTextColor((!s_wifiOk || line.indexOf("FAIL") >= 0) ? GxEPD_RED : GxEPD_BLACK);
  display.setCursor(x + w - 20 - lw, y + h - 14);
  display.print(line);

  thickHLine(x, y + h - S().rule, w, S().rule, GxEPD_BLACK);
}

static void drawWeatherNowBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  int16_t x0 = x + 20;
  if (!g_haveWx) {
    display.setFont(S().bodyL);
    printAt(x0, y + 60, s_wxOk ? "Weather..." : "Weather unavailable", GxEPD_BLACK);
    return;
  }
  int16_t icon = 84;
  if (icon > h - 40) icon = h - 40;
  drawWeatherIcon(x0, y + 24, icon, g_wx.code, g_wx.isDay);

  char tbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%d", g_wx.temp);
  display.setFont(S().big);
  int16_t x1, y1; uint16_t tw, th;
  display.getTextBounds(tbuf, 0, 0, &x1, &y1, &tw, &th);
  int16_t tempX = x0 + icon + 24, tempBase = y + 92;
  printAt(tempX, tempBase, tbuf, GxEPD_BLACK);
  drawDegree(tempX + tw + x1 + 12, tempBase - 58, 6, GxEPD_RED);

  display.setFont(S().labelL);
  printAt(x0, y + 148, fitStr(g_wx.cond, w - 40), GxEPD_BLACK);

  if (h >= 190) {
    char stats[56];
    // A mono face is wider: the Riposte look drops the word "Wind" and the
    // triple spaces so the line still fits a 6-column block.
    if (S().upper) {
      snprintf(stats, sizeof(stats), "Feels %d  RH %d%%  %d %s",
               g_wx.feels, g_wx.hum, g_wx.wind, strcmp(g_set.unitW, "kmh") == 0 ? "km/h" : "mph");
    } else {
      snprintf(stats, sizeof(stats), "Feels %d   RH %d%%   Wind %d %s",
               g_wx.feels, g_wx.hum, g_wx.wind, strcmp(g_set.unitW, "kmh") == 0 ? "km/h" : "mph");
    }
    display.setFont(S().body);
    printAt(x0, y + 176, fitStr(stats, w - 40), GxEPD_BLACK);
  }
}

static void drawForecastBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (!g_haveWx) return;
  thickHLine(x + 16, y + 2, w - 32, 1, GxEPD_BLACK);
  int16_t colW = (w - 24) / 3;
  for (int i = 0; i < 3; i++) {
    int16_t cx = x + 12 + colW / 2 + i * colW;
    display.setFont(S().label);
    printCentered(cx, y + 26, g_wx.d[i].dow, (i == 0) ? GxEPD_RED : GxEPD_BLACK);
    drawWeatherIcon(cx - 21, y + 36, 42, g_wx.d[i].code, true);
    char hl[16];
    snprintf(hl, sizeof(hl), "%d / %d", g_wx.d[i].hi, g_wx.d[i].lo);
    display.setFont(S().body);
    printCentered(cx, y + 102, hl, GxEPD_BLACK);
    if (g_wx.d[i].pop >= 30 && h >= 130) {
      char pp[8];
      snprintf(pp, sizeof(pp), "%d%%", g_wx.d[i].pop);
      printCentered(cx, y + 124, pp, GxEPD_RED);
    }
  }
}

static void drawCalendarBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  int16_t rx = x + 28;
  bool enabled = strcmp(g_set.calMode, "none") != 0;
  sectionHeader(rx, y + 28, x + w - 20 - rx, "CALENDAR", GxEPD_RED,
                enabled && !s_calOk && g_haveCal);

  display.setFont(S().bodyL);
  if (!enabled) {
    printAt(rx, y + 78, "Calendar not configured", GxEPD_BLACK);
    return;
  }
  if (!g_haveCal) {
    printAt(rx, y + 78, s_calOk ? "No events" : "Calendar unavailable", GxEPD_BLACK);
    if (!s_calOk && s_calErr[0]) {
      display.setFont(S().body);
      printAt(rx, y + 106, fitStr(s_calErr, w - 52), GxEPD_RED);
    }
    return;
  }
  if (g_nEvents == 0) {
    printAt(rx, y + 78, "No events today or tomorrow", GxEPD_BLACK);
    return;
  }
  int16_t yy = y + 62;
  bool tomorrowHdr = false;
  int16_t timeColW = 88;
  for (int i = 0; i < g_nEvents; i++) {
    EventT& e = g_events[i];
    if (yy > y + h - 14) break;
    if (e.day == 1 && !tomorrowHdr) {
      display.setFont(S().label);
      printTracked(rx, yy, "TOMORROW", GxEPD_BLACK);
      // Classic keeps its short 66 px accent; Riposte rules the whole word.
      thickHLine(rx, yy + 8, S().fullRule ? (int16_t)trackedWidth("TOMORROW") : 66, 2, GxEPD_RED);
      yy += 30;
      tomorrowHdr = true;
      if (yy > y + h - 14) break;
    }
    bool nowEv = !e.allDay && e.day == 0 &&
                 (uint32_t)g_now >= e.ts0 && (uint32_t)g_now < e.ts1;
    if (nowEv) display.fillRect(rx - 14, yy - 18, 5, 24, GxEPD_RED);
    display.setFont(S().label);
    printRight(rx + timeColW, yy, e.when, nowEv ? GxEPD_RED : GxEPD_BLACK);
    display.setFont(S().bodyL);
    printAt(rx + timeColW + 14, yy, fitStr(e.title, x + w - 20 - (rx + timeColW + 14)),
            nowEv ? GxEPD_RED : GxEPD_BLACK);
    yy += 34;
  }
}

static void drawInboxBlock(int16_t x, int16_t y, int16_t w, int16_t h) {
  int16_t rx = x + 28;
  bool enabled = g_set.imUser[0] != 0;
  thickHLine(x + 4, y, w - 20, 1, GxEPD_BLACK);
  sectionHeader(rx, y + 26, x + w - 16 - rx, "INBOX", GxEPD_RED,
                enabled && !s_mailOk && g_haveMail);
  if (enabled && g_haveMail && g_unread > 0) {
    char ub[20];
    snprintf(ub, sizeof(ub), "%s unread", g_unread > 99 ? "99+" : String(g_unread).c_str());
    display.setFont(S().body);
    printRight(x + w - 16, y + 26, ub, GxEPD_RED);
  }
  display.setFont(S().bodyL);
  if (!enabled) {
    printAt(rx, y + 70, "Email not configured", GxEPD_BLACK);
    return;
  }
  if (!g_haveMail) {
    printAt(rx, y + 70, s_mailOk ? "Checking..." : "Email unavailable", GxEPD_BLACK);
    if (!s_mailOk && s_mailErr[0]) {
      display.setFont(S().body);
      printAt(rx, y + 96, fitStr(s_mailErr, w - 52), GxEPD_RED);
    }
    return;
  }
  if (g_nEmails == 0) {
    printAt(rx, y + 70, "Nothing needs attention", GxEPD_BLACK);
    return;
  }
  int16_t yy = y + 52;
  int16_t senderW = 150;
  for (int i = 0; i < g_nEmails; i++) {
    if (yy > y + h - 6) break;
    EmailT& e = g_emails[i];
    bullet(rx + 4, yy - 5, 4, GxEPD_RED);
    display.setFont(S().label);
    printAt(rx + 16, yy, fitStr(e.from, senderW), GxEPD_BLACK);
    display.setFont(S().body);
    int16_t sx = rx + 16 + senderW + 12;
    printAt(sx, yy, fitStr(e.subj, x + w - 62 - sx), GxEPD_BLACK);
    printRight(x + w - 14, yy, shortAge(e.ts), GxEPD_BLACK);
    yy += 22;
  }
}

// ---------------- contributed-block widgets ----------------
static void drawContribBlock(const BlockDef& def, const BlockData& d,
                             int16_t x, int16_t y, int16_t w, int16_t h) {
  int16_t cy = y + 6;
  if (def.title[0]) {
    sectionHeader(x + 12, y + 24, w - 24, def.title,
                  def.accentRed ? GxEPD_RED : GxEPD_BLACK, false);
    cy = y + 40;
  }
  if (!d.ok) {
    display.setFont(S().body);
    printAt(x + 12, cy + 22, fitStr(d.err[0] ? d.err : "no data yet", w - 24), GxEPD_RED);
    return;
  }
  char buf[96];
  switch (def.widget) {
    case BW_BIG_NUMBER: {
      if (def.wLabel[0] && !def.title[0]) {
        display.setFont(S().label);
        printAt(x + 12, cy + 18, def.wLabel, GxEPD_BLACK);
        cy += 22;
      }
      blockTemplate(def.wValue, d, buf, sizeof(buf));
      display.setFont(S().big);
      String v(buf);
      // Letters and symbols have no glyph in DashTempFont: they measure zero
      // and draw blank ("$42" became "42"), so the width test alone never
      // sent them to the real font. Ask the glyph question first.
      bool bigFont = blockBigNumDrawable(buf) && textWidth(v) <= (uint16_t)(w - 24);
      if (y + h - cy < 76) {                    // short block: compact value
        display.setFont(S().display);
        printAt(x + 12, cy + 28, fitStr(v, w - 24), def.accentRed ? GxEPD_RED : GxEPD_BLACK);
        break;
      }
      if (!bigFont) {
        display.setFont(S().display);   // fall back for long or non-numeric values
        if (textWidth(v) > (uint16_t)(w - 24)) v = fitStr(v, w - 24);
        printAt(x + 12, cy + 40, v, def.accentRed ? GxEPD_RED : GxEPD_BLACK);
        cy += 48;
      } else {
        printAt(x + 12, cy + 58, v, def.accentRed ? GxEPD_RED : GxEPD_BLACK);
        cy += 68;
      }
      if (def.wSub[0] && cy + 20 <= y + h) {
        blockTemplate(def.wSub, d, buf, sizeof(buf));
        display.setFont(S().body);
        printAt(x + 12, cy + 14, fitStr(buf, w - 24), GxEPD_BLACK);
      }
      break;
    }
    case BW_LIST: {
      const int16_t rowH = 22;
      int16_t yy = cy + 18;
      // Rows past the frame's bottom used to vanish without a trace, and
      // the row lost first is whichever the feed put last (its own "+N
      // more" notice, typically). Spend the last slot on a marker instead.
      int avail = (y + h - 6 - yy) / rowH + 1;
      int hidden = 0;
      int shown = blockListVisible(d.nRows, avail, &hidden);
      display.setFont(S().body);
      for (int i = 0; i < shown; i++) {
        bullet(x + 16, yy - 5, 3, def.accentRed ? GxEPD_RED : GxEPD_BLACK);
        int16_t secW = d.rows[i].secondary[0] ? 52 : 0;
        printAt(x + 26, yy, fitStr(d.rows[i].primary, w - 40 - secW), GxEPD_BLACK);
        if (secW) printRight(x + w - 12, yy, d.rows[i].secondary, GxEPD_BLACK);
        yy += rowH;
      }
      if (hidden && avail > 0) {
        snprintf(buf, sizeof(buf), "+%d more", hidden);
        printAt(x + 26, yy, buf, GxEPD_RED);
      }
      break;
    }
    case BW_BAR: {
      blockTemplate(def.wValue, d, buf, sizeof(buf));
      float v = atof(buf);
      if (def.wLabel[0]) {
        display.setFont(S().label);
        printAt(x + 12, cy + 18, def.wLabel, GxEPD_BLACK);
      }
      int16_t bw = w - 24, bx = x + 12, by = cy + 28;
      display.drawRect(bx, by, bw, 16, GxEPD_BLACK);
      float frac = def.barMax > 0 ? v / def.barMax : 0;
      if (frac < 0) frac = 0;
      if (frac > 1) frac = 1;
      display.fillRect(bx + 2, by + 2, (int16_t)((bw - 4) * frac), 12,
                       def.accentRed ? GxEPD_RED : GxEPD_BLACK);
      display.setFont(S().body);
      printRight(x + w - 12, cy + 62, buf, GxEPD_BLACK);
      break;
    }
    default: {  // BW_TEXT
      blockTemplate(def.wSub[0] ? def.wSub : def.wValue, d, buf, sizeof(buf));
      display.setFont(S().bodyL);
      printAt(x + 12, cy + 26, fitStr(buf, w - 24), GxEPD_BLACK);
      break;
    }
  }
}

static void drawAll() {
  int16_t W = display.width(), H = display.height();
  int16_t cw = W / GRID_COLS, chh = H / GRID_ROWS;

  display.fillScreen(GxEPD_WHITE);
  for (JsonObjectConst it : s_layoutDoc.as<JsonArrayConst>()) {
    const char* id = it["block"] | "";
    int16_t bx = (int16_t)(it["x"] | 0) * cw, by = (int16_t)(it["y"] | 0) * chh;
    int16_t bw = (int16_t)(it["w"] | 1) * cw, bh = (int16_t)(it["h"] | 1) * chh;
    if (!strcmp(id, "core-clock")) drawClockBlock(bx, by, bw, bh);
    else if (!strcmp(id, "core-datestatus")) drawDateStatusBlock(bx, by, bw, bh);
    else if (!strcmp(id, "core-weather")) drawWeatherNowBlock(bx, by, bw, bh);
    else if (!strcmp(id, "core-forecast")) drawForecastBlock(bx, by, bw, bh);
    else if (!strcmp(id, "core-calendar")) drawCalendarBlock(bx, by, bw, bh);
    else if (!strcmp(id, "core-inbox")) drawInboxBlock(bx, by, bw, bh);
    else {
      const char* inst = it["inst"] | "";
      for (int i = 0; i < s_nContrib; i++)
        if (!strcmp(s_contrib[i].inst, inst)) {
          if (s_contrib[i].loaded)
            drawContribBlock(s_contrib[i].def, s_contrib[i].data, bx, by, bw, bh);
          else {
            display.setFont(S().body);
            printAt(bx + 12, by + 24, "block missing", GxEPD_RED);
          }
          break;
        }
    }
  }
  if (!s_wifiOk)
    for (int i = 0; i < 3; i++) display.drawRect(i, i, W - 2 * i, H - 2 * i, GxEPD_RED);
}

// ---------------- setup-mode screen ----------------
static void drawSetupScreen(PortalReason reason) {
  int16_t W = display.width(), H = display.height();
  // Vertical positions scale with panel height (fractions chosen so the
  // classic 800x480 layout is pixel-identical); fonts are fixed-size.
  int16_t step = (int16_t)((int32_t)H * 96 / 480);
  int16_t hOff = (int16_t)((int32_t)H * 44 / 480);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    thickHLine(0, 0, W, 8, GxEPD_RED);
    display.setFont(S().display);
    printCentered(W / 2, (int16_t)((int32_t)H * 70 / 480), "Let's set up your dashboard", GxEPD_BLACK);
    display.setFont(S().bodyL);
    const char* why =
      (reason == PORTAL_BUTTON) ? "(setup button held - settings mode)" :
      (reason == PORTAL_FAILURES) ? "(couldn't reach WiFi - please reconfigure)" :
      "(first boot)";
    printCentered(W / 2, (int16_t)((int32_t)H * 102 / 480), why,
                  (reason == PORTAL_FAILURES) ? GxEPD_RED : GxEPD_BLACK);

    int16_t y = (int16_t)((int32_t)H * 170 / 480), x = (int16_t)((int32_t)W * 90 / 800);
    display.setFont(S().labelL);
    printAt(x, y, "1.", GxEPD_RED);
    display.setFont(S().bodyL);
    printAt(x + 34, y, "On your phone or laptop, join the WiFi network:", GxEPD_BLACK);
    display.setFont(S().display);
    printCentered(W / 2, y + hOff, PORTAL_AP_NAME, GxEPD_BLACK);

    y += step;
    display.setFont(S().labelL);
    printAt(x, y, "2.", GxEPD_RED);
    display.setFont(S().bodyL);
    printAt(x + 34, y, "A setup page opens by itself. If not, visit:", GxEPD_BLACK);
    display.setFont(S().display);
    char apUrl[28];
    snprintf(apUrl, sizeof(apUrl), "http://%d.%d.%d.%d",
             PORTAL_AP_IP1, PORTAL_AP_IP2, PORTAL_AP_IP3, PORTAL_AP_IP4);
    printCentered(W / 2, y + hOff, apUrl, GxEPD_RED);

    y += step;
    display.setFont(S().labelL);
    printAt(x, y, "3.", GxEPD_RED);
    display.setFont(S().bodyL);
    printAt(x + 34, y, "Answer the questions - WiFi, email, calendar,", GxEPD_BLACK);
    printAt(x + 34, y + 28, "weather - and the dashboard starts by itself.", GxEPD_BLACK);

    display.setFont(nullptr);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(16, H - 12);
    display.print("epaper-dashboard v" FW_VERSION "  |  to reopen setup later: tap RST, then hold BOOT ~2s");
  } while (display.nextPage());
  display.hibernate();
}

// ---------------- fetch + sleep ----------------
static bool connectWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // Always a plain DHCP client on the home LAN: clear any static-IP leftovers
  // in the WiFi stack (e.g. from previously flashed firmware) so the router
  // hands out the address — 192.168.1.x on a 192.168.1.0/24 network.
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(g_set.ssid, g_set.wifiPass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(250);
  if (WiFi.status() != WL_CONNECTED) return false;
  strlcpy(g_lastIp, WiFi.localIP().toString().c_str(), sizeof(g_lastIp));
  Serial.printf("DHCP lease: %s\n", g_lastIp);
  return true;
}

static bool timeIsValid() { return time(nullptr) > 1600000000; }

static void syncTime() {
  configTzTime(g_set.tz, "pool.ntp.org", "time.google.com", "time.nist.gov");
  uint32_t t0 = millis();
  while (!timeIsValid() && millis() - t0 < 8000) delay(200);
}

static void fetchAll() {
  if (s_skipAll) {
    s_mailOk = s_calOk = s_wxOk = false;
    strlcpy(s_mailErr, "recovery mode - fetch skipped, see serial", sizeof(s_mailErr));
    strlcpy(s_calErr, "recovery mode - fetch skipped, see serial", sizeof(s_calErr));
    Serial.println("RECOVERY: skipping all fetches after repeated crashes");
    return;
  }

  // email
  s_mailOk = false;
  s_mailErr[0] = 0;
  Serial.printf("fetch: mail (heap %u)\n", (unsigned)ESP.getFreeHeap());
  if (g_set.imUser[0]) {
    ImapResult r;
    if (imapFetch(g_set, r)) {
      s_mailOk = true;
      g_haveMail = 1;
      g_mailTs = (uint32_t)time(nullptr);
      g_unread = r.unread;
      g_nEmails = r.n;
      memcpy(g_emails, r.emails, sizeof(g_emails));
    } else {
      snprintf(s_mailErr, sizeof(s_mailErr), "%.10s: %.80s", r.stage, r.msg);
      Serial.printf("IMAP fail %s\n", s_mailErr);
    }
  } else {
    s_mailOk = true;   // disabled = not an error
  }

  // calendar
  s_calOk = false;
  s_calErr[0] = 0;
  Serial.printf("fetch: calendar (heap %u)\n", (unsigned)ESP.getFreeHeap());
  if (s_skipCal) {
    strlcpy(s_calErr, "skipped after repeated crashes - see serial", sizeof(s_calErr));
    Serial.println("RECOVERY: skipping calendar fetch");
  } else if (strcmp(g_set.calMode, "none") != 0) {
    struct tm lt;
    time_t now = time(nullptr);
    localtime_r(&now, &lt);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
    time_t midnight = mktime(&lt);
    CalResult r;
    if (calendarFetch(g_set, now - 3600, midnight + 2 * 86400, midnight, r)) {
      s_calOk = true;
      g_haveCal = 1;
      g_calTs = (uint32_t)time(nullptr);
      g_nEvents = r.n;
      memcpy(g_events, r.events, sizeof(g_events));
    } else {
      strlcpy(s_calErr, r.msg, sizeof(s_calErr));
      Serial.printf("CAL fail: %s\n", s_calErr);
    }
  } else {
    s_calOk = true;
  }

  // weather
  s_wxOk = false;
  Serial.printf("fetch: weather (heap %u)\n", (unsigned)ESP.getFreeHeap());
  WxResult w;
  if (weatherFetch(g_set, w)) {
    s_wxOk = true;
    g_haveWx = 1;
    g_wxTs = (uint32_t)time(nullptr);
    g_wx = w.wx;
  } else {
    Serial.printf("WX fail: %s\n", w.msg);
  }

  // contributed blocks (declarative fetch, SSRF-guarded, size-capped)
  for (int i = 0; i < s_nContrib; i++) {
    ContribSlot& s = s_contrib[i];
    if (!s.loaded) continue;
    Serial.printf("fetch: block %s (heap %u)\n", s.def.id, (unsigned)ESP.getFreeHeap());
    JsonObjectConst params = s_layoutDoc[s.layoutIdx]["params"];
    if (!blockFetch(s.def, params, s.data))
      Serial.printf("block %s: %s\n", s.def.id, s.data.err);
  }
}

// ---------------- portal preview (sample data on the real panel) ----------------
static void initDisplay();   // defined below (forward decl for host builds)

static void fillSampleGlobals() {
  g_now = 1784738700;   // Wed 2026-07-22 09:45 PDT — a nice-looking sample
  localtime_r(&g_now, &g_tm);
  g_haveWx = 1;
  g_wx.temp = 26; g_wx.feels = 27; g_wx.hum = 52; g_wx.wind = 6;
  g_wx.code = 2; g_wx.isDay = 1;
  strlcpy(g_wx.cond, "Partly cloudy", sizeof(g_wx.cond));
  const char* dows[3] = {"Today", "Thu", "Fri"};
  int his[3] = {29, 27, 25}, los[3] = {18, 17, 16}, codes[3] = {1, 2, 61}, pops[3] = {0, 10, 55};
  for (int i = 0; i < 3; i++) {
    strlcpy(g_wx.d[i].dow, dows[i], sizeof(g_wx.d[i].dow));
    g_wx.d[i].hi = his[i]; g_wx.d[i].lo = los[i];
    g_wx.d[i].code = codes[i]; g_wx.d[i].pop = pops[i];
  }
  g_haveCal = 1; g_nEvents = 3;
  auto ev = [&](int i, const char* t, const char* wh, uint32_t a, uint32_t b, int day, int ad) {
    strlcpy(g_events[i].title, t, sizeof(g_events[i].title));
    strlcpy(g_events[i].when, wh, sizeof(g_events[i].when));
    g_events[i].ts0 = a; g_events[i].ts1 = b; g_events[i].day = day; g_events[i].allDay = ad;
  };
  ev(0, "Team standup", "9:30 AM", 1784737800, 1784739600, 0, 0);
  ev(1, "Lunch with Sarah", "12:00 PM", 1784746800, 1784750400, 0, 0);
  ev(2, "Building inspection", "all day", 1784790000, 1784876400, 1, 1);
  g_haveMail = 1; g_unread = 7; g_nEmails = 3;
  auto em = [&](int i, const char* f, const char* su, uint32_t ts) {
    strlcpy(g_emails[i].from, f, sizeof(g_emails[i].from));
    strlcpy(g_emails[i].subj, su, sizeof(g_emails[i].subj));
    g_emails[i].ts = ts;
  };
  em(0, "Sarah Chen", "Re: lunch today?", g_now - 720);
  em(1, "GitHub", "[epaper-dash] PR #14 merged", g_now - 4500);
  em(2, "Migadu Status", "Maintenance window Sunday", g_now - 11700);
  s_mailOk = s_calOk = s_wxOk = s_wifiOk = true;
  strlcpy(g_lastIp, "192.168.1.57", sizeof(g_lastIp));
}

static void portalPreviewRender() {
  // Prefer REAL data: after a refresh cycle (LAN editor window) the caches
  // hold live mail/calendar/weather; in settings mode the RTC cache from the
  // last successful cycle usually survives too. Sample data is the fallback.
  bool haveReal = timeIsValid() && (g_haveWx || g_haveCal || g_haveMail);
  Serial.printf("portal: preview render (%s data)\n", haveReal ? "real" : "sample");
  if (haveReal) {
    g_now = time(nullptr);
    localtime_r(&g_now, &g_tm);
  } else {
    fillSampleGlobals();
  }
  s_mailOk = s_calOk = s_wxOk = s_wifiOk = true;   // previews never show FAIL flags
  prepareLayout();
  for (int i = 0; i < s_nContrib; i++)
    if (s_contrib[i].loaded && !s_contrib[i].data.ok)
      blockSampleData(s_contrib[i].def, s_contrib[i].data);
  initDisplay();
  display.setFullWindow();
  display.firstPage();
  do {
    drawAll();
  } while (display.nextPage());
  display.hibernate();
}

static void goToSleep() {
  time_t now = time(nullptr);
  uint64_t sleepSec;
  struct tm ti;
  localtime_r(&now, &ti);
  bool quiet = false;
  if (g_set.quiet && timeIsValid()) {
    if (g_set.quietStart <= g_set.quietEnd)
      quiet = (ti.tm_hour >= g_set.quietStart && ti.tm_hour < g_set.quietEnd);
    else
      quiet = (ti.tm_hour >= g_set.quietStart || ti.tm_hour < g_set.quietEnd);
  }
  if (quiet && timeIsValid()) {
    struct tm target = ti;
    target.tm_hour = g_set.quietEnd;
    target.tm_min = 0;
    target.tm_sec = 0;
    time_t t = mktime(&target);
    if (t <= now) t += 24 * 3600;
    sleepSec = (uint64_t)(t - now);
    if (sleepSec > 3 * 3600ULL) sleepSec = 3 * 3600ULL;   // chunk long sleeps
  } else if (timeIsValid()) {
    uint32_t per = (uint32_t)g_set.refreshMin * 60;
    sleepSec = per - ((uint32_t)now % per);
    if (sleepSec < 45) sleepSec += per;
  } else {
    sleepSec = 180;
  }
  Serial.printf("Deep sleep %llus\n", (unsigned long long)sleepSec);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(sleepSec * 1000000ULL);
  esp_deep_sleep_start();
}

static void initDisplay() {
  static bool inited = false;
  if (inited) return;             // portal may render more than once
  inited = true;
  hspi.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  display.setRotation(0);
  display.setTextWrap(false);
}

// ---------------- main ----------------

// IMPORTANT: holding BOOT (GPIO0) *while* RST is released puts the ESP32
// into its ROM serial-download mode — the sketch never runs and the device
// looks dead until the next plain reset. So the setup gesture is:
// tap RST first, THEN press & hold BOOT. After any non-deep-sleep reset we
// watch the button for ~2.5 s to catch that.
static bool setupButtonRequested() {
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  delay(20);
  if (digitalRead(SETUP_BUTTON_PIN) == LOW) return true;
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) return false;  // timer wake: quick check only
  Serial.println("(hold BOOT within 2.5 s to open the setup portal)");
  uint32_t t0 = millis();
  while (millis() - t0 < 2500) {
    if (digitalRead(SETUP_BUTTON_PIN) == LOW) return true;
    delay(25);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  g_bootCount++;

  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("\n== e-paper dashboard v" FW_VERSION ", boot #%lu, reset reason %d ==\n",
                (unsigned long)g_bootCount, (int)rr);

  // crash-loop accounting: previous cycle never completed?
  if (g_phase == 1) g_fetchCrashes++;
  else if (g_phase == 2) g_renderCrashes++;
  g_phase = 0;
  if (g_fetchCrashes || g_renderCrashes)
    Serial.printf("WARNING: incomplete cycles - fetch:%u render:%u\n",
                  g_fetchCrashes, g_renderCrashes);
  s_skipCal = g_fetchCrashes >= 2;
  s_skipAll = g_fetchCrashes >= 4;

  bool buttonHeld = setupButtonRequested();
  bool haveConfig = settingsLoad();
  fsStoreBegin();
  prepareLayout();
  portalSetPanelInfo(display.width(), display.height());

  if (buttonHeld || !haveConfig || g_wifiFails >= FAILS_BEFORE_PORTAL) {
    PortalReason why = buttonHeld ? PORTAL_BUTTON
                       : (!haveConfig ? PORTAL_FIRST_BOOT : PORTAL_FAILURES);
    g_wifiFails = 0;
    initDisplay();
    drawSetupScreen(why);
    portalSetPreviewHook(portalPreviewRender);
    portalRun(why);   // never returns (reboots on finish)
    return;
  }

  setenv("TZ", g_set.tz, 1);
  tzset();

  s_wifiOk = connectWiFi();
  if (s_wifiOk) {
    g_wifiFails = 0;
    syncTime();
    g_phase = 1;                 // entering fetch (crash-loop tracking)
    fetchAll();
  } else {
    g_wifiFails++;
    s_mailOk = s_calOk = s_wxOk = false;
    strlcpy(s_mailErr, "no WiFi", sizeof(s_mailErr));
    strlcpy(s_calErr, "no WiFi", sizeof(s_calErr));
    Serial.printf("WiFi failed (%d in a row)\n", g_wifiFails);
  }

  // A MANUAL reset (RST tap / power-on — not a deep-sleep timer wake) opens
  // the LAN editor window after the refresh: the portal UI stays reachable
  // at the device's LAN IP / epaper-dashboard.local while real data is
  // loaded, then the device goes back to sleep. Timer wakes skip this.
  bool editorWindow = (rr != ESP_RST_DEEPSLEEP) && s_wifiOk;
  if (!editorWindow) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  g_now = time(nullptr);
  localtime_r(&g_now, &g_tm);

  Serial.printf("render (heap %u)\n", (unsigned)ESP.getFreeHeap());
  g_phase = 2;                   // entering render
  initDisplay();
  display.setFullWindow();
  display.firstPage();
  do {
    drawAll();
  } while (display.nextPage());
  display.hibernate();

  g_phase = 0;                   // cycle completed
  if (!s_skipCal && !s_skipAll) {  // full cycle succeeded -> clear the guard
    g_fetchCrashes = 0;
    g_renderCrashes = 0;
  }
  Serial.println("cycle complete");

  if (editorWindow) {
    portalSetPreviewHook(portalPreviewRender);
    portalServeLan(5 * 60 * 1000UL, 30 * 60 * 1000UL);   // 5 min idle, 30 min cap
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  goToSleep();
}

void loop() {}
