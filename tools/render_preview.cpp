// Renders the REAL dashboard drawing code on the host into PNG-able PPMs,
// using the mock GxEPD2 framebuffer. Scene 4 installs real SIGNED blocks
// through the actual verify path (mbedTLS) and renders a custom layout.
#include "../firmware/epaper_dashboard/epaper_dashboard.ino"

static void fillSampleData() {
  strcpy(g_set.ssid, "AirPort");
  strcpy(g_set.imUser, "alice@example.com");
  strcpy(g_set.calMode, "caldav");
  strcpy(g_set.unitT, "c");
  g_set.h24 = false;
  g_set.refreshMin = 5;
  fillSampleGlobals();               // the firmware's own sample data
  g_nEvents = 5;
  auto ev = [&](int i, const char* t, const char* w, uint32_t a, uint32_t b, int day, int ad) {
    strcpy(g_events[i].title, t); strcpy(g_events[i].when, w);
    g_events[i].ts0 = a; g_events[i].ts1 = b; g_events[i].day = day; g_events[i].allDay = ad;
  };
  ev(2, "Dentist - Dr. Alvarez", "2:00 PM", 1784754000, 1784757600, 0, 0);
  ev(3, "Building inspection", "all day", 1784790000, 1784876400, 1, 1);
  ev(4, "Team standup", "9:30 AM", 1784824200, 1784826000, 1, 0);
  g_nEmails = 4;
  strcpy(g_emails[3].from, "Home Depot");
  strcpy(g_emails[3].subj, "Your order is ready for pickup");
  g_emails[3].ts = g_now - 26100;
  g_bootCount = 137;
}

static String readAll(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) { printf("MISSING %s\n", path); return String(); }
  String s;
  int c;
  while ((c = fgetc(f)) != EOF) s += (char)c;
  fclose(f);
  return s;
}

static void render(const char* out) {
  display.setFullWindow();
  display.firstPage();
  do { drawAll(); } while (display.nextPage());
  display.dumpPPM(out);
}

