#include "imap.h"
#include "net_util.h"
#include <WiFiClientSecure.h>

// ---------------- pure parsing ----------------

String imapCriteria(const Settings& s) {
  if (strcmp(s.imShow, "flagged") == 0) return "FLAGGED";
  if (strcmp(s.imShow, "both") == 0)    return "OR UNSEEN FLAGGED";
  if (strcmp(s.imShow, "custom") == 0 && s.imCustom[0]) return String(s.imCustom);
  return "UNSEEN";
}

// unfold headers + extract From/Subject/Date
void imapParseHeaderBlock(const char* block, EmailT& out) {
  memset(&out, 0, sizeof(out));
  String from, subj, date;
  String cur;
  const char* p = block;
  auto commit = [&]() {
    if (cur.startsWith("From:") || cur.startsWith("FROM:") || cur.startsWith("from:")) from = cur.substring(5);
    else if (strncasecmp(cur.c_str(), "Subject:", 8) == 0) subj = cur.substring(8);
    else if (strncasecmp(cur.c_str(), "Date:", 5) == 0) date = cur.substring(5);
    cur = "";
  };
  while (*p) {
    const char* nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    String line;
    line.reserve(len);
    for (size_t i = 0; i < len; i++) if (p[i] != '\r') line += p[i];
    if (line.length() && (line[0] == ' ' || line[0] == '\t')) {
      cur += ' ';
      String t = line;
      t.trim();
      cur += t;
    } else {
      if (cur.length()) commit();
      cur = line;
    }
    if (!nl) break;
    p = nl + 1;
  }
  if (cur.length()) commit();

  from.trim(); subj.trim(); date.trim();

  // From: display name if present, else mailbox
  char fromAscii[64];
  rfc2047ToAscii(from.c_str(), fromAscii, sizeof(fromAscii));
  String f(fromAscii);
  int lt = f.indexOf('<');
  if (lt >= 0) {
    String name = f.substring(0, lt);
    name.trim();
    if (name.length() >= 2 && name[0] == '"' && name[name.length() - 1] == '"')
      name = name.substring(1, name.length() - 1);
    name.trim();
    if (!name.length()) {                    // bare <addr>
      int gt = f.indexOf('>', lt);
      name = f.substring(lt + 1, gt > lt ? gt : f.length());
    }
    f = name;
  }
  f.trim();
  strlcpy(out.from, f.c_str(), sizeof(out.from));

  char subjAscii[80];
  rfc2047ToAscii(subj.c_str(), subjAscii, sizeof(subjAscii));
  if (!subjAscii[0]) strlcpy(subjAscii, "(no subject)", sizeof(subjAscii));
  strlcpy(out.subj, subjAscii, sizeof(out.subj));

  out.ts = (uint32_t)rfc2822ToEpoch(date.c_str());
}

// ---------------- protocol ----------------

static const uint32_t IMAP_TIMEOUT_MS = 12000;

class ImapConn {
 public:
  WiFiClientSecure cli;
  int tagN = 0;
  char lastLine[160];

  bool readLine(String& line) {
    line = "";
    uint32_t t0 = millis();
    while (millis() - t0 < IMAP_TIMEOUT_MS) {
      while (cli.available()) {
        char c = (char)cli.read();
        if (c == '\n') {
          if (line.endsWith("\r")) line.remove(line.length() - 1);
          strlcpy(lastLine, line.c_str(), sizeof(lastLine));
          return true;
        }
        if (line.length() < 4000) line += c;
      }
      if (!cli.connected() && !cli.available()) return false;
      delay(5);
    }
    return false;
  }

  String sendCmd(const String& cmd) {
    String tag = "a" + String(++tagN);
    cli.print(tag + " " + cmd + "\r\n");
    return tag;
  }

  // Wait for the tagged response; call cb for each untagged line.
  // Returns "OK", "NO", "BAD" or "" (timeout).
  template <typename F>
  String waitTagged(const String& tag, F cb) {
    String line;
    while (readLine(line)) {
      if (line.startsWith(tag + " ")) {
        String rest = line.substring(tag.length() + 1);
        if (rest.startsWith("OK")) return "OK";
        if (rest.startsWith("NO")) return "NO";
        if (rest.startsWith("BAD")) return "BAD";
        return "NO";
      }
      cb(line);
    }
    return "";
  }
  String waitTagged(const String& tag) {
    return waitTagged(tag, [](const String&) {});
  }
};

static String imapQuote(const char* s) {
  String o = "\"";
  for (const char* p = s; *p; p++) {
    if (*p == '"' || *p == '\\') o += '\\';
    o += *p;
  }
  o += '"';
  return o;
}

static void fail(ImapResult& r, const char* stage, const char* msg) {
  r.ok = false;
  strlcpy(r.stage, stage, sizeof(r.stage));
  strlcpy(r.msg, msg, sizeof(r.msg));
}

