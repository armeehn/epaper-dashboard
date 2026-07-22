// Host unit tests for the dashboard's parsing logic.
// Built with EpoxyDuino core + stub headers; run on Linux.
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../firmware/epaper_dashboard/net_util.h"
#include "../firmware/epaper_dashboard/ics.h"
#include "../firmware/epaper_dashboard/imap.h"
#include "../firmware/epaper_dashboard/caldav.h"
#include "../firmware/epaper_dashboard/settings.h"

static int passed = 0, failed = 0;
#define CHECK(cond, name) do { \
  if (cond) { passed++; } \
  else { failed++; printf("FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)
#define CHECK_EQ(a, b, name) do { \
  auto va = (a); auto vb = (b); \
  if (va == vb) { passed++; } \
  else { failed++; \
    printf("FAIL: %s (line %d): got %lld want %lld\n", name, __LINE__, (long long)va, (long long)vb); } \
} while (0)
#define CHECK_STR(a, b, name) do { \
  if (strcmp((a), (b)) == 0) { passed++; } \
  else { failed++; printf("FAIL: %s (line %d): got '%s' want '%s'\n", name, __LINE__, (a), (b)); } \
} while (0)

static const time_t T_RFC2822   = 1784731512;  // Wed, 22 Jul 2026 07:45:12 -0700
static const time_t T_ICS_UTC   = 1784754000;  // 20260722T210000Z
static const time_t T_ICS_LOCAL = 1784754000;  // 20260722T140000 in PDT
static const time_t T_MIDNIGHT  = 1784703600;  // 2026-07-22 00:00 PDT
static const time_t T_WED_0930  = 1784737800;
static const time_t T_THU_0930  = 1784824200;
static const time_t T_WED_1100  = 1784743200;

static void testNetUtil() {
  // rfc2822
  CHECK_EQ(rfc2822ToEpoch("Wed, 22 Jul 2026 07:45:12 -0700"), T_RFC2822, "rfc2822 with tz");
  CHECK_EQ(rfc2822ToEpoch("22 Jul 2026 14:45:12 GMT"), T_RFC2822, "rfc2822 no day, GMT");
  CHECK_EQ(rfc2822ToEpoch("garbage"), (time_t)0, "rfc2822 garbage");

  // ics stamps
  bool dateOnly, utc;
  CHECK_EQ(icsStampToEpoch("20260722T210000Z", &dateOnly, &utc), T_ICS_UTC, "ics utc");
  CHECK(utc && !dateOnly, "ics utc flags");
  CHECK_EQ(icsStampToEpoch("20260722T140000", &dateOnly, &utc), T_ICS_LOCAL, "ics local");
  CHECK(!utc && !dateOnly, "ics local flags");
  CHECK_EQ(icsStampToEpoch("20260722", &dateOnly, &utc), T_MIDNIGHT, "ics date-only");
  CHECK(dateOnly, "ics date-only flag");

  // rfc2047
  char buf[64];
  rfc2047ToAscii("=?UTF-8?Q?Caf=C3=A9_r=C3=A9sum=C3=A9?=", buf, sizeof(buf));
  CHECK_STR(buf, "Cafe resume", "rfc2047 Q");
  rfc2047ToAscii("=?utf-8?B?UmVuw6kgTcO8bGxlcg==?=", buf, sizeof(buf));   // "René Müller"
  CHECK_STR(buf, "Rene Muller", "rfc2047 B");
  rfc2047ToAscii("plain text", buf, sizeof(buf));
  CHECK_STR(buf, "plain text", "rfc2047 passthrough");
  rfc2047ToAscii("=?UTF-8?B?SGk=?= =?UTF-8?B?IHRoZXJl?=", buf, sizeof(buf));
  CHECK_STR(buf, "Hi there", "rfc2047 adjacent words");

  // urls
  String host, path;
  uint16_t port;
  bool tls;
  CHECK(splitUrl("https://cdav.migadu.com/calendars/a@b.c/", host, port, path, tls), "splitUrl ok");
  CHECK_STR(host.c_str(), "cdav.migadu.com", "splitUrl host");
  CHECK_EQ(port, 443, "splitUrl port");
  CHECK_STR(path.c_str(), "/calendars/a@b.c/", "splitUrl path");
  CHECK(splitUrl("http://x.y:8080/z", host, port, path, tls) && port == 8080 && !tls, "splitUrl port+scheme");
  CHECK(!splitUrl("ftp://nope", host, port, path, tls), "splitUrl rejects ftp");
  CHECK_STR(resolveHref("https://cdav.migadu.com/x/y", "/principals/u/").c_str(),
            "https://cdav.migadu.com/principals/u/", "resolveHref absolute path");
  CHECK_STR(resolveHref("https://a.b/x/", "https://c.d/e").c_str(), "https://c.d/e",
            "resolveHref absolute url");
  CHECK_STR(resolveHref("https://a.b/x/", "cal/").c_str(), "https://a.b/x/cal/",
            "resolveHref relative");

  String xs = "a&amp;b&lt;c&#13;&gt;";
  xmlUnescape(xs);
  CHECK_STR(xs.c_str(), "a&b<c>", "xmlUnescape");
}

