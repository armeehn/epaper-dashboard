// Renders the REAL dashboard drawing code on the host into PNG-able PPMs,
// using the mock GxEPD2 framebuffer (tools/../..: see /tmp/libs/mockepd).
// Build via tools/build_preview.sh style command in the repo docs.
#include "../firmware/epaper_dashboard/epaper_dashboard.ino"

static void fillSampleData() {
  // settings
  strcpy(g_set.ssid, "AirPort");
  strcpy(g_set.imUser, "alice@example.com");
  strcpy(g_set.imHost, "imap.migadu.com");
  strcpy(g_set.calMode, "caldav");
  strcpy(g_set.place, "Los Angeles");
  strcpy(g_set.tz, "PST8PDT,M3.2.0,M11.1.0");
  g_set.h24 = false;
  g_set.refreshMin = 5;

  // "now": Wed 2026-07-22 09:45 PDT
  g_now = 1784738700;
  localtime_r(&g_now, &g_tm);

  // weather
  g_haveWx = 1;
  g_wx.temp = 78; g_wx.feels = 79; g_wx.hum = 52; g_wx.wind = 6;
  g_wx.code = 2; g_wx.isDay = 1;
  strcpy(g_wx.cond, "Partly cloudy");
  strcpy(g_wx.d[0].dow, "Today"); g_wx.d[0].hi = 84; g_wx.d[0].lo = 64; g_wx.d[0].code = 1;  g_wx.d[0].pop = 0;
  strcpy(g_wx.d[1].dow, "Thu");   g_wx.d[1].hi = 81; g_wx.d[1].lo = 63; g_wx.d[1].code = 2;  g_wx.d[1].pop = 10;
  strcpy(g_wx.d[2].dow, "Fri");   g_wx.d[2].hi = 77; g_wx.d[2].lo = 61; g_wx.d[2].code = 61; g_wx.d[2].pop = 55;

  // calendar (ts values precomputed for PDT)
  g_haveCal = 1;
  g_nEvents = 5;
  auto ev = [&](int i, const char* t, const char* w, uint32_t a, uint32_t b, int day, int allday) {
    strcpy(g_events[i].title, t); strcpy(g_events[i].when, w);
    g_events[i].ts0 = a; g_events[i].ts1 = b;
    g_events[i].day = day; g_events[i].allDay = allday;
  };
  ev(0, "Team standup",            "9:30 AM",  1784737800, 1784739600, 0, 0);  // happening now
  ev(1, "Lunch with Sarah",        "12:00 PM", 1784746800, 1784750400, 0, 0);
  ev(2, "Dentist - Dr. Alvarez",   "2:00 PM",  1784754000, 1784757600, 0, 0);
  ev(3, "Building inspection",     "all day",  1784790000, 1784876400, 1, 1);
  ev(4, "Team standup",            "9:30 AM",  1784824200, 1784826000, 1, 0);

  // inbox
  g_haveMail = 1;
  g_unread = 7;
  g_nEmails = 5;
  auto em = [&](int i, const char* f, const char* s, uint32_t ts) {
    strcpy(g_emails[i].from, f); strcpy(g_emails[i].subj, s); g_emails[i].ts = ts;
  };
  em(0, "Sarah Chen",    "Re: lunch today? found a new spot on 3rd",      g_now - 720);
  em(1, "GitHub",        "[epaper-dash] PR #14: CalDAV expand fallback",  g_now - 4500);
  em(2, "Migadu Status", "Maintenance window Sunday 02:00 UTC",           g_now - 11700);
  em(3, "Home Depot",    "Your order is ready for pickup",                g_now - 26100);
  em(4, "Ars Technica",  "Daily: e-paper displays are having a moment",   g_now - 104000);

  strcpy(g_lastIp, "192.168.1.57");
  g_bootCount = 137;
  s_mailOk = s_calOk = s_wxOk = s_wifiOk = true;
  s_mailErr[0] = s_calErr[0] = 0;
}

int main(int, char**) {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  fillSampleData();

  drawAll();
  display.dumpPPM("preview_dashboard.ppm");

  // offline cycle: WiFi down, cached data shown with red flags
  s_wifiOk = false; s_mailOk = false; s_calOk = false; s_wxOk = false;
  drawAll();
  display.dumpPPM("preview_offline.ppm");
  s_wifiOk = true; s_mailOk = s_calOk = s_wxOk = true;

  // first-boot setup screen
  drawSetupScreen(PORTAL_FIRST_BOOT);
  display.dumpPPM("preview_setup.ppm");

  printf("previews written\n");
  return 0;
}
