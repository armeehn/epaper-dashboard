#include "caldav.h"
#include "ics.h"
#include "net_util.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---------------- pure XML scanning ----------------

static int findNoCase(const String& hay, const char* needle, int from) {
  int nl = strlen(needle);
  int end = (int)hay.length() - nl;
  for (int i = from < 0 ? 0 : from; i <= end; i++) {
    int j = 0;
    while (j < nl && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j])) j++;
    if (j == nl) return i;
  }
  return -1;
}

// text inside the next <...href ...>TEXT</...> after position of `anchor`
String davHrefAfter(const String& xml, const char* anchor) {
  int a = anchor && anchor[0] ? findNoCase(xml, anchor, 0) : 0;
  if (a < 0) return "";
  int h = findNoCase(xml, "href", a);
  while (h >= 0) {
    int open = xml.lastIndexOf('<', h);
    int close = xml.indexOf('>', h);
    if (open < 0 || close < 0) return "";
    if (xml[open + 1] != '/') {                      // it's an opening tag
      int endTag = findNoCase(xml, "href", close);   // the matching </x:href>
      int endOpen = xml.lastIndexOf('<', endTag);
      if (endTag < 0 || endOpen < 0) return "";
      String v = xml.substring(close + 1, endOpen);
      v.trim();
      xmlUnescape(v);
      return v;
    }
    h = findNoCase(xml, "href", close);
  }
  return "";
}

int davExtractCalendars(const String& xml, const String& baseUrl,
                        DavCalendar* out, int outCap) {
  int n = 0;
  int pos = 0;
  while (n < outCap) {
    int rs = findNoCase(xml, "response", pos);
    if (rs < 0) break;
    int rsClose = xml.indexOf('>', rs);
    int re = findNoCase(xml, "/", rs);   // find closing </...response>
    // find the matching close tag robustly: search "response>" preceded by '/'
    int scan = rsClose;
    int reEnd = -1;
    while (true) {
      int cand = findNoCase(xml, "response", scan + 1);
      if (cand < 0) break;
      int lt = xml.lastIndexOf('<', cand);
      if (lt >= 0 && xml[lt + 1] == '/') { reEnd = xml.indexOf('>', cand); break; }
      scan = cand;
    }
    (void)re;
    if (reEnd < 0) break;
    String block = xml.substring(rs, reEnd);
    pos = reEnd;

    // must be a calendar collection
    int rt = findNoCase(block, "resourcetype", 0);
    if (rt >= 0) {
      int rtEnd = findNoCase(block, "resourcetype", rt + 5);
      String rtBlk = block.substring(rt, rtEnd > rt ? rtEnd : block.length());
      if (findNoCase(rtBlk, "calendar", 0) < 0) continue;
    } else {
      continue;
    }
    // if the server lists supported components, require VEVENT
    if (findNoCase(block, "supported-calendar-component-set", 0) >= 0 &&
        findNoCase(block, "VEVENT", 0) < 0)
      continue;

    String href = davHrefAfter(block, "");
    if (!href.length()) continue;

    String name;
    int dn = findNoCase(block, "displayname", 0);
    if (dn >= 0) {
      int gt = block.indexOf('>', dn);
      int lt2 = gt >= 0 ? block.indexOf('<', gt) : -1;
      if (gt >= 0 && lt2 > gt) {
        name = block.substring(gt + 1, lt2);
        name.trim();
        xmlUnescape(name);
      }
    }
    if (!name.length()) {                 // fall back to last path segment
      String h = href;
      while (h.endsWith("/")) h.remove(h.length() - 1);
      int cut = h.lastIndexOf('/');
      name = cut >= 0 ? h.substring(cut + 1) : h;
    }

    String abs = resolveHref(baseUrl, href);
    char ascii[40];
    utf8ToAscii(name.c_str(), ascii, sizeof(ascii));
    strlcpy(out[n].name, ascii, sizeof(out[n].name));
    strlcpy(out[n].href, abs.c_str(), sizeof(out[n].href));
    n++;
  }
  return n;
}

// ---------------- HTTP helper (custom methods + redirects) ----------------

struct DavResponse {
  int code = 0;
  String body;
  String finalUrl;
};

