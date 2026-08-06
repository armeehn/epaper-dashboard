/*
 * E-Paper Dashboard v2 — turn-key edition
 * ----------------------------------------------------------------------
 * ESP32 + Waveshare 7.5" tri-color panel. First boot (or BOOT held at
 * reset, or repeated failures) opens a captive-portal setup wizard
 * (Bootstrap UI at http://192.168.4.1 on the "EPaper-Dashboard" WiFi).
 * The wizard configures WiFi, IMAP email, CalDAV/ICS calendar, weather,
 * clock and refresh — testing each live — then the device runs the
 * dashboard loop: wake → fetch (IMAP + CalDAV/ICS + Open-Meteo) →
 * render → deep sleep. Last-good data survives in RTC memory; failures
 * are flagged per section instead of blanking the screen.
 *
 * Libraries (Library Manager): GxEPD2 (+deps), ArduinoJson v7.
 * Board: "ESP32 Dev Module". Partition scheme: "Huge APP".
 */

#include "config.h"
#include "settings.h"
#include "dashboard_data.h"
#include "imap.h"
#include "caldav.h"
#include "weather.h"
#include "portal.h"

#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <esp_system.h>   // esp_reset_reason()

#include <GxEPD2_3C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "ClockFont.h"   // DejaVu Serif Bold digits (generated)
#include "TempFont.h"    // DejaVu Serif Bold digits (generated)

// ---------------- display ----------------
#if defined(PANEL_75_B_V2)
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 2>
  display(GxEPD2_750c_Z08(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));   // 800x480
#elif defined(PANEL_75_B_V1)
GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT / 2>
  display(GxEPD2_750c(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));       // 640x384
#elif defined(PANEL_75_HD_B)
GxEPD2_3C<GxEPD2_750c_Z90, GxEPD2_750c_Z90::HEIGHT / 2>
  display(GxEPD2_750c_Z90(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));   // 880x528
#else
#error "Select a panel in config.h"
#endif

SPIClass hspi(HSPI);

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

// Weather icon kinds. MUST stay above the first function definition: the
// Arduino IDE auto-generates function prototypes and hoists them to just
// above the first function, so iconForCode()'s prototype (which returns
// IconKind) needs this enum declared before that point.
enum IconKind { IC_SUN, IC_PART, IC_CLOUD, IC_FOG, IC_RAIN, IC_SNOW, IC_STORM };

// ---------------- small draw helpers ----------------
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
static void sectionHeader(int16_t x, int16_t yBase, const char* label, bool stale) {
  display.setFont(&FreeSansBold9pt7b);
  printAt(x, yBase, label, GxEPD_BLACK);
  thickHLine(x, yBase + 8, textWidth(label), 3, GxEPD_RED);
  if (stale) printAt(x + textWidth(label) + 10, yBase, "!", GxEPD_RED);
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

// ---------------- dashboard sections ----------------
static void drawTopBand(int16_t W, int16_t bandH) {
  display.setFont(&DashClockFont);
  String timeStr = fmtClock(g_tm, false);
  int16_t x1, y1; uint16_t tw, th;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &tw, &th);
  int16_t tx = 20, tyBase = 104;
  printAt(tx, tyBase, timeStr, GxEPD_BLACK);
  if (!g_set.h24) {
    display.setFont(&FreeSansBold12pt7b);
    printAt(tx + tw + x1 + 12, tyBase, g_tm.tm_hour < 12 ? "AM" : "PM", GxEPD_RED);
  }

  char buf[40];
  strftime(buf, sizeof(buf), "%A, %B %e", &g_tm);
  String dateStr(buf);
  dateStr.replace("  ", " ");
  display.setFont(&FreeSansBold18pt7b);
  printRight(W - 20, 56, dateStr, GxEPD_BLACK);

  display.setFont(&FreeSans9pt7b);
  String sub = "";
  if (g_haveMail && g_unread > 0)
    sub += String(g_unread > 99 ? String("99+") : String(g_unread)) + " unread   ";
  sub += "updated " + fmtClock(g_tm, true);
  bool anyStale = (!s_mailOk && g_set.imUser[0]) ||
                  (!s_calOk && strcmp(g_set.calMode, "none") != 0) || !s_wxOk;
  printRight(W - 20, 88, sub, anyStale ? GxEPD_RED : GxEPD_BLACK);
  if (!s_wifiOk) {
    display.setFont(&FreeSansBold9pt7b);
    printRight(W - 20, 110, "OFFLINE - showing last data", GxEPD_RED);
  }
  thickHLine(0, bandH - 3, W, 3, GxEPD_BLACK);
}

