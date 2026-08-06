#pragma once
// Streaming ICS (iCalendar) parser + recurrence expansion into a time window.
// Pure logic (no networking) — unit-testable on host.
#include <Arduino.h>
#include <time.h>
#include "dashboard_data.h"

#define ICS_MAX_RAW 24    // raw VEVENTs kept before expansion

struct VEvent {
  char uid[28];
  char summary[60];
  time_t dtstart = 0;
  time_t dtend = 0;
  bool allday = false;
  bool cancelled = false;
  bool hasRrule = false;
  char rrule[80];
  time_t recurrenceId = 0;    // nonzero: this is an override instance
  time_t exdates[6];
  int nExdates = 0;
};

// Incremental parser: feed characters (or lines); collects VEVENTs that could
// intersect [winStart, winEnd] (recurring masters are always kept).
// NOTE: collected events live in shared static storage (attached by begin()),
// NOT inside this object — ~6 KB of VEvents on the caller's stack would
// overflow the ESP32 Arduino loop task's 8 KB stack. One parser at a time.
class IcsParser {
 public:
  VEvent* events = nullptr;   // -> static storage, valid after begin()
  int n = 0;
  bool overflow = false;      // more events matched than we could keep
  long veventCount = 0;

  void begin(time_t winStart, time_t winEnd);
  void feedChar(char ch);
  void feedLine(const char* line);   // complete UNFOLDED-or-raw line (no CRLF)
  void end();                        // flush pending buffered line

 private:
  time_t ws_ = 0, we_ = 0;
  bool inEvent_ = false;
  VEvent cur_;
  String pending_;                   // line unfolding buffer
  bool havePending_ = false;
  String lineBuf_;                   // char-mode accumulator

  void processLine(const char* line);
  void commitEvent();
};

// Expand raw events into concrete EventT instances within [winStart, winEnd].
// Handles: plain events, DAILY/WEEKLY RRULE (INTERVAL, BYDAY, UNTIL, COUNT),
// EXDATE, cancelled events, RECURRENCE-ID overrides.
// MONTHLY/YEARLY rules: only the master occurrence is considered (limitation).
// h24: time formatting for the "when" field. Returns number of events written.
int icsExpandToWindow(const VEvent* raw, int nRaw, time_t winStart, time_t winEnd,
                      time_t localMidnight, bool h24, EventT* out, int outCap);
