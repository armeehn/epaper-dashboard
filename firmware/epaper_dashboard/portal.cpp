#include "portal.h"
#include "config.h"
#include "settings.h"
#include "dashboard_data.h"
#include "imap.h"
#include "caldav.h"
#include "weather.h"
#include "blocks.h"
#include "blocksig.h"
#include "fsstore.h"
#include "net_util.h"
#include "portal_assets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

static WebServer server(80);
static DNSServer dns;
static uint32_t s_lastActivity = 0;
static void touch() { s_lastActivity = millis(); }

static uint16_t s_panelW = 800, s_panelH = 480;
void portalSetPanelInfo(uint16_t w, uint16_t h) { s_panelW = w; s_panelH = h; }

// async wifi-join state
static enum { WST_IDLE, WST_CONNECTING, WST_CONNECTED, WST_FAILED } wifiState = WST_IDLE;
static uint32_t wifiT0 = 0;
static String wifiReason;
static bool rebootRequested = false;
static uint32_t rebootAt = 0;

// ---------------- helpers ----------------

static void sendGz(const uint8_t* data, size_t len, const char* mime) {
  touch();
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "max-age=3600");
  server.send_P(200, mime, (const char*)data, len);
}

static void sendJson(JsonDocument& doc) {
  touch();
  String s;
  serializeJson(doc, s);
  server.send(200, "application/json", s);
}

