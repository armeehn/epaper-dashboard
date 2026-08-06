#include "settings.h"
#include "dashboard_data.h"
#include <Preferences.h>

Settings g_set;
static const char* NVS_NS = "epdash";
static const char* NVS_KEY = "cfg";

static void cpy(char* dst, size_t n, JsonVariantConst v, const char* dflt) {
  const char* s = v | dflt;
  strlcpy(dst, s ? s : "", n);
}

bool settingsApplyJson(JsonDocument& doc) {
  Settings old = g_set;   // for password-keeping
  Settings s;             // defaults

  JsonObject w = doc["wifi"];
  cpy(s.ssid, sizeof(s.ssid), w["ssid"], "");
  cpy(s.wifiPass, sizeof(s.wifiPass), w["pass"], "");
  if (!s.wifiPass[0] && old.wifiPass[0] && strcmp(s.ssid, old.ssid) == 0)
    strlcpy(s.wifiPass, old.wifiPass, sizeof(s.wifiPass));

  JsonObject m = doc["imap"];
  cpy(s.imHost, sizeof(s.imHost), m["host"], "");
  s.imPort = m["port"] | 993;
  cpy(s.imUser, sizeof(s.imUser), m["user"], "");
  cpy(s.imPass, sizeof(s.imPass), m["pass"], "");
  cpy(s.imFolder, sizeof(s.imFolder), m["folder"], "INBOX");
  cpy(s.imShow, sizeof(s.imShow), m["show"], "unseen");
  cpy(s.imCustom, sizeof(s.imCustom), m["custom"], "");
  s.imCount = m["count"] | 5;
  if (s.imCount < 1) s.imCount = 1;
  if (s.imCount > MAXM) s.imCount = MAXM;
  if (!s.imPass[0] && old.imPass[0] &&
      strcmp(s.imUser, old.imUser) == 0 && strcmp(s.imHost, old.imHost) == 0)
    strlcpy(s.imPass, old.imPass, sizeof(s.imPass));

  JsonObject c = doc["cal"];
  cpy(s.calMode, sizeof(s.calMode), c["mode"], "none");
  cpy(s.calUrl, sizeof(s.calUrl), c["url"], "");
  cpy(s.calUser, sizeof(s.calUser), c["user"], "");
  cpy(s.calPass, sizeof(s.calPass), c["pass"], "");
  if (!s.calPass[0] && old.calPass[0] && strcmp(s.calUser, old.calUser) == 0)
    strlcpy(s.calPass, old.calPass, sizeof(s.calPass));

  JsonObject x = doc["wx"];
  s.lat = x["lat"] | 0.0f;
  s.lon = x["lon"] | 0.0f;
  cpy(s.place, sizeof(s.place), x["place"], "");
  cpy(s.unitT, sizeof(s.unitT), x["unitT"], "c");
  cpy(s.unitW, sizeof(s.unitW), x["unitW"], "mph");

  JsonObject k = doc["clock"];
  s.h24 = k["h24"] | false;
  cpy(s.tz, sizeof(s.tz), k["tz"], "UTC0");
  cpy(s.tzName, sizeof(s.tzName), k["tzname"], "UTC");

  JsonObject r = doc["refresh"];
  s.refreshMin = r["min"] | 5;
  if (s.refreshMin < 3) s.refreshMin = 3;
  if (s.refreshMin > 240) s.refreshMin = 240;
  s.quiet = r["quiet"] | true;
  s.quietStart = r["qs"] | 0;
  s.quietEnd = r["qe"] | 6;

  s.allowUnsigned = doc["blocks"]["allowUnsigned"] | g_set.allowUnsigned;

  if (!s.ssid[0]) return false;
  g_set = s;
  return true;
}

void settingsToJson(JsonDocument& doc) {
  doc["wifi"]["ssid"] = g_set.ssid;                 // secrets intentionally omitted
  JsonObject m = doc["imap"].to<JsonObject>();
  m["host"] = g_set.imHost; m["port"] = g_set.imPort; m["user"] = g_set.imUser;
  m["folder"] = g_set.imFolder; m["show"] = g_set.imShow;
  m["custom"] = g_set.imCustom; m["count"] = g_set.imCount;
  JsonObject c = doc["cal"].to<JsonObject>();
  c["mode"] = g_set.calMode; c["url"] = g_set.calUrl; c["user"] = g_set.calUser;
  JsonObject x = doc["wx"].to<JsonObject>();
  x["lat"] = g_set.lat; x["lon"] = g_set.lon; x["place"] = g_set.place;
  x["unitT"] = g_set.unitT; x["unitW"] = g_set.unitW;
  JsonObject k = doc["clock"].to<JsonObject>();
  k["h24"] = g_set.h24; k["tz"] = g_set.tz; k["tzname"] = g_set.tzName;
  JsonObject r = doc["refresh"].to<JsonObject>();
  r["min"] = g_set.refreshMin; r["quiet"] = g_set.quiet;
  r["qs"] = g_set.quietStart; r["qe"] = g_set.quietEnd;
  doc["blocks"]["allowUnsigned"] = g_set.allowUnsigned;
}

bool settingsSave() {
  JsonDocument doc;
  settingsToJson(doc);
  // secrets must be persisted — add them back for storage
  doc["wifi"]["pass"] = g_set.wifiPass;
  doc["imap"]["pass"] = g_set.imPass;
  doc["cal"]["pass"] = g_set.calPass;
  String blob;
  serializeJson(doc, blob);
  Preferences p;
  if (!p.begin(NVS_NS, false)) return false;
  bool ok = p.putString(NVS_KEY, blob) > 0;
  p.end();
  return ok;
}

bool settingsLoad() {
  Preferences p;
  if (!p.begin(NVS_NS, true)) return false;
  String blob = p.getString(NVS_KEY, "");
  p.end();
  if (!blob.length()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, blob)) return false;
  if (!settingsApplyJson(doc)) return false;
  // applyJson skips secrets when empty w/ mismatch; on load they come straight in:
  strlcpy(g_set.wifiPass, doc["wifi"]["pass"] | "", sizeof(g_set.wifiPass));
  strlcpy(g_set.imPass,   doc["imap"]["pass"] | "", sizeof(g_set.imPass));
  strlcpy(g_set.calPass,  doc["cal"]["pass"]  | "", sizeof(g_set.calPass));
  return true;
}

void settingsWipe() {
  Preferences p;
  if (p.begin(NVS_NS, false)) { p.clear(); p.end(); }
}