bool imapFetch(const Settings& s, ImapResult& r) {
  memset(&r, 0, sizeof(r));
  if (!s.imUser[0]) { fail(r, "config", "email not configured"); return false; }

  ImapConn c;
  c.cli.setInsecure();
  // arduino-esp32 3.x changed WiFiClient::setTimeout from seconds to
  // milliseconds; passing the wrong unit gives a 12 ms socket timeout.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  c.cli.setTimeout(IMAP_TIMEOUT_MS);
#else
  c.cli.setTimeout(IMAP_TIMEOUT_MS / 1000);
#endif
  if (!c.cli.connect(s.imHost, (uint16_t)s.imPort)) {
    fail(r, "connect", (String("can't reach ") + s.imHost + ":" + s.imPort +
                        " (host/port correct? network up?)").c_str());
    return false;
  }
  String line;
  if (!c.readLine(line) || !line.startsWith("* OK")) {
    fail(r, "greeting", "server sent no IMAP greeting");
    c.cli.stop();
    return false;
  }

  String tag = c.sendCmd("LOGIN " + imapQuote(s.imUser) + " " + imapQuote(s.imPass));
  if (c.waitTagged(tag) != "OK") {
    fail(r, "login", "login rejected - check address & password (some providers need an app password)");
    c.cli.stop();
    return false;
  }

  tag = c.sendCmd("SELECT " + imapQuote(s.imFolder));
  if (c.waitTagged(tag) != "OK") {
    fail(r, "select", (String("folder '") + s.imFolder + "' not found").c_str());
    c.cli.stop();
    return false;
  }

  // unseen count
  int ids[120];
  int nIds = 0;
  auto searchCb = [&](const String& l) {
    if (!l.startsWith("* SEARCH")) return;
    const char* p = l.c_str() + 8;
    while (*p) {
      while (*p == ' ') p++;
      int v = atoi(p);
      if (v > 0 && nIds < (int)(sizeof(ids) / sizeof(ids[0]))) ids[nIds++] = v;
      while (*p && *p != ' ') p++;
    }
  };
  tag = c.sendCmd("SEARCH UNSEEN");
  if (c.waitTagged(tag, searchCb) != "OK") {
    fail(r, "search", "SEARCH UNSEEN failed");
    c.cli.stop();
    return false;
  }
  r.unread = nIds;

  // matching set for display
  String crit = imapCriteria(s);
  if (crit != "UNSEEN") {
    nIds = 0;
    tag = c.sendCmd("SEARCH " + crit);
    if (c.waitTagged(tag, searchCb) != "OK") {
      fail(r, "search", (String("SEARCH failed: ") + crit).c_str());
      c.cli.stop();
      return false;
    }
  }

  int want = s.imCount;
  if (want > MAXM) want = MAXM;
  int from = nIds - want;
  if (from < 0) from = 0;
  // transient header buffer on the heap (keeps 4 KB out of static RAM)
  char* block = (char*)malloc(4096);
  if (nIds > 0 && block) {
    String set;
    for (int i = from; i < nIds; i++) {
      if (set.length()) set += ",";
      set += String(ids[i]);
    }
    // fetch header fields; literal comes as {len} + raw bytes
    String cmd = "FETCH " + set + " (BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])";
    tag = c.sendCmd(cmd);
    String l;
    while (c.readLine(l)) {
      if (l.startsWith(tag + " ")) {
        break;   // tagged completion
      }
      int br = l.indexOf('{');
      if (l.startsWith("*") && l.indexOf("FETCH") > 0 && br > 0) {
        long need = atol(l.c_str() + br + 1);      // literal length
        if (need < 0) need = 0;
        const long keepMax = 3800;
        long got = 0, kept = 0;
        uint32_t t0 = millis();
        uint8_t bin[128];
        while (got < need && millis() - t0 < IMAP_TIMEOUT_MS) {
          long chunk = need - got;
          if (chunk > (long)sizeof(bin)) chunk = sizeof(bin);
          int n = c.cli.read(bin, chunk);
          if (n > 0) {
            long room = keepMax - kept;
            if (room > 0) {
              long take = (n < room) ? n : room;
              memcpy(block + kept, bin, take);
              kept += take;
            }
            got += n;
          } else {
            delay(5);
          }
        }
        block[kept] = 0;
        if (r.n < MAXM) imapParseHeaderBlock(block, r.emails[r.n++]);
      }
    }
  }
  free(block);

  tag = c.sendCmd("LOGOUT");
  c.waitTagged(tag);
  c.cli.stop();

  // newest first
  for (int i = 0; i < r.n; i++)
    for (int j = i + 1; j < r.n; j++)
      if (r.emails[j].ts > r.emails[i].ts) {
        EmailT t = r.emails[i];
        r.emails[i] = r.emails[j];
        r.emails[j] = t;
      }

  r.ok = true;
  return true;
}