static bool readBody(JsonDocument& doc) {
  touch();
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

// Everything the setup page needs in order to describe THIS device rather
// than assume a particular panel, board or chip. The page has no hardware
// constants of its own; adding a panel preset to config.h is enough to make
// the UI describe it correctly.
static void hState() {
  JsonDocument doc;
  doc["ap"] = PORTAL_AP_NAME;
  doc["fw"] = FW_VERSION;
  doc["haveConfig"] = g_set.valid();
  doc["staConnected"] = WiFi.status() == WL_CONNECTED;

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["panel"] = PANEL_NAME;
  dev["panelW"] = s_panelW;
  dev["panelH"] = s_panelH;
  dev["colors"] = EPD_IS_3C ? 3 : 2;
  dev["refreshSec"] = PANEL_REFRESH_S;
  dev["board"] = BOARD_NAME;
  dev["chip"] = CHIP_NAME;
  dev["cols"] = GRID_COLS;
  dev["rows"] = GRID_ROWS;
  dev["hostname"] = DEVICE_HOSTNAME;
  dev["apIp"] = WiFi.softAPIP().toString();
  dev["setupPin"] = SETUP_BUTTON_PIN;
  dev["registry"] = DEFAULT_REGISTRY_URL;

  // Also kept flat: the browser caches app.js for an hour, so right after an
  // OTA update a stale copy of the page may still be reading these.
  doc["panelW"] = s_panelW;
  doc["panelH"] = s_panelH;
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
  // Re-running the WiFi step with a blank password + unchanged SSID keeps
  // the stored password instead of clobbering it.
  bool keepStored = (pass.length() == 0) && (ssid == String(g_set.ssid)) && g_set.wifiPass[0];
  strlcpy(g_set.ssid, ssid.c_str(), sizeof(g_set.ssid));
  if (!keepStored) strlcpy(g_set.wifiPass, pass.c_str(), sizeof(g_set.wifiPass));
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

// GET /api/imap/detect?domain=example.com
// Walks the conventional host names for a mail domain and reports the first
// one that answers with an IMAP greeting. This is what lets the wizard set up
// a provider nobody has added to any list.
static void hImapDetect() {
  JsonDocument doc;
  if (WiFi.status() != WL_CONNECTED) {
    doc["ok"] = false;
    doc["msg"] = "finish the WiFi step first - the device has no internet yet";
    sendJson(doc);
    return;
  }
  String domain = server.arg("domain");
  domain.toLowerCase();
  domain.trim();
  int at = domain.indexOf('@');
  if (at >= 0) domain = domain.substring(at + 1);
  if (!mailDomainAllowed(domain)) {
    doc["ok"] = false;
    doc["msg"] = "that doesn't look like a mail domain";
    sendJson(doc);
    return;
  }
  const char* prefixes[] = { "imap.", "mail.", "", "imap4.", "secure." };
  char banner[120];
  JsonArray tried = doc["tried"].to<JsonArray>();
  for (const char* pfx : prefixes) {
    String host = String(pfx) + domain;
    tried.add(host);
    if (imapProbe(host.c_str(), 993, banner, sizeof(banner))) {
      doc["ok"] = true;
      doc["host"] = host;
      doc["port"] = 993;
      doc["banner"] = banner;
      sendJson(doc);
      return;
    }
  }
  doc["ok"] = false;
  doc["msg"] = "no IMAP server answered on the usual names - enter the host under Advanced";
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

// ---------------- blocks & layout API ----------------

static void (*s_previewHook)() = nullptr;
void portalSetPreviewHook(void (*fn)()) { s_previewHook = fn; }

static bool httpGetText(const String& url, String& out, size_t cap,
                        char* err, size_t errLen) {
  if (!blockUrlAllowed(url, err, errLen)) return false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(12000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(cli, url)) { strlcpy(err, "bad URL", errLen); return false; }
  int code = http.GET();
  if (code != 200) {
    snprintf(err, errLen, "HTTP %d", code);
    http.end();
    return false;
  }
  out = http.getString();
  http.end();
  if (out.length() > cap) { strlcpy(err, "file too large", errLen); return false; }
  return true;
}

static void hLayoutGet() {
  JsonDocument doc;
  layoutLoad(doc);
  sendJson(doc);
}

static void hLayoutPost() {
  String body = server.arg("plain");
  char err[80];
  JsonDocument doc;
  bool ok = layoutSave(body.c_str(), body.length(), err, sizeof(err));
  doc["ok"] = ok;
  if (!ok) doc["msg"] = err;
  sendJson(doc);
}

static void hBlocksGet() {
  JsonDocument idx;
  blocksList(idx);
  // enrich with parsed param metadata so the editor can build forms
  JsonDocument out;
  out["allowUnsigned"] = g_set.allowUnsigned;
  JsonArray arr = out["blocks"].to<JsonArray>();
  for (JsonObjectConst b : idx.as<JsonArrayConst>()) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = b["id"]; o["name"] = b["name"]; o["author"] = b["author"];
    o["version"] = b["version"]; o["sigOk"] = b["sigOk"]; o["keyid"] = b["keyid"];
    BlockDef def;
    if (blockLoadDef(b["id"] | "", def)) {
      o["minW"] = def.minW; o["minH"] = def.minH;
      JsonArray ps = o["params"].to<JsonArray>();
      for (int i = 0; i < def.nParams; i++) {
        JsonObject p = ps.add<JsonObject>();
        p["key"] = def.params[i].key; p["label"] = def.params[i].label;
        p["type"] = def.params[i].type; p["default"] = def.params[i].defval;
        p["choices"] = def.params[i].choices;
      }
    }
  }
  sendJson(out);
}

static void installFromContent(const String& content) {
  JsonDocument doc;
  char err[96];
  EpbInfo info;
  bool ok = blockInstall(content.c_str(), content.length(),
                         g_set.allowUnsigned, err, sizeof(err), &info);
  doc["ok"] = ok;
  doc["sigOk"] = info.sigOk;
  doc["keyid"] = info.keyid;
  if (!ok) doc["msg"] = err;
  sendJson(doc);
}

static void hBlockInstall() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false,\"msg\":\"bad request\"}"); return; }
  const char* url = d["url"] | "";
  const char* content = d["content"] | "";
  if (url[0]) {
    if (WiFi.status() != WL_CONNECTED) {
      server.send(200, "application/json", "{\"ok\":false,\"msg\":\"finish the WiFi step first\"}");
      return;
    }
    String body;
    char err[80];
    if (!httpGetText(String(url), body, 8192, err, sizeof(err))) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["msg"] = err;
      sendJson(doc);
      return;
    }
    installFromContent(body);
  } else if (content[0]) {
    installFromContent(String(content));
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"url or content required\"}");
  }
}