static bool davRequest(const char* method, String url, const String& user,
                       const String& pass, const char* depth, const String& body,
                       DavResponse& out, int maxBody = 40000) {
  const char* hdrKeys[] = {"Location"};
  for (int hop = 0; hop < 3; hop++) {
    WiFiClientSecure cli;
    cli.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.collectHeaders(hdrKeys, 1);
    if (!http.begin(cli, url)) return false;
    if (user.length()) http.setAuthorization(user.c_str(), pass.c_str());
    if (depth) http.addHeader("Depth", depth);
    http.addHeader("Content-Type", "application/xml; charset=utf-8");
    http.addHeader("Prefer", "return-minimal");
    int code = http.sendRequest(method, (uint8_t*)body.c_str(), body.length());
    if (code == 301 || code == 302 || code == 307 || code == 308) {
      String loc = http.header("Location");
      http.end();
      if (!loc.length()) return false;
      url = resolveHref(url, loc);
      continue;
    }
    out.code = code;
    out.finalUrl = url;
    if (code > 0) {
      int len = http.getSize();
      if (len > maxBody) {
        out.body = "";
        http.end();
        out.code = -100;   // too large
        return false;
      }
      out.body = http.getString();
      if ((int)out.body.length() > maxBody) {
        out.body = "";
        out.code = -100;
        http.end();
        return false;
      }
    }
    http.end();
    return true;
  }
  return false;
}

// ---------------- discovery ----------------

static void dmsg(DavDiscovery& d, const char* m) {
  strlcpy(d.msg, m, sizeof(d.msg));
}

void caldavDiscover(const String& baseIn, const String& user, const String& pass,
                    DavDiscovery& out) {
  memset(&out, 0, sizeof(out));
  String base = baseIn;
  base.trim();
  if (!base.startsWith("http")) base = "https://" + base;

  static const char* PROPFIND_CUP =
    "<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\">"
    "<d:prop><d:current-user-principal/></d:prop></d:propfind>";
  static const char* PROPFIND_HOME =
    "<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\" "
    "xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
    "<d:prop><c:calendar-home-set/></d:prop></d:propfind>";
  static const char* PROPFIND_LIST =
    "<?xml version=\"1.0\"?><d:propfind xmlns:d=\"DAV:\" "
    "xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
    "<d:prop><d:resourcetype/><d:displayname/>"
    "<c:supported-calendar-component-set/></d:prop></d:propfind>";

  // 1) find the principal
  String principal;
  String tries[2] = {base, resolveHref(base, "/.well-known/caldav")};
  DavResponse resp;
  int lastCode = 0;
  for (int i = 0; i < 2; i++) {
    if (!davRequest("PROPFIND", tries[i], user, pass, "0", PROPFIND_CUP, resp)) continue;
    lastCode = resp.code;
    if (resp.code == 207) {
      principal = davHrefAfter(resp.body, "current-user-principal");
      if (principal.length()) { base = resp.finalUrl; break; }
    }
    if (resp.code == 401) {
      dmsg(out, "server said 401 unauthorized - check username & password");
      return;
    }
  }
  if (!principal.length()) {
    // maybe the user pasted the calendar home (or a calendar) directly — try listing it
    if (davRequest("PROPFIND", base, user, pass, "1", PROPFIND_LIST, resp) && resp.code == 207) {
      out.n = davExtractCalendars(resp.body, base, out.cals, 8);
      if (out.n > 0) { out.ok = true; return; }
    }
    char m[120];
    snprintf(m, sizeof(m), "no CalDAV principal found (HTTP %d) - is the server URL right?", lastCode);
    dmsg(out, m);
    return;
  }

  // 2) principal -> calendar-home-set
  String principalUrl = resolveHref(base, principal);
  if (!davRequest("PROPFIND", principalUrl, user, pass, "0", PROPFIND_HOME, resp) ||
      resp.code != 207) {
    dmsg(out, "found account, but couldn't read calendar-home-set");
    return;
  }
  String home = davHrefAfter(resp.body, "calendar-home-set");
  if (!home.length()) {
    dmsg(out, "server did not report a calendar home");
    return;
  }

  // 3) list calendars
  String homeUrl = resolveHref(base, home);
  if (!davRequest("PROPFIND", homeUrl, user, pass, "1", PROPFIND_LIST, resp) ||
      resp.code != 207) {
    dmsg(out, "couldn't list calendars in the calendar home");
    return;
  }
  out.n = davExtractCalendars(resp.body, homeUrl, out.cals, 8);
  if (out.n == 0) {
    dmsg(out, "no VEVENT calendars found in this account");
    return;
  }
  out.ok = true;
}

// ---------------- event fetch ----------------

