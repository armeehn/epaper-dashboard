#pragma once
// CalDAV client: discovery (wizard) + event fetch (dashboard), and ICS-URL mode.
#include <Arduino.h>
#include "dashboard_data.h"
#include "settings.h"

struct DavCalendar {
  char name[40];
  char href[160];
};

struct DavDiscovery {
  bool ok;
  int n;
  DavCalendar cals[8];
  char msg[120];
};

// --- pure XML-scanning helpers (unit-testable) ---
// Find the text of the first <...href>...</...href> appearing after `anchor`
// (a tag-name fragment like "current-user-principal"). Empty if absent.
String davHrefAfter(const String& xml, const char* anchor);
// Split a multistatus body into <response> blocks and extract calendar
// collections (resourcetype contains "calendar", VEVENT supported if listed).
int davExtractCalendars(const String& xml, const String& baseUrl,
                        DavCalendar* out, int outCap);

// --- network operations ---
// Walk .well-known/caldav -> principal -> calendar-home-set -> calendar list.
void caldavDiscover(const String& base, const String& user, const String& pass,
                    DavDiscovery& out);
// Fetch events in [winStart, winEnd] from the configured calendar (CalDAV
// REPORT with expand, retry without) or ICS URL (streamed). Fills CalResult.
bool calendarFetch(const Settings& s, time_t winStart, time_t winEnd,
                   time_t localMidnight, CalResult& r);