static void hBlockRemove() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  bool ok = blockRemove(d["id"] | "");
  server.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"not installed\"}");
}

static void hBlockPolicy() {
  JsonDocument d;
  if (!readBody(d)) { server.send(400, "application/json", "{\"ok\":false}"); return; }
  g_set.allowUnsigned = d["allowUnsigned"] | false;
  settingsSave();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void hRegistry() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json", "{\"ok\":false,\"msg\":\"finish the WiFi step first\"}");
    return;
  }
  String body;
  char err[80];
  JsonDocument doc;
  if (!httpGetText(server.arg("url"), body, REGISTRY_MAX_BYTES, err, sizeof(err))) {
    doc["ok"] = false;
    doc["msg"] = err;
    sendJson(doc);
    return;
  }
  // index may be a signed epb envelope or bare JSON
  String payload;
  EpbInfo info;
  if (epbOpen(body.c_str(), body.length(), payload, info, REGISTRY_MAX_PAYLOAD)) {
    doc["sigOk"] = info.sigOk;
    doc["keyid"] = info.keyid;
  } else if (info.envelope) {
    // A real envelope we could not open. Falling through to the bare-JSON
    // path would parse the ENVELOPE instead of its payload and report the
    // useless "not a block index"; say what actually went wrong.
    doc["ok"] = false;
    doc["msg"] = info.err;
    sendJson(doc);
    return;
  } else {
    payload = body;
    doc["sigOk"] = false;
  }
  JsonDocument idx;
  if (deserializeJson(idx, payload) || strcmp(idx["format"] | "", "epb-index1") != 0) {
    doc["ok"] = false;
    doc["msg"] = "not a block index";
    sendJson(doc);
    return;
  }
  doc["ok"] = true;
  doc["blocks"] = idx["blocks"];
  sendJson(doc);
}

// POST /api/preview — optional body: a layout JSON array. If present it is
// validated and SAVED first, then rendered; so the editor's "preview" always
// shows exactly the canvas being edited, not whatever /layout.json held
// before. (The render path re-reads the file — there is no other way to hand
// it a candidate layout — and silently previewing stale state is precisely
// the bug this prevents.) An empty body just renders the saved layout.
static void hPreview() {
  String body = server.arg("plain");
  bool looksLikeLayout = false;
  for (size_t i = 0; i < body.length(); i++) {
    char c = body[i];
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
    looksLikeLayout = (c == '[');
    break;
  }
  if (looksLikeLayout) {
    char err[80];
    if (!layoutSave(body.c_str(), body.length(), err, sizeof(err))) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["msg"] = err;
      sendJson(doc);
      return;                    // invalid layout: report, don't render stale
    }
  }
  server.send(200, "application/json", "{\"ok\":true,\"msg\":\"rendering (~25 s of flashing)\"}");
  delay(50);
  if (s_previewHook) s_previewHook();
}

// ---------------- firmware update (OTA) ----------------
//
// POST /api/ota (multipart file upload of epaper-dashboard-app.bin) streams
// the image into the SPARE app slot via Update; the boot switch only happens
// after the received bytes pass the optional ?sha256=<hex> check AND the
// image itself verifies (Update.end runs esp_image_verify). The running slot
// is untouched until then — an interrupted or corrupted upload changes
// nothing. Needs a two-slot partition table ("Minimal SPIFFS"); on the old
// single-slot huge_app table we answer with the one-time USB migration hint.
static bool s_otaActive = false, s_otaOk = false;
static char s_otaErr[120] = "";
static size_t s_otaBytes = 0;
static mbedtls_sha256_context s_otaSha;