static const char* SAMPLE_ICS =
  "BEGIN:VCALENDAR\r\n"
  "PRODID:-//test//EN\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:plain-1\r\n"
  "SUMMARY:Dentist\\, downtown caf\xC3\xA9\r\n"
  "DTSTART:20260722T140000\r\n"
  "DTEND:20260722T150000\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:allday-1\r\n"
  "SUMMARY:Trip day with a very long summary that will need\r\n"
  " folding across lines\r\n"
  "DTSTART;VALUE=DATE:20260723\r\n"
  "DTEND;VALUE=DATE:20260724\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:standup-1\r\n"
  "SUMMARY:Standup\r\n"
  "DTSTART;TZID=America/Los_Angeles:20260105T093000\r\n"
  "DTEND;TZID=America/Los_Angeles:20260105T094500\r\n"
  "RRULE:FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:standup-1\r\n"
  "RECURRENCE-ID;TZID=America/Los_Angeles:20260722T093000\r\n"
  "SUMMARY:Standup (moved)\r\n"
  "DTSTART;TZID=America/Los_Angeles:20260722T110000\r\n"
  "DTEND;TZID=America/Los_Angeles:20260722T111500\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:daily-1\r\n"
  "SUMMARY:Meds\r\n"
  "DTSTART:20260721T080000\r\n"
  "DTEND:20260721T080500\r\n"
  "RRULE:FREQ=DAILY;COUNT=3\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:cancelled-1\r\n"
  "SUMMARY:Cancelled thing\r\n"
  "STATUS:CANCELLED\r\n"
  "DTSTART:20260722T160000\r\n"
  "END:VEVENT\r\n"
  "BEGIN:VEVENT\r\n"
  "UID:old-1\r\n"
  "SUMMARY:Ancient event\r\n"
  "DTSTART:20200101T100000\r\n"
  "END:VEVENT\r\n"
  "END:VCALENDAR\r\n";

