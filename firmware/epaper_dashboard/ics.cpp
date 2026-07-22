#include "ics.h"
#include "net_util.h"
#include <string.h>
#include <stdlib.h>

// ---------------- parser ----------------

void IcsParser::begin(time_t winStart, time_t winEnd) {
  ws_ = winStart;
  we_ = winEnd;
  n = 0;
  overflow = false;
  veventCount = 0;
  inEvent_ = false;
  havePending_ = false;
  pending_ = "";
  lineBuf_ = "";
}

void IcsParser::feedChar(char ch) {
  if (ch == '\r') return;
  if (ch == '\n') {
    // unfolding: a following line starting with space/tab continues this one —
    // we can't know yet, so buffer one line.
    if (havePending_) {
      if (lineBuf_.length() && (lineBuf_[0] == ' ' || lineBuf_[0] == '\t')) {
        pending_ += lineBuf_.substring(1);
      } else {
        processLine(pending_.c_str());
        pending_ = lineBuf_;
      }
    } else {
      pending_ = lineBuf_;
      havePending_ = true;
    }
    lineBuf_ = "";
    return;
  }
  if (lineBuf_.length() < 512) lineBuf_ += ch;
}

void IcsParser::feedLine(const char* line) {
  for (const char* p = line; *p; p++) feedChar(*p);
  feedChar('\n');
}

void IcsParser::end() {
  feedChar('\n');
  if (havePending_ && pending_.length()) processLine(pending_.c_str());
  havePending_ = false;
}

static bool propIs(const char* line, const char* name, const char** rest) {
  size_t n = strlen(name);
  if (strncasecmp(line, name, n) != 0) return false;
  if (line[n] != ':' && line[n] != ';') return false;
  *rest = line + n;
  return true;
}

// value part after ':' (params between ';' and ':' available via line scan)
static const char* propValue(const char* rest) {
  const char* c = strchr(rest, ':');
  return c ? c + 1 : rest;
}

void IcsParser::processLine(const char* line) {
  if (!line[0]) return;
  if (strcasecmp(line, "BEGIN:VEVENT") == 0) {
    inEvent_ = true;
    cur_ = VEvent();
    veventCount++;
    return;
  }
  if (strcasecmp(line, "END:VEVENT") == 0) {
    if (inEvent_) commitEvent();
    inEvent_ = false;
    return;
  }
  if (!inEvent_) return;

  const char* rest;
  if (propIs(line, "SUMMARY", &rest)) {
    char raw[96];
    strlcpy(raw, propValue(rest), sizeof(raw));
    char unesc[96];
    icalUnescapeInto(raw, unesc, sizeof(unesc));
    utf8ToAscii(unesc, cur_.summary, sizeof(cur_.summary));
  } else if (propIs(line, "UID", &rest)) {
    strlcpy(cur_.uid, propValue(rest), sizeof(cur_.uid));
  } else if (propIs(line, "DTSTART", &rest)) {
    bool dateOnly, utc;
    cur_.dtstart = icsStampToEpoch(propValue(rest), &dateOnly, &utc);
    if (dateOnly) cur_.allday = true;
  } else if (propIs(line, "DTEND", &rest)) {
    bool dateOnly, utc;
    cur_.dtend = icsStampToEpoch(propValue(rest), &dateOnly, &utc);
  } else if (propIs(line, "RRULE", &rest)) {
    cur_.hasRrule = true;
    strlcpy(cur_.rrule, propValue(rest), sizeof(cur_.rrule));
  } else if (propIs(line, "EXDATE", &rest)) {
    // possibly comma-separated
    const char* v = propValue(rest);
    while (v && *v && cur_.nExdates < (int)(sizeof(cur_.exdates) / sizeof(cur_.exdates[0]))) {
      char one[24];
      const char* comma = strchr(v, ',');
      size_t len = comma ? (size_t)(comma - v) : strlen(v);
      if (len >= sizeof(one)) len = sizeof(one) - 1;
      memcpy(one, v, len);
      one[len] = 0;
      time_t t = icsStampToEpoch(one, nullptr, nullptr);
      if (t) cur_.exdates[cur_.nExdates++] = t;
      v = comma ? comma + 1 : nullptr;
    }
  } else if (propIs(line, "RECURRENCE-ID", &rest)) {
    cur_.recurrenceId = icsStampToEpoch(propValue(rest), nullptr, nullptr);
  } else if (propIs(line, "STATUS", &rest)) {
    if (strcasecmp(propValue(rest), "CANCELLED") == 0) cur_.cancelled = true;
  }
}