static void drawWeather(int16_t leftW, int16_t bandH, int16_t H) {
  int16_t x0 = 20;
  if (!g_haveWx) {
    display.setFont(&FreeSans12pt7b);
    printAt(x0, bandH + 60, s_wxOk ? "Weather..." : "Weather unavailable", GxEPD_BLACK);
    return;
  }
  drawWeatherIcon(x0, bandH + 24, 84, g_wx.code, g_wx.isDay);

  char tbuf[8];
  snprintf(tbuf, sizeof(tbuf), "%d", g_wx.temp);
  display.setFont(&DashTempFont);
  int16_t x1, y1; uint16_t tw, th;
  display.getTextBounds(tbuf, 0, 0, &x1, &y1, &tw, &th);
  int16_t tempX = x0 + 108, tempBase = bandH + 92;
  printAt(tempX, tempBase, tbuf, GxEPD_BLACK);
  drawDegree(tempX + tw + x1 + 12, tempBase - 58, 6, GxEPD_RED);

  display.setFont(&FreeSansBold12pt7b);
  printAt(x0, bandH + 148, fitStr(g_wx.cond, leftW - 40), GxEPD_BLACK);

  char stats[56];
  snprintf(stats, sizeof(stats), "Feels %d   RH %d%%   Wind %d %s",
           g_wx.feels, g_wx.hum, g_wx.wind, strcmp(g_set.unitW, "kmh") == 0 ? "km/h" : "mph");
  display.setFont(&FreeSans9pt7b);
  printAt(x0, bandH + 176, stats, GxEPD_BLACK);

  int16_t fcTop = H - 148;
  thickHLine(16, fcTop - 10, leftW - 32, 1, GxEPD_BLACK);
  int16_t colW = (leftW - 24) / 3;
  for (int i = 0; i < 3; i++) {
    int16_t cx = 12 + colW / 2 + i * colW;
    display.setFont(&FreeSansBold9pt7b);
    printCentered(cx, fcTop + 16, g_wx.d[i].dow, (i == 0) ? GxEPD_RED : GxEPD_BLACK);
    drawWeatherIcon(cx - 21, fcTop + 26, 42, g_wx.d[i].code, true);
    char hl[16];
    snprintf(hl, sizeof(hl), "%d / %d", g_wx.d[i].hi, g_wx.d[i].lo);
    display.setFont(&FreeSans9pt7b);
    printCentered(cx, fcTop + 92, hl, GxEPD_BLACK);
    if (g_wx.d[i].pop >= 30) {
      char pp[8];
      snprintf(pp, sizeof(pp), "%d%%", g_wx.d[i].pop);
      printCentered(cx, fcTop + 114, pp, GxEPD_RED);
    }
  }
}