static void testIcs() {
  time_t ws = T_MIDNIGHT, we = T_MIDNIGHT + 2 * 86400;
  IcsParser p;
  p.begin(ws, we);
  for (const char* c = SAMPLE_ICS; *c; c++) p.feedChar(*c);
  p.end();

  CHECK_EQ(p.veventCount, 7, "ics saw all vevents");
  // kept: plain-1, allday-1, standup master, override, daily master, (cancelled kept: it's non-rrule within window -> kept then dropped at expand)
  CHECK(p.n >= 5, "ics kept relevant events");

  EventT out[MAXE];
  int n = icsExpandToWindow(p.events, p.n, ws, we, T_MIDNIGHT, false, out, MAXE);

  // expected instances:
  //  Wed: Meds 8:00 (daily COUNT=3: 21,22,23 -> 22 & 23 in window)
  //  Wed: Standup MOVED to 11:00 (base 9:30 overridden)
  //  Wed: Dentist 14:00
  //  Thu: all-day Trip
  //  Thu: Meds 8:00
  //  Thu: Standup 9:30
  bool sawDentist = false, sawTrip = false, sawMoved = false, sawThuStandup = false;
  int medsCount = 0;
  bool sawWedBaseStandup = false, sawCancelled = false, sawAncient = false;
  for (int i = 0; i < n; i++) {
    if (strcmp(out[i].title, "Dentist, downtown cafe") == 0) {
      sawDentist = true;
      CHECK_EQ((long long)out[i].ts0, (long long)T_ICS_LOCAL, "dentist ts");
      CHECK_EQ(out[i].day, 0, "dentist today");
      CHECK_STR(out[i].when, "2:00 PM", "dentist when");
    }
    if (strncmp(out[i].title, "Trip day", 8) == 0) {
      sawTrip = true;
      CHECK_EQ(out[i].allDay, 1, "trip allday");
      CHECK_EQ(out[i].day, 1, "trip tomorrow");
      CHECK_STR(out[i].when, "all day", "trip when");
    }
    if (strcmp(out[i].title, "Standup (moved)") == 0) {
      sawMoved = true;
      CHECK_EQ((long long)out[i].ts0, (long long)T_WED_1100, "moved ts");
    }
    if (strcmp(out[i].title, "Standup") == 0) {
      if (out[i].ts0 == (uint32_t)T_WED_0930) sawWedBaseStandup = true;
      if (out[i].ts0 == (uint32_t)T_THU_0930) sawThuStandup = true;
    }
    if (strcmp(out[i].title, "Meds") == 0) medsCount++;
    if (strstr(out[i].title, "Cancelled")) sawCancelled = true;
    if (strstr(out[i].title, "Ancient")) sawAncient = true;
  }
  CHECK(sawDentist, "dentist present");
  CHECK(sawTrip, "trip present");
  CHECK(sawMoved, "override instance present");
  CHECK(!sawWedBaseStandup, "overridden base instance suppressed");
  CHECK(sawThuStandup, "thursday standup present");
  CHECK_EQ(medsCount, 2, "daily COUNT=3 gives 2 in window");
  CHECK(!sawCancelled, "cancelled dropped");
  CHECK(!sawAncient, "out-of-window dropped");
  // sorted?
  for (int i = 1; i < n; i++) CHECK(out[i].ts0 >= out[i - 1].ts0, "sorted order");

  // EXDATE removal
  IcsParser p2;
  p2.begin(ws, we);
  const char* ex =
    "BEGIN:VEVENT\r\n"
    "UID:ex-1\r\nSUMMARY:Gym\r\n"
    "DTSTART:20260720T180000\r\n"
    "RRULE:FREQ=DAILY\r\n"
    "EXDATE:20260722T180000\r\n"
    "END:VEVENT\r\n";
  for (const char* c = ex; *c; c++) p2.feedChar(*c);
  p2.end();
  EventT out2[MAXE];
  int n2 = icsExpandToWindow(p2.events, p2.n, ws, we, T_MIDNIGHT, true, out2, MAXE);
  CHECK_EQ(n2, 1, "exdate removed one of two");
  CHECK_STR(out2[0].when, "18:00", "24h formatting");
  CHECK_EQ(out2[0].day, 1, "gym tomorrow only");
}

static void testImapParse() {
  EmailT e;
  imapParseHeaderBlock(
    "From: =?UTF-8?Q?Ren=C3=A9_M=C3=BCller?= <rene@example.com>\r\n"
    "Subject: Hello\r\n"
    " there folded\r\n"
    "Date: Wed, 22 Jul 2026 07:45:12 -0700\r\n", e);
  CHECK_STR(e.from, "Rene Muller", "imap from decoded");
  CHECK_STR(e.subj, "Hello there folded", "imap subject unfolded");
  CHECK_EQ((long long)e.ts, (long long)T_RFC2822, "imap date");

  imapParseHeaderBlock("From: bare@example.com\r\nDate: bad\r\n", e);
  CHECK_STR(e.from, "bare@example.com", "imap bare address");
  CHECK_STR(e.subj, "(no subject)", "imap no subject");
  CHECK_EQ((long long)e.ts, 0LL, "imap bad date -> 0");

  Settings s;
  strcpy(s.imShow, "unseen");
  CHECK(imapCriteria(s) == "UNSEEN", "criteria unseen");
  strcpy(s.imShow, "both");
  CHECK(imapCriteria(s) == "OR UNSEEN FLAGGED", "criteria both");
  strcpy(s.imShow, "custom");
  strcpy(s.imCustom, "UNSEEN FROM \"boss\"");
  CHECK(imapCriteria(s) == "UNSEEN FROM \"boss\"", "criteria custom");
}