int main(int, char**) {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  system("rm -rf /tmp/fsroot && mkdir -p /tmp/fsroot/b");

  fillSampleData();
  prepareLayout();                       // default (classic) layout from FS
  render("preview_dashboard.ppm");

  s_wifiOk = false; s_mailOk = s_calOk = s_wxOk = false;
  render("preview_offline.ppm");
  s_wifiOk = true; s_mailOk = s_calOk = s_wxOk = true;

  drawSetupScreen(PORTAL_FIRST_BOOT);
  display.dumpPPM("preview_setup.ppm");

  // ---- scene 4: signed blocks installed for real + custom layout ----
  const char* base = "registry/blocks";   // run the preview from the repo root
  char err[96];
  EpbInfo info;
  const char* names[3] = {"hackernews-top", "crypto-price", "github-stars"};
  for (int i = 0; i < 3; i++) {
    char p[160];
    snprintf(p, sizeof(p), "%s/%s/%s.epb", base, names[i], names[i]);
    String epb = readAll(p);
    bool ok = blockInstall(epb.c_str(), epb.length(), /*allowUnsigned=*/false,
                           err, sizeof(err), &info);
    printf("install %s: %s (sigOk=%d keyid=%s) %s\n", names[i],
           ok ? "OK" : "FAIL", info.sigOk, info.keyid, ok ? "" : err);
    if (!ok) return 1;
  }
  const char* customLayout =
    "[{\"inst\":\"clk\",\"block\":\"core-clock\",\"x\":0,\"y\":0,\"w\":7,\"h\":3},"
    "{\"inst\":\"dat\",\"block\":\"core-datestatus\",\"x\":7,\"y\":0,\"w\":9,\"h\":3},"
    "{\"inst\":\"wx\",\"block\":\"core-weather\",\"x\":0,\"y\":3,\"w\":6,\"h\":5},"
    "{\"inst\":\"fc\",\"block\":\"core-forecast\",\"x\":0,\"y\":8,\"w\":6,\"h\":4},"
    "{\"inst\":\"cal\",\"block\":\"core-calendar\",\"x\":6,\"y\":3,\"w\":10,\"h\":4},"
    "{\"inst\":\"hn\",\"block\":\"hackernews-top\",\"x\":6,\"y\":7,\"w\":10,\"h\":3},"
    "{\"inst\":\"btc\",\"block\":\"crypto-price\",\"x\":6,\"y\":10,\"w\":5,\"h\":2,"
      "\"params\":{\"coin\":\"bitcoin\"}},"
    "{\"inst\":\"gh\",\"block\":\"github-stars\",\"x\":11,\"y\":10,\"w\":5,\"h\":2}]";
  if (!layoutSave(customLayout, strlen(customLayout), err, sizeof(err))) {
    printf("layoutSave FAIL: %s\n", err);
    return 1;
  }
  prepareLayout();
  for (int i = 0; i < s_nContrib; i++)
    if (s_contrib[i].loaded) blockSampleData(s_contrib[i].def, s_contrib[i].data);
  // nicer sample rows for the HN block
  for (int i = 0; i < s_nContrib; i++)
    if (!strcmp(s_contrib[i].def.id, "hackernews-top")) {
      BlockData& d = s_contrib[i].data;
      const char* t[4] = {"Show HN: I built an e-paper dashboard with signed blocks",
                          "The quiet beauty of tri-color e-ink",
                          "SabreDAV at 20: CalDAV that just works",
                          "Why declarative beats scriptable for IoT plugins"};
      d.nRows = 4;
      for (int r = 0; r < 4; r++) {
        strcpy(d.rows[r].primary, t[r]);
        snprintf(d.rows[r].secondary, sizeof(d.rows[r].secondary), "%d", 512 - r * 87);
      }
    }
  render("preview_blocks.ppm");

  // ---- scene 5: what a widget does with content it cannot draw ----
  // A big-number bound to a non-numeric value ("n/a", "$42") and a list
  // handed more rows than its frame holds. Both once drew blank space,
  // which on a wall is indistinguishable from good news.
  const char* limitsLayout =
    "[{\"inst\":\"clk\",\"block\":\"core-clock\",\"x\":0,\"y\":0,\"w\":7,\"h\":3},"
    "{\"inst\":\"dat\",\"block\":\"core-datestatus\",\"x\":7,\"y\":0,\"w\":9,\"h\":3},"
    "{\"inst\":\"gh\",\"block\":\"github-stars\",\"x\":0,\"y\":3,\"w\":5,\"h\":3},"
    "{\"inst\":\"btc\",\"block\":\"crypto-price\",\"x\":0,\"y\":6,\"w\":5,\"h\":3,"
      "\"params\":{\"coin\":\"bitcoin\"}},"
    "{\"inst\":\"hn\",\"block\":\"hackernews-top\",\"x\":5,\"y\":3,\"w\":11,\"h\":3},"
    "{\"inst\":\"hn2\",\"block\":\"hackernews-top\",\"x\":5,\"y\":6,\"w\":11,\"h\":6}]";
  if (!layoutSave(limitsLayout, strlen(limitsLayout), err, sizeof(err))) {
    printf("layoutSave FAIL: %s\n", err);
    return 1;
  }
  prepareLayout();
  for (int i = 0; i < s_nContrib; i++) {
    ContribSlot& c = s_contrib[i];
    if (!c.loaded) continue;
    blockSampleData(c.def, c.data);
    BlockData& d = c.data;
    if (!strcmp(c.inst, "gh")) strcpy(d.values[0].text, "n/a");
    if (!strcmp(c.inst, "btc")) strcpy(d.values[0].text, "$42");
    if (!strcmp(c.def.id, "hackernews-top")) {
      d.nRows = 6;
      for (int r = 0; r < 6; r++) {
        snprintf(d.rows[r].primary, sizeof(d.rows[0].primary), "Story number %d", r + 1);
        snprintf(d.rows[r].secondary, sizeof(d.rows[0].secondary), "%d", 600 - r * 90);
      }
    }
  }
  render("preview_widget_limits.ppm");

  printf("previews written\n");
  return 0;
}