static void drawCalendar(int16_t rx, int16_t W, int16_t bandH, int16_t sectY) {
  bool enabled = strcmp(g_set.calMode, "none") != 0;
  sectionHeader(rx, bandH + 28, "CALENDAR", enabled && !s_calOk && g_haveCal);

  display.setFont(&FreeSans12pt7b);
  if (!enabled) {
    printAt(rx, bandH + 78, "Calendar not configured", GxEPD_BLACK);
    return;
  }
  if (!g_haveCal) {
    printAt(rx, bandH + 78, s_calOk ? "No events" : "Calendar unavailable", GxEPD_BLACK);
    if (!s_calOk && s_calErr[0]) {
      display.setFont(&FreeSans9pt7b);
      printAt(rx, bandH + 106, fitStr(s_calErr, W - rx - 24), GxEPD_RED);
    }
    return;
  }
  if (g_nEvents == 0) {
    printAt(rx, bandH + 78, "No events today or tomorrow", GxEPD_BLACK);
    return;
  }

  int16_t y = bandH + 62;
  bool tomorrowHdr = false;
  int16_t timeColW = 88;
  for (int i = 0; i < g_nEvents; i++) {
    EventT& e = g_events[i];
    if (y > sectY - 14) break;
    if (e.day == 1 && !tomorrowHdr) {
      display.setFont(&FreeSansBold9pt7b);
      printAt(rx, y, "TOMORROW", GxEPD_BLACK);
      thickHLine(rx, y + 8, 66, 2, GxEPD_RED);
      y += 30;
      tomorrowHdr = true;
      if (y > sectY - 14) break;
    }
    bool nowEv = !e.allDay && e.day == 0 &&
                 (uint32_t)g_now >= e.ts0 && (uint32_t)g_now < e.ts1;
    if (nowEv) display.fillRect(rx - 14, y - 18, 5, 24, GxEPD_RED);
    display.setFont(&FreeSansBold9pt7b);
    printRight(rx + timeColW, y, e.when, nowEv ? GxEPD_RED : GxEPD_BLACK);
    display.setFont(&FreeSans12pt7b);
    printAt(rx + timeColW + 14, y, fitStr(e.title, W - 20 - (rx + timeColW + 14)),
            nowEv ? GxEPD_RED : GxEPD_BLACK);
    y += 34;
  }
}

static void drawInbox(int16_t rx, int16_t W, int16_t sectY, int16_t H) {
  bool enabled = g_set.imUser[0] != 0;
  thickHLine(rx - 24, sectY, W - (rx - 24) - 16, 1, GxEPD_BLACK);
  sectionHeader(rx, sectY + 26, "INBOX", enabled && !s_mailOk && g_haveMail);
  if (enabled && g_haveMail && g_unread > 0) {
    char ub[20];
    snprintf(ub, sizeof(ub), "%s unread", g_unread > 99 ? "99+" : String(g_unread).c_str());
    display.setFont(&FreeSans9pt7b);
    printRight(W - 16, sectY + 26, ub, GxEPD_RED);
  }

  display.setFont(&FreeSans12pt7b);
  if (!enabled) {
    printAt(rx, sectY + 70, "Email not configured", GxEPD_BLACK);
    return;
  }
  if (!g_haveMail) {
    printAt(rx, sectY + 70, s_mailOk ? "Checking..." : "Email unavailable", GxEPD_BLACK);
    if (!s_mailOk && s_mailErr[0]) {
      display.setFont(&FreeSans9pt7b);
      printAt(rx, sectY + 96, fitStr(s_mailErr, W - rx - 24), GxEPD_RED);
    }
    return;
  }
  if (g_nEmails == 0) {
    printAt(rx, sectY + 70, "Nothing needs attention", GxEPD_BLACK);
    return;
  }

  int16_t y = sectY + 52;          // tight rows: 4 emails fit under the split
  int16_t senderW = 150;
  for (int i = 0; i < g_nEmails; i++) {
    if (y > H - 6) break;
    EmailT& e = g_emails[i];
    display.fillCircle(rx + 4, y - 5, 4, GxEPD_RED);
    display.setFont(&FreeSansBold9pt7b);
    printAt(rx + 16, y, fitStr(e.from, senderW), GxEPD_BLACK);
    display.setFont(&FreeSans9pt7b);
    int16_t sx = rx + 16 + senderW + 12;
    printAt(sx, y, fitStr(e.subj, W - 62 - sx), GxEPD_BLACK);
    printRight(W - 14, y, shortAge(e.ts), GxEPD_BLACK);
    y += 22;
  }
}

