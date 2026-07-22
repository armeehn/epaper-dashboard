#include "portal.h"
#include "config.h"
#include "settings.h"
#include "dashboard_data.h"
#include "imap.h"
#include "caldav.h"
#include "weather.h"
#include "portal_assets.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static DNSServer dns;

// async wifi-join state
static enum { WST_IDLE, WST_CONNECTING, WST_CONNECTED, WST_FAILED } wifiState = WST_IDLE;
static uint32_t wifiT0 = 0;
static String wifiReason;
static bool rebootRequested = false;
static uint32_t rebootAt = 0;

// ---------------- helpers ----------------

static void sendGz(const uint8_t* data, size_t len, const char* mime) {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "max-age=3600");
  server.send_P(200, mime, (const char*)data, len);
}

static void sendJson(JsonDocument& doc) {
  String s;
  serializeJson(doc, s);
  server.send(200, "application/json", s);
}

static bool readBody(JsonDocument& doc) {
  if (!server.hasArg("plain")) return false;
  return deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok;
}

static void redirectToPortal() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// Build a Settings copy from a test-request JSON (falls back to stored values)
static void imapFromJson(JsonDocument& d, Settings& s) {
  s = g_set;
  strlcpy(s.imHost, d["host"] | s.imHost, sizeof(s.imHost));
  s.imPort = d["port"] | 993;
  strlcpy(s.imUser, d["user"] | "", sizeof(s.imUser));
  const char* pw = d["pass"] | "";
  if (pw[0]) strlcpy(s.imPass, pw, sizeof(s.imPass));
  strlcpy(s.imFolder, d["folder"] | "INBOX", sizeof(s.imFolder));
  strlcpy(s.imShow, d["show"] | "unseen", sizeof(s.imShow));
  strlcpy(s.imCustom, d["custom"] | "", sizeof(s.imCustom));
  s.imCount = d["count"] | 5;
}

// ---------------- handlers ----------------

static void hRoot() { sendGz(ASSET_INDEX, ASSET_INDEX_LEN, "text/html"); }
static void hCss() { sendGz(ASSET_BOOTSTRAP, ASSET_BOOTSTRAP_LEN, "text/css"); }
static void hJs() { sendGz(ASSET_APPJS, ASSET_APPJS_LEN, "application/javascript"); }

static void hState() {
  JsonDocument doc;
  doc["ap"] = PORTAL_AP_NAME;
  doc["fw"] = FW_VERSION;
  doc["haveConfig"] = g_set.valid();
  doc["staConnected"] = WiFi.status() == WL_CONNECTED;
  if (g_set.valid()) {
    JsonDocument cfg;
    settingsToJson(cfg);
    doc["cfg"] = cfg;
  }
  sendJson(doc);
}

static void hScan() {
  int n = WiFi.scanNetworks(false, false);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  // de-dupe by SSID, keep strongest
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool dup = false;
    for (JsonObject o : arr)
      if (ssid == (const char*)o["ssid"]) {
        dup = true;
        if (WiFi.RSSI(i) > (int)o["rssi"]) o["rssi"] = WiFi.RSSI(i);
        break;
      }
    if (dup) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = ssid;
    o["rssi"] = WiFi.RSSI(i);
    o["enc"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  sendJson(doc);
}

static void hWifiJoin() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  String ssid = d["ssid"] | "";
  String pass = d["pass"] | "";
  if (!ssid.length()) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  strlcpy(g_set.ssid, ssid.c_str(), sizeof(g_set.ssid));
  strlcpy(g_set.wifiPass, pass.c_str(), sizeof(g_set.wifiPass));
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);   // DHCP on the LAN side
  WiFi.begin(g_set.ssid, g_set.wifiPass);
  wifiState = WST_CONNECTING;
  wifiT0 = millis();
  server.send(200, "application/json", "{\"started\":true}");
}

static void hWifiStatus() {
  JsonDocument doc;
  wl_status_t st = WiFi.status();
  if (wifiState == WST_CONNECTING) {
    if (st == WL_CONNECTED) wifiState = WST_CONNECTED;
    else if (millis() - wifiT0 > 25000) {
      wifiState = WST_FAILED;
      wifiReason = (st == WL_NO_SSID_AVAIL) ? "network not found (2.4 GHz only!)" : "join timeout - likely a wrong password";
    } else if (st == WL_CONNECT_FAILED) {
      wifiState = WST_FAILED;
      wifiReason = "association failed - check the password";
    }
  }
  switch (wifiState) {
    case WST_CONNECTED:
      doc["state"] = "connected";
      doc["ip"] = WiFi.localIP().toString();
      doc["rssi"] = WiFi.RSSI();
      break;
    case WST_CONNECTING: doc["state"] = "connecting"; break;
    case WST_FAILED:
      doc["state"] = "failed";
      doc["reason"] = wifiReason;
      break;
    default: doc["state"] = "idle";
  }
  sendJson(doc);
}

static void hTestImap() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false,\"msg\":\"bad request\"}"); return; }
  Settings tmp;
  imapFromJson(d, tmp);
  JsonDocument doc;
  if (WiFi.status() != WL_CONNECTED) {
    doc["ok"] = false;
    doc["stage"] = "network";
    doc["msg"] = "finish the WiFi step first - the device has no internet yet";
    sendJson(doc);
    return;
  }
  ImapResult r;
  imapFetch(tmp, r);
  doc["ok"] = r.ok;
  doc["unread"] = r.unread;
  doc["stage"] = r.stage;
  doc["msg"] = r.msg;
  JsonArray arr = doc["shown"].to<JsonArray>();
  for (int i = 0; i < r.n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["from"] = r.emails[i].from;
    o["subj"] = r.emails[i].subj;
  }
  sendJson(doc);
}