static void stampUtc(time_t t, char* out, size_t n) {
  struct tm g;
  gmtime_r(&t, &g);
  snprintf(out, n, "%04d%02d%02dT%02d%02d%02dZ",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
}

static String reportBody(time_t ws, time_t we, bool expand) {
  char s[20], e[20];
  stampUtc(ws, s, sizeof(s));
  stampUtc(we, e, sizeof(e));
  String b =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
    "<d:prop>";
  if (expand)
    b += String("<c:calendar-data><c:expand start=\"") + s + "\" end=\"" + e + "\"/></c:calendar-data>";
  else
    b += "<c:calendar-data/>";
  b += "</d:prop><c:filter><c:comp-filter name=\"VCALENDAR\">"
       "<c:comp-filter name=\"VEVENT\">";
  b += String("<c:time-range start=\"") + s + "\" end=\"" + e + "\"/>";
  b += "</c:comp-filter></c:comp-filter></c:filter></c:calendar-query>";
  return b;
}

// pull VEVENT blocks out of a (possibly XML-escaped) REPORT body
static void parseReportIcs(const String& bodyRaw, IcsParser& parser) {
  String body = bodyRaw;
  xmlUnescape(body);
  int pos = 0;
  while (true) {
    int b = body.indexOf("BEGIN:VCALENDAR", pos);
    if (b < 0) break;
    int e = body.indexOf("END:VCALENDAR", b);
    if (e < 0) e = body.length();
    else e += 13;
    for (int i = b; i < e; i++) parser.feedChar(body[i]);
    parser.feedChar('\n');
    pos = e;
  }
  parser.end();
}

// Stream sink so HTTPClient can pour an ICS download straight into the parser
class IcsSink : public Stream {
 public:
  IcsParser* p;
  explicit IcsSink(IcsParser* parser) : p(parser) {}
  size_t write(uint8_t c) override { p->feedChar((char)c); return 1; }
  size_t write(const uint8_t* buf, size_t size) override {
    for (size_t i = 0; i < size; i++) p->feedChar((char)buf[i]);
    return size;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

static bool fetchIcsUrl(const Settings& s, IcsParser& parser, char* msg, size_t msgLen) {
  String url = s.calUrl;
  if (url.startsWith("webcal://")) url = "https://" + url.substring(9);
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(25000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(cli, url)) {
    strlcpy(msg, "bad ICS URL", msgLen);
    return false;
  }
  if (s.calUser[0]) http.setAuthorization(s.calUser, s.calPass);
  int code = http.GET();
  if (code != 200) {
    snprintf(msg, msgLen, "ICS fetch failed (HTTP %d)", code);
    http.end();
    return false;
  }
  IcsSink sink(&parser);
  http.writeToStream(&sink);
  http.end();
  parser.end();
  return true;
}

bool calendarFetch(const Settings& s, time_t winStart, time_t winEnd,
                   time_t localMidnight, CalResult& r) {
  memset(&r, 0, sizeof(r));
  if (strcmp(s.calMode, "none") == 0) {
    strlcpy(r.msg, "calendar disabled", sizeof(r.msg));
    return false;
  }

  IcsParser parser;
  parser.begin(winStart, winEnd);

  if (strcmp(s.calMode, "ics") == 0) {
    if (!fetchIcsUrl(s, parser, r.msg, sizeof(r.msg))) return false;
  } else {
    // CalDAV REPORT with server-side expansion, falling back to raw
    DavResponse resp;
    String body = reportBody(winStart, winEnd, true);
    bool okReq = davRequest("REPORT", s.calUrl, s.calUser, s.calPass, "1", body, resp);
    bool got = okReq && resp.code == 207 && resp.body.indexOf("BEGIN:VEVENT") >= 0;
    if (!got) {
      if (okReq && resp.code == 401) {
        strlcpy(r.msg, "calendar login rejected (401)", sizeof(r.msg));
        return false;
      }
      if (!okReq && resp.code == -100) {
        strlcpy(r.msg, "calendar response too large", sizeof(r.msg));
        return false;
      }
      DavResponse resp2;
      body = reportBody(winStart, winEnd, false);
      if (!davRequest("REPORT", s.calUrl, s.calUser, s.calPass, "1", body, resp2) ||
          resp2.code != 207) {
        int codeShown = resp2.code ? resp2.code : resp.code;
        snprintf(r.msg, sizeof(r.msg), "CalDAV REPORT failed (HTTP %d)", codeShown);
        return false;
      }
      parseReportIcs(resp2.body, parser);
    } else {
      parseReportIcs(resp.body, parser);
    }
  }

  r.n = icsExpandToWindow(parser.events, parser.n, winStart, winEnd,
                          localMidnight, s.h24, r.events, MAXE);
  if (parser.overflow)
    strlcpy(r.msg, "busy calendar - some events beyond the first batch were skipped", sizeof(r.msg));
  r.ok = true;
  return true;
}