static void testDavXml() {
  String principal =
    "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\"><d:response>"
    "<d:href>/</d:href><d:propstat><d:prop><d:current-user-principal>"
    "<d:href>/principals/alice%40example.com/</d:href>"
    "</d:current-user-principal></d:prop></d:propstat></d:response></d:multistatus>";
  CHECK_STR(davHrefAfter(principal, "current-user-principal").c_str(),
            "/principals/alice%40example.com/", "principal href");

  String list =
    "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" "
    "xmlns:cal=\"urn:ietf:params:xml:ns:caldav\" xmlns:cs=\"http://sabredav.org/ns\">"
    "<d:response><d:href>/calendars/alice/</d:href><d:propstat><d:prop>"
    "<d:resourcetype><d:collection/></d:resourcetype></d:prop></d:propstat></d:response>"
    "<d:response><d:href>/calendars/alice/home-abc123/</d:href><d:propstat><d:prop>"
    "<d:resourcetype><d:collection/><cal:calendar/></d:resourcetype>"
    "<d:displayname>Personal &amp; Family</d:displayname>"
    "<cal:supported-calendar-component-set><cal:comp name=\"VEVENT\"/>"
    "</cal:supported-calendar-component-set></d:prop></d:propstat></d:response>"
    "<d:response><d:href>/calendars/alice/tasks/</d:href><d:propstat><d:prop>"
    "<d:resourcetype><d:collection/><cal:calendar/></d:resourcetype>"
    "<d:displayname>Tasks</d:displayname>"
    "<cal:supported-calendar-component-set><cal:comp name=\"VTODO\"/>"
    "</cal:supported-calendar-component-set></d:prop></d:propstat></d:response>"
    "<d:response><d:href>/calendars/alice/work/</d:href><d:propstat><d:prop>"
    "<d:resourcetype><d:collection/><cal:calendar/></d:resourcetype>"
    "</d:prop></d:propstat></d:response>"
    "</d:multistatus>";
  DavCalendar cals[8];
  int n = davExtractCalendars(list, "https://cdav.migadu.com/calendars/alice/", cals, 8);
  CHECK_EQ(n, 2, "two VEVENT calendars found");
  if (n == 2) {
    CHECK_STR(cals[0].name, "Personal & Family", "cal 1 displayname");
    CHECK_STR(cals[0].href, "https://cdav.migadu.com/calendars/alice/home-abc123/", "cal 1 href absolute");
    CHECK_STR(cals[1].name, "work", "cal 2 fallback name from path");
  }
}

static void testSettings() {
  JsonDocument d;
  deserializeJson(d,
    "{\"wifi\":{\"ssid\":\"Net\",\"pass\":\"pw1\"},"
    "\"imap\":{\"host\":\"imap.migadu.com\",\"user\":\"a@b.c\",\"pass\":\"secret\",\"count\":99},"
    "\"cal\":{\"mode\":\"caldav\",\"url\":\"https://cdav.migadu.com/x/\",\"user\":\"a@b.c\",\"pass\":\"cs\"},"
    "\"clock\":{\"h24\":true,\"tz\":\"PST8PDT,M3.2.0,M11.1.0\"},"
    "\"refresh\":{\"min\":1}}");
  CHECK(settingsApplyJson(d), "settings apply");
  CHECK_STR(g_set.imPass, "secret", "imap pass stored");
  CHECK_EQ(g_set.imCount, MAXM, "count clamped");
  CHECK_EQ(g_set.refreshMin, 3, "refresh clamped to 3");
  CHECK(g_set.h24, "h24 flag");

  // re-apply with empty pass, same account -> secret kept
  JsonDocument d2;
  deserializeJson(d2,
    "{\"wifi\":{\"ssid\":\"Net\",\"pass\":\"\"},"
    "\"imap\":{\"host\":\"imap.migadu.com\",\"user\":\"a@b.c\",\"pass\":\"\"},"
    "\"cal\":{\"mode\":\"caldav\",\"url\":\"https://cdav.migadu.com/x/\",\"user\":\"a@b.c\",\"pass\":\"\"}}");
  CHECK(settingsApplyJson(d2), "settings re-apply");
  CHECK_STR(g_set.imPass, "secret", "imap pass kept on re-apply");
  CHECK_STR(g_set.wifiPass, "pw1", "wifi pass kept on re-apply");
  CHECK_STR(g_set.calPass, "cs", "cal pass kept on re-apply");

  // different user -> pass NOT kept
  JsonDocument d3;
  deserializeJson(d3,
    "{\"wifi\":{\"ssid\":\"Net\"},"
    "\"imap\":{\"host\":\"imap.migadu.com\",\"user\":\"other@b.c\",\"pass\":\"\"}}");
  CHECK(settingsApplyJson(d3), "settings apply new user");
  CHECK_STR(g_set.imPass, "", "imap pass dropped for new user");
}

int main(int, char**) {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  testNetUtil();
  testIcs();
  testImapParse();
  testDavXml();
  testSettings();
  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