static void drawStatusLine(int16_t leftW, int16_t H) {
  // Lives entirely inside the weather column so it never crowds the
  // calendar/inbox column. Optional segments are dropped if space runs out.
  display.setFont(nullptr);   // classic 6x8 font (cursor = top-left)
  String line = "";
  if (g_set.imUser[0]) line += String("mail ") + (s_mailOk ? "ok" : "FAIL");
  if (strcmp(g_set.calMode, "none") != 0) {
    if (line.length()) line += " | ";
    line += String("cal ") + (s_calOk ? "ok" : "FAIL");
  }
  if (line.length()) line += " | ";
  line += String("wx ") + (s_wxOk ? "ok" : "FAIL");

  int16_t maxW = leftW - 26;
  String ipSeg = g_lastIp[0] ? String(" | ") + g_lastIp : String("");
  String bootSeg = String(" | #") + String(g_bootCount);
  if (textWidth(line + ipSeg + bootSeg) <= (uint16_t)maxW) line += ipSeg + bootSeg;
  else if (textWidth(line + ipSeg) <= (uint16_t)maxW) line += ipSeg;

  bool anyFail = line.indexOf("FAIL") >= 0;
  display.setTextColor(anyFail ? GxEPD_RED : GxEPD_BLACK);
  display.setCursor(16, H - 12);
  display.print(line);
}

static void drawAll() {
  int16_t W = display.width(), H = display.height();
  int16_t bandH = 118;
  int16_t leftW = (int16_t)((int32_t)W * 37 / 100);
  int16_t rx = leftW + 28;
  int16_t sectY = bandH + (int16_t)((H - bandH) * 13 / 20);

  display.fillScreen(GxEPD_WHITE);
  drawTopBand(W, bandH);
  display.fillRect(leftW, bandH, 2, H - bandH, GxEPD_BLACK);
  drawWeather(leftW, bandH, H);
  drawCalendar(rx, W, bandH, sectY);
  drawInbox(rx, W, sectY, H);
  drawStatusLine(leftW, H);
  if (!s_wifiOk)
    for (int i = 0; i < 3; i++) display.drawRect(i, i, W - 2 * i, H - 2 * i, GxEPD_RED);
}

// ---------------- setup-mode screen ----------------
static void drawSetupScreen(PortalReason reason) {
  int16_t W = display.width(), H = display.height();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    thickHLine(0, 0, W, 8, GxEPD_RED);
    display.setFont(&FreeSansBold18pt7b);
    printCentered(W / 2, 70, "Let's set up your dashboard", GxEPD_BLACK);
    display.setFont(&FreeSans12pt7b);
    const char* why =
      (reason == PORTAL_BUTTON) ? "(setup button held - settings mode)" :
      (reason == PORTAL_FAILURES) ? "(couldn't reach WiFi - please reconfigure)" :
      "(first boot)";
    printCentered(W / 2, 102, why, (reason == PORTAL_FAILURES) ? GxEPD_RED : GxEPD_BLACK);

    int16_t y = 170, x = 90;
    display.setFont(&FreeSansBold12pt7b);
    printAt(x, y, "1.", GxEPD_RED);
    display.setFont(&FreeSans12pt7b);
    printAt(x + 34, y, "On your phone or laptop, join the WiFi network:", GxEPD_BLACK);
    display.setFont(&FreeSansBold18pt7b);
    printCentered(W / 2, y + 44, PORTAL_AP_NAME, GxEPD_BLACK);

    y += 96;
    display.setFont(&FreeSansBold12pt7b);
    printAt(x, y, "2.", GxEPD_RED);
    display.setFont(&FreeSans12pt7b);
    printAt(x + 34, y, "A setup page opens by itself. If not, visit:", GxEPD_BLACK);
    display.setFont(&FreeSansBold18pt7b);
    char apUrl[28];
    snprintf(apUrl, sizeof(apUrl), "http://%d.%d.%d.%d",
             PORTAL_AP_IP1, PORTAL_AP_IP2, PORTAL_AP_IP3, PORTAL_AP_IP4);
    printCentered(W / 2, y + 44, apUrl, GxEPD_RED);

    y += 96;
    display.setFont(&FreeSansBold12pt7b);
    printAt(x, y, "3.", GxEPD_RED);
    display.setFont(&FreeSans12pt7b);
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

  if (buttonHeld || !haveConfig || g_wifiFails >= FAILS_BEFORE_PORTAL) {
    PortalReason why = buttonHeld ? PORTAL_BUTTON
                       : (!haveConfig ? PORTAL_FIRST_BOOT : PORTAL_FAILURES);
    g_wifiFails = 0;
    initDisplay();
    drawSetupScreen(why);
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
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

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
  goToSleep();
}

void loop() {}
