#pragma once
// Runtime configuration, stored as one JSON blob in NVS (Preferences).
#include <Arduino.h>
#include <ArduinoJson.h>

struct Settings {
  // wifi
  char ssid[33] = "";
  char wifiPass[65] = "";
  // imap ("" user = email disabled)
  char imHost[64] = "";
  int  imPort = 993;
  char imUser[64] = "";
  char imPass[64] = "";
  char imFolder[32] = "INBOX";
  char imShow[8] = "unseen";    // unseen | flagged | both | custom
  char imCustom[96] = "";
  int  imCount = 5;
  // calendar
  char calMode[8] = "none";     // none | caldav | ics
  char calUrl[160] = "";
  char calUser[64] = "";
  char calPass[64] = "";
  // weather
  float lat = 0, lon = 0;
  char place[40] = "";
  char unitT[2] = "c";          // c | f
  char unitW[4] = "mph";        // mph | kmh
  // clock
  bool h24 = false;
  char tz[64] = "UTC0";
  char tzName[40] = "UTC";
  // refresh
  int refreshMin = 5;
  bool quiet = true;
  int quietStart = 0, quietEnd = 6;
  // blocks policy
  bool allowUnsigned = false;   // install unsigned blocks? (explicit opt-in)

  bool valid() const { return ssid[0] != 0; }
};

extern Settings g_set;

bool settingsLoad();                       // from NVS; false if none stored
bool settingsSave();                       // to NVS
void settingsWipe();
// Apply a config JSON (from the portal). Empty passwords keep the stored
// ones when the rest of that account is unchanged (so re-opening the portal
// never wipes secrets).
bool settingsApplyJson(JsonDocument& doc);
// Serialize for the portal (secrets omitted).
void settingsToJson(JsonDocument& doc);