void IcsParser::commitEvent() {
  if (!cur_.dtstart) return;
  time_t end = cur_.dtend ? cur_.dtend : cur_.dtstart + (cur_.allday ? 86400 : 3600);
  // keep if it may intersect the window; recurring masters always kept
  if (!cur_.hasRrule && !cur_.recurrenceId) {
    if (end <= ws_ || cur_.dtstart >= we_) return;
  }
  if (cur_.hasRrule) {
    // cheap reject: UNTIL in the past
    const char* u = strstr(cur_.rrule, "UNTIL=");
    if (u) {
      char stamp[24];
      strlcpy(stamp, u + 6, sizeof(stamp));
      char* semi = strchr(stamp, ';');
      if (semi) *semi = 0;
      time_t until = icsStampToEpoch(stamp, nullptr, nullptr);
      if (until && until < ws_) return;
    }
  }
  if (n >= ICS_MAX_RAW) {
    overflow = true;
    return;
  }
  events[n++] = cur_;
}

// ---------------- recurrence expansion ----------------

struct Rrule {
  char freq[10] = "";
  int interval = 1;
  int count = 0;          // 0 = none
  time_t until = 0;       // 0 = none
  uint8_t byday = 0;      // bit 0=SU .. 6=SA; 0 = not given
};

static int dowIndex(const char* d) {
  static const char* names[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  for (int i = 0; i < 7; i++)
    if (strncasecmp(d, names[i], 2) == 0) return i;
  return -1;
}

static void parseRrule(const char* s, Rrule& r) {
  char buf[80];
  strlcpy(buf, s, sizeof(buf));
  char* save = nullptr;
  for (char* tok = strtok_r(buf, ";", &save); tok; tok = strtok_r(nullptr, ";", &save)) {
    char* eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = 0;
    const char* val = eq + 1;
    if (strcasecmp(tok, "FREQ") == 0) strlcpy(r.freq, val, sizeof(r.freq));
    else if (strcasecmp(tok, "INTERVAL") == 0) { r.interval = atoi(val); if (r.interval < 1) r.interval = 1; }
    else if (strcasecmp(tok, "COUNT") == 0) r.count = atoi(val);
    else if (strcasecmp(tok, "UNTIL") == 0) r.until = icsStampToEpoch(val, nullptr, nullptr);
    else if (strcasecmp(tok, "BYDAY") == 0) {
      const char* p = val;
      while (*p) {
        while (*p && !isalpha((unsigned char)*p)) p++;    // skip ordinals like 2 or -1
        int d = dowIndex(p);
        if (d >= 0) r.byday |= (1 << d);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
      }
    }
  }
}

static bool isExdate(const VEvent& e, time_t t) {
  for (int i = 0; i < e.nExdates; i++)
    if (e.exdates[i] == t) return true;
  return false;
}

static void fmtWhen(time_t t, bool allday, bool h24, char* out, size_t outLen) {
  if (allday) {
    strlcpy(out, "all day", outLen);
    return;
  }
  struct tm ti;
  localtime_r(&t, &ti);
  if (h24) snprintf(out, outLen, "%02d:%02d", ti.tm_hour, ti.tm_min);
  else {
    int h = ti.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(out, outLen, "%d:%02d %s", h, ti.tm_min, ti.tm_hour < 12 ? "AM" : "PM");
  }
}

static void emit(const VEvent& e, time_t st, time_t en, time_t localMidnight,
                 bool h24, EventT* out, int& n, int outCap) {
  if (n >= outCap) return;
  EventT& o = out[n];
  memset(&o, 0, sizeof(o));
  strlcpy(o.title, e.summary[0] ? e.summary : "(untitled)", sizeof(o.title));
  o.ts0 = (uint32_t)st;
  o.ts1 = (uint32_t)en;
  o.allDay = e.allday ? 1 : 0;
  o.day = (st < localMidnight + 86400) ? 0 : 1;
  fmtWhen(st, e.allday, h24, o.when, sizeof(o.when));
  n++;
}

int icsExpandToWindow(const VEvent* raw, int nRaw, time_t winStart, time_t winEnd,
                      time_t localMidnight, bool h24, EventT* out, int outCap) {
  // collect override keys: instances redefined elsewhere (RECURRENCE-ID)
  struct OKey { char uid[28]; time_t rid; };
  OKey overrides[ICS_MAX_RAW];
  int nOv = 0;
  for (int i = 0; i < nRaw; i++)
    if (raw[i].recurrenceId && nOv < ICS_MAX_RAW) {
      strlcpy(overrides[nOv].uid, raw[i].uid, sizeof(overrides[nOv].uid));
      overrides[nOv].rid = raw[i].recurrenceId;
      nOv++;
    }
  auto isOverridden = [&](const VEvent& e, time_t t) {
    for (int i = 0; i < nOv; i++)
      if (overrides[i].rid == t && strncmp(overrides[i].uid, e.uid, sizeof(overrides[i].uid)) == 0)
        return true;
    return false;
  };

  int n = 0;
  for (int i = 0; i < nRaw; i++) {
    const VEvent& e = raw[i];
    if (e.cancelled) continue;
    time_t dur = (e.dtend > e.dtstart) ? (e.dtend - e.dtstart) : (e.allday ? 86400 : 3600);

    if (!e.hasRrule) {
      time_t en = e.dtstart + dur;
      if (en > winStart && e.dtstart < winEnd) {
        // an override instance replaces a master occurrence; both are plain here
        emit(e, e.dtstart, en, localMidnight, h24, out, n, outCap);
      }
      continue;
    }

    Rrule r;
    parseRrule(e.rrule, r);
    bool daily = strcasecmp(r.freq, "DAILY") == 0;
    bool weekly = strcasecmp(r.freq, "WEEKLY") == 0;
    if (!daily && !weekly) {
      // MONTHLY/YEARLY etc: master occurrence only (documented limitation)
      time_t en = e.dtstart + dur;
      if (en > winStart && e.dtstart < winEnd)
        emit(e, e.dtstart, en, localMidnight, h24, out, n, outCap);
      continue;
    }

    struct tm st;
    localtime_r(&e.dtstart, &st);

    // Walk occurrences from DTSTART. Occurrence k semantics:
    //  DAILY: dtstart + k*interval days
    //  WEEKLY: for each interval-week, each BYDAY weekday (default: DTSTART's)
    int made = 0;         // occurrences generated so far (for COUNT)
    const int MAX_ITER = 800;
    if (daily) {
      for (int k = 0; k < MAX_ITER; k++) {
        // compute via localtime to survive DST changes: add days to civil date
        struct tm t = st;
        t.tm_mday += k * r.interval;
        t.tm_isdst = -1;
        time_t occ = mktime(&t);
        if (r.until && occ > r.until) break;
        if (r.count && made >= r.count) break;
        made++;
        if (occ >= winEnd) break;
        if (occ + dur <= winStart) continue;
        if (isExdate(e, occ) || isOverridden(e, occ)) continue;
        emit(e, occ, occ + dur, localMidnight, h24, out, n, outCap);
      }
    } else {  // weekly
      uint8_t days = r.byday;
      struct tm sd;
      localtime_r(&e.dtstart, &sd);
      if (!days) days = (1 << sd.tm_wday);
      bool stop = false;
      for (int week = 0; week < MAX_ITER / 7 && !stop; week++) {
        for (int dow = 0; dow < 7; dow++) {
          if (!(days & (1 << dow))) continue;
          struct tm t = sd;
          // day offset from DTSTART's day to this dow in week #week*interval
          int delta = (dow - sd.tm_wday + 7) % 7 + week * 7 * r.interval;
          t.tm_mday += delta;
          t.tm_isdst = -1;
          time_t occ = mktime(&t);
          if (occ < e.dtstart) continue;
          if (r.until && occ > r.until) { stop = true; break; }
          if (r.count && made >= r.count) { stop = true; break; }
          made++;
          if (occ >= winEnd) { stop = true; break; }
          if (occ + dur <= winStart) continue;
          if (isExdate(e, occ) || isOverridden(e, occ)) continue;
          emit(e, occ, occ + dur, localMidnight, h24, out, n, outCap);
        }
      }
    }
  }

  // sort by start
  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++)
      if (out[j].ts0 < out[i].ts0 ||
          (out[j].ts0 == out[i].ts0 && out[j].allDay > out[i].allDay)) {
        EventT t = out[i];
        out[i] = out[j];
        out[j] = t;
      }
  return n;
}