static void otaShaStart() {
  mbedtls_sha256_init(&s_otaSha);
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_sha256_starts(&s_otaSha, 0);
#else
  mbedtls_sha256_starts_ret(&s_otaSha, 0);
#endif
}
static void otaShaUpdate(const uint8_t* d, size_t n) {
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_sha256_update(&s_otaSha, d, n);
#else
  mbedtls_sha256_update_ret(&s_otaSha, d, n);
#endif
}
static void otaShaFinish(uint8_t out[32]) {
#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_sha256_finish(&s_otaSha, out);
#else
  mbedtls_sha256_finish_ret(&s_otaSha, out);
#endif
  mbedtls_sha256_free(&s_otaSha);
}

static void otaFail(const char* msg) {
  if (!s_otaErr[0]) strlcpy(s_otaErr, msg, sizeof(s_otaErr));
  if (s_otaActive) {
    Update.abort();
    mbedtls_sha256_free(&s_otaSha);
    s_otaActive = false;
  }
  Serial.printf("OTA: FAILED - %s\n", s_otaErr);
}

static void hOtaUpload() {
  touch();
  HTTPUpload& up = server.upload();
  switch (up.status) {
    case UPLOAD_FILE_START: {
      s_otaOk = false;
      s_otaErr[0] = 0;
      s_otaBytes = 0;
      s_otaActive = false;
      const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
      if (!next) {
        otaFail("no spare OTA slot on this partition table - a one-time USB "
                "flash of the new factory/partition image is needed first "
                "(see the FLASHING notes in the CI artifact)");
        return;
      }
      Serial.printf("OTA: receiving '%s' into slot %s (%u bytes free)\n",
                    up.filename.c_str(), next->label, (unsigned)next->size);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        otaFail(Update.errorString());
        return;
      }
      otaShaStart();
      s_otaActive = true;
      break;
    }
    case UPLOAD_FILE_WRITE:
      if (!s_otaActive) return;
      if (Update.write(up.buf, up.currentSize) != up.currentSize) {
        otaFail(Update.errorString());
        return;
      }
      otaShaUpdate(up.buf, up.currentSize);
      s_otaBytes += up.currentSize;
      break;
    case UPLOAD_FILE_END: {
      if (!s_otaActive) return;
      s_otaActive = false;             // teardown handled right here
      uint8_t digest[32];
      otaShaFinish(digest);
      String want = server.arg("sha256");
      want.trim();
      if (!hexDigestMatches(want.c_str(), digest, sizeof(digest))) {
        Update.abort();
        strlcpy(s_otaErr, "SHA-256 mismatch - wrong or corrupted file; "
                          "nothing was changed", sizeof(s_otaErr));
        Serial.printf("OTA: FAILED - %s\n", s_otaErr);
        return;
      }
      if (!Update.end(true)) {         // verifies the image, flips boot slot
        strlcpy(s_otaErr, Update.errorString(), sizeof(s_otaErr));
        Serial.printf("OTA: FAILED - %s\n", s_otaErr);
        return;
      }
      s_otaOk = true;
      Serial.printf("OTA: %u bytes flashed and verified, boot slot switched\n",
                    (unsigned)s_otaBytes);
      break;
    }
    case UPLOAD_FILE_ABORTED:
      otaFail("upload aborted");
      break;
  }
}

static void hOtaFinish() {
  JsonDocument doc;
  doc["ok"] = s_otaOk;
  doc["bytes"] = (uint32_t)s_otaBytes;
  if (s_otaOk) {
    doc["msg"] = "flashed - rebooting into the new firmware";
    rebootRequested = true;
    rebootAt = millis() + 1500;
  } else {
    doc["msg"] = s_otaErr[0] ? s_otaErr : "no file received";
  }
  sendJson(doc);
}