static void hDiscover() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false,\"msg\":\"bad request\"}"); return; }
  JsonDocument doc;
  if (WiFi.status() != WL_CONNECTED) {
    doc["ok"] = false;
    doc["msg"] = "finish the WiFi step first";
    sendJson(doc);
    return;
  }
  String pass = d["pass"] | "";
  if (!pass.length() && g_set.calPass[0]) pass = g_set.calPass;
  DavDiscovery disc;
  caldavDiscover(String((const char*)(d["base"] | "")), String((const char*)(d["user"] | "")), pass, disc);
  doc["ok"] = disc.ok;
  doc["msg"] = disc.msg;
  JsonArray arr = doc["calendars"].to<JsonArray>();
  for (int i = 0; i < disc.n; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = disc.cals[i].name;
    o["href"] = disc.cals[i].href;
  }
  sendJson(doc);
}

static void hTestCal() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false,\"msg\":\"bad request\"}"); return; }
  JsonDocument doc;
  if (WiFi.status() != WL_CONNECTED) {
    doc["ok"] = false;
    doc["msg"] = "finish the WiFi step first";
    sendJson(doc);
    return;
  }
  Settings tmp = g_set;
  strlcpy(tmp.calMode, d["mode"] | "caldav", sizeof(tmp.calMode));
  strlcpy(tmp.calUrl, d["url"] | "", sizeof(tmp.calUrl));
  strlcpy(tmp.calUser, d["user"] | "", sizeof(tmp.calUser));
  const char* pw = d["pass"] | "";
  if (pw[0]) strlcpy(tmp.calPass, pw, sizeof(tmp.calPass));

  time_t now = time(nullptr);
  if (now < 1600000000) now = 1753200000;   // clock not NTP-synced yet: rough 2025+ fallback
  struct tm lt;
  localtime_r(&now, &lt);
  lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
  time_t midnight = mktime(&lt);

  CalResult r;
  calendarFetch(tmp, midnight, midnight + 2 * 86400, midnight, r);
  doc["ok"] = r.ok;
  doc["count"] = r.n;
  doc["msg"] = r.msg;
  JsonArray arr = doc["sample"].to<JsonArray>();
  for (int i = 0; i < r.n && i < 3; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["title"] = r.events[i].title;
    o["when"] = r.events[i].when;
  }
  sendJson(doc);
}

static void hGeocode() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"no internet yet\"}");
    return;
  }
  server.send(200, "application/json", weatherGeocode(server.arg("q")));
}

static void hSave() {
  JsonDocument d;
  JsonDocument doc;
  if (!readBody(d) || !settingsApplyJson(d)) {
    doc["ok"] = false;
    doc["msg"] = "invalid config (WiFi name is required)";
    sendJson(doc);
    return;
  }
  bool saved = settingsSave();
  doc["ok"] = saved;
  if (!saved) doc["msg"] = "flash write failed";
  sendJson(doc);
}

static void hFinish() {
  server.send(200, "application/json", "{\"ok\":true}");
  rebootRequested = true;
  rebootAt = millis() + 1200;
}

// ---------------- main ----------------

void portalRun(PortalReason reason) {
  (void)reason;
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(PORTAL_AP_NAME, PORTAL_AP_PASS[0] ? PORTAL_AP_PASS : nullptr);
  delay(150);
  // Pin the hotspot (and its DHCP pool) to the subnet from config.h — kept
  // off the home LAN's 192.168.1.0/24 so AP+STA routing can't collide mid-setup.
  IPAddress apIp(PORTAL_AP_IP1, PORTAL_AP_IP2, PORTAL_AP_IP3, PORTAL_AP_IP4);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  delay(100);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, hRoot);
  server.on("/index.html", HTTP_GET, hRoot);
  server.on("/bootstrap.min.css", HTTP_GET, hCss);
  server.on("/app.js", HTTP_GET, hJs);
  server.on("/api/state", HTTP_GET, hState);
  server.on("/api/scan", HTTP_GET, hScan);
  server.on("/api/wifi", HTTP_POST, hWifiJoin);
  server.on("/api/wifi/status", HTTP_GET, hWifiStatus);
  server.on("/api/test/imap", HTTP_POST, hTestImap);
  server.on("/api/caldav/discover", HTTP_POST, hDiscover);
  server.on("/api/test/cal", HTTP_POST, hTestCal);
  server.on("/api/geocode", HTTP_GET, hGeocode);
  server.on("/api/save", HTTP_POST, hSave);
  server.on("/api/finish", HTTP_POST, hFinish);
  // captive-portal probes
  server.on("/generate_204", redirectToPortal);        // Android
  server.on("/gen_204", redirectToPortal);
  server.on("/hotspot-detect.html", redirectToPortal); // Apple
  server.on("/library/test/success.html", redirectToPortal);
  server.on("/ncsi.txt", redirectToPortal);            // Windows
  server.on("/connecttest.txt", redirectToPortal);
  server.on("/redirect", redirectToPortal);
  server.onNotFound(redirectToPortal);
  server.begin();

  Serial.printf("Portal up: join '%s', open http://%s/\n",
                PORTAL_AP_NAME, WiFi.softAPIP().toString().c_str());

  while (true) {
    dns.processNextRequest();
    server.handleClient();
    if (rebootRequested && millis() > rebootAt) {
      Serial.println("Setup finished - rebooting into dashboard");
      delay(100);
      ESP.restart();
    }
    delay(2);
  }
}