static void hOtaInfo() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = FW_VERSION;
  const esp_partition_t* run = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  doc["running"] = run ? run->label : "?";
  doc["otaReady"] = (next != nullptr);
  if (next) doc["slotBytes"] = (uint32_t)next->size;
  sendJson(doc);
}

// ---------------- main ----------------

static void registerRoutes(bool captive) {
  server.on("/", HTTP_GET, hRoot);
  server.on("/index.html", HTTP_GET, hRoot);
  server.on("/bootstrap.min.css", HTTP_GET, hCss);
  server.on("/app.js", HTTP_GET, hJs);
  server.on("/api/state", HTTP_GET, hState);
  server.on("/api/scan", HTTP_GET, hScan);
  server.on("/api/wifi", HTTP_POST, hWifiJoin);
  server.on("/api/wifi/status", HTTP_GET, hWifiStatus);
  server.on("/api/test/imap", HTTP_POST, hTestImap);
  server.on("/api/imap/detect", HTTP_GET, hImapDetect);
  server.on("/api/caldav/discover", HTTP_POST, hDiscover);
  server.on("/api/test/cal", HTTP_POST, hTestCal);
  server.on("/api/geocode", HTTP_GET, hGeocode);
  server.on("/api/save", HTTP_POST, hSave);
  server.on("/api/finish", HTTP_POST, hFinish);
  server.on("/api/layout", HTTP_GET, hLayoutGet);
  server.on("/api/layout", HTTP_POST, hLayoutPost);
  server.on("/api/blocks", HTTP_GET, hBlocksGet);
  server.on("/api/blocks/install", HTTP_POST, hBlockInstall);
  server.on("/api/blocks/remove", HTTP_POST, hBlockRemove);
  server.on("/api/blocks/policy", HTTP_POST, hBlockPolicy);
  server.on("/api/registry", HTTP_GET, hRegistry);
  server.on("/api/preview", HTTP_POST, hPreview);
  server.on("/api/ota/info", HTTP_GET, hOtaInfo);
  server.on("/api/ota", HTTP_POST, hOtaFinish, hOtaUpload);
  if (captive) {
    // captive-portal probes
    server.on("/generate_204", redirectToPortal);        // Android
    server.on("/gen_204", redirectToPortal);
    server.on("/hotspot-detect.html", redirectToPortal); // Apple
    server.on("/library/test/success.html", redirectToPortal);
    server.on("/ncsi.txt", redirectToPortal);            // Windows
    server.on("/connecttest.txt", redirectToPortal);
    server.on("/redirect", redirectToPortal);
    server.onNotFound(redirectToPortal);
  } else {
    server.onNotFound([]() { touch(); server.send(404, "text/plain", "not found"); });
  }
}

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
  // Settings mode with a stored config: auto-join the home WiFi too, so
  // registry installs / geocoding / tests work without redoing the WiFi step.
  if (g_set.valid()) {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(g_set.ssid, g_set.wifiPass);
    wifiState = WST_CONNECTING;
    wifiT0 = millis();
  }
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  MDNS.begin(DEVICE_HOSTNAME);

  registerRoutes(true);
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

void portalServeLan(uint32_t idleMs, uint32_t maxMs) {
  MDNS.begin(DEVICE_HOSTNAME);
  registerRoutes(false);
  server.begin();
  touch();
  uint32_t start = millis();
  Serial.printf("LAN editor window: http://%s/ (and http://" DEVICE_HOSTNAME ".local/), "
                "closes after %lus idle\n",
                WiFi.localIP().toString().c_str(), (unsigned long)(idleMs / 1000));
  while (millis() - s_lastActivity < idleMs && millis() - start < maxMs) {
    server.handleClient();
    if (rebootRequested && millis() > rebootAt) {
      Serial.println("Portal requested reboot");
      delay(100);
      ESP.restart();
    }
    delay(2);
  }
  server.stop();
  Serial.println("LAN editor window closed");
}
