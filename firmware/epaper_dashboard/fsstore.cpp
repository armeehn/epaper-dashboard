#include "fsstore.h"
#include "config.h"
#include <memory>
#include <new>
#include <LittleFS.h>

static bool s_fsOk = false;

bool fsStoreBegin() {
  if (s_fsOk) return true;
  s_fsOk = LittleFS.begin(true);   // format on first mount
  // Block descriptors live in /b/. Unlike SPIFFS, LittleFS has real
  // directories and will not create a missing parent on open-for-write, so
  // make it once here; mkdir on an existing directory is a no-op.
  if (s_fsOk) LittleFS.mkdir("/b");
  return s_fsOk;
}

static String readFileStr(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

static bool writeFileStr(const char* path, const char* data, size_t len) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)data, len);
  f.close();
  return n == len;
}

// ---------------- layout ----------------

// Classic screen as the default layout, in GRID_COLS x GRID_ROWS cells
// (50 x 40 px each on an 800x480 panel, scaled on any other).
const char* layoutDefaultJson() {
  return
    "[{\"inst\":\"clk\",\"block\":\"core-clock\",\"x\":0,\"y\":0,\"w\":7,\"h\":3},"
    "{\"inst\":\"dat\",\"block\":\"core-datestatus\",\"x\":7,\"y\":0,\"w\":9,\"h\":3},"
    "{\"inst\":\"wx\",\"block\":\"core-weather\",\"x\":0,\"y\":3,\"w\":6,\"h\":5},"
    "{\"inst\":\"fc\",\"block\":\"core-forecast\",\"x\":0,\"y\":8,\"w\":6,\"h\":4},"
    "{\"inst\":\"cal\",\"block\":\"core-calendar\",\"x\":6,\"y\":3,\"w\":10,\"h\":5},"
    "{\"inst\":\"inb\",\"block\":\"core-inbox\",\"x\":6,\"y\":8,\"w\":10,\"h\":4}]";
}

static bool isBuiltinId(const char* id) {
  return !strcmp(id, "core-clock") || !strcmp(id, "core-datestatus") ||
         !strcmp(id, "core-weather") || !strcmp(id, "core-forecast") ||
         !strcmp(id, "core-calendar") || !strcmp(id, "core-inbox");
}

bool layoutLoad(JsonDocument& doc) {
  String s;
  if (fsStoreBegin()) s = readFileStr("/layout.json");
  if (!s.length()) s = layoutDefaultJson();
  if (deserializeJson(doc, s) != DeserializationError::Ok || !doc.is<JsonArray>()) {
    deserializeJson(doc, layoutDefaultJson());
  }
  return true;
}

bool layoutSave(const char* json, size_t len, char* err, size_t errLen) {
  if (len > 6000) {
    strlcpy(err, "layout too large", errLen);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, json, len) || !doc.is<JsonArray>()) {
    strlcpy(err, "layout must be a JSON array", errLen);
    return false;
  }
  JsonDocument idx;
  blocksList(idx);
  int count = 0;
  for (JsonObjectConst it : doc.as<JsonArrayConst>()) {
    if (++count > 20) {
      strlcpy(err, "too many blocks (max 20)", errLen);
      return false;
    }
    int x = it["x"] | -1, y = it["y"] | -1, w = it["w"] | 0, h = it["h"] | 0;
    const char* id = it["block"] | "";
    if (x < 0 || y < 0 || w < 1 || h < 1 || x + w > GRID_COLS || y + h > GRID_ROWS) {
      snprintf(err, errLen, "block '%s' out of the %dx%d grid", id, GRID_COLS, GRID_ROWS);
      return false;
    }
    if (!isBuiltinId(id)) {
      bool found = false;
      for (JsonObjectConst b : idx.as<JsonArrayConst>())
        if (!strcmp(b["id"] | "", id)) { found = true; break; }
      if (!found) {
        snprintf(err, errLen, "block '%s' is not installed", id);
        return false;
      }
    }
  }
  if (!fsStoreBegin() || !writeFileStr("/layout.json", json, len)) {
    strlcpy(err, "flash write failed", errLen);
    return false;
  }
  err[0] = 0;
  return true;
}

// ---------------- installed blocks ----------------

bool blocksList(JsonDocument& doc) {
  String s;
  if (fsStoreBegin()) s = readFileStr("/b/index.json");
  if (!s.length() || deserializeJson(doc, s) != DeserializationError::Ok || !doc.is<JsonArray>()) {
    doc.to<JsonArray>();
  }
  return true;
}

static bool indexWrite(JsonDocument& doc) {
  String s;
  serializeJson(doc, s);
  return writeFileStr("/b/index.json", s.c_str(), s.length());
}

bool blockInstall(const char* epbJson, size_t len, bool allowUnsigned,
                  char* err, size_t errLen, EpbInfo* infoOut) {
  if (!fsStoreBegin()) {
    strlcpy(err, "filesystem unavailable", errLen);
    return false;
  }
  String payload;
  EpbInfo info;
  bool isEnvelope = epbOpen(epbJson, len, payload, info, BLK_MAX_DESC);
  if (!isEnvelope) {
    if (info.envelope) {   // an .epb we could not open — don't parse it as one
      if (infoOut) *infoOut = info;
      strlcpy(err, info.err, errLen);
      return false;
    }
    // maybe a bare block.json (unsigned)
    payload = String();
    payload.reserve(len);
    for (size_t i = 0; i < len; i++) payload += epbJson[i];
    info = EpbInfo();
  }
  if (infoOut) *infoOut = info;

  if (!info.sigOk) {
    if (info.sigPresent) {
      snprintf(err, errLen, "signature check failed: %s", info.err);
      return false;
    }
    if (!allowUnsigned) {
      strlcpy(err, "unsigned block refused (enable 'allow unsigned' to override)", errLen);
      return false;
    }
  }

  // Heap, not stack: a BlockDef is ~4 KB and this runs inside a portal request
  // handler on the 8 KB Arduino loop task (see tests/host/run_tests.sh).
  std::unique_ptr<BlockDef> defp(new (std::nothrow) BlockDef());
  if (!defp) {
    strlcpy(err, "out of memory", errLen);
    return false;
  }
  BlockDef& def = *defp;
  if (!blockParse(payload.c_str(), payload.length(), def, err, errLen)) return false;
  if (def.builtin) {
    strlcpy(err, "builtin ids are reserved", errLen);
    return false;
  }
  // Validate the URL template: params may only appear in path/query, never
  // in the host, so replace placeholders with 'x' and run the SSRF checks on
  // the result. (Re-checked with real values at every fetch too.)
  {
    String u = def.url;
    int b1;
    while ((b1 = u.indexOf('{')) >= 0) {
      int b2 = u.indexOf('}', b1);
      if (b2 < 0) break;
      u = u.substring(0, b1) + "x" + u.substring(b2 + 1);
    }
    int scheme = u.indexOf("://");
    int hostEnd = u.indexOf('/', scheme >= 0 ? scheme + 3 : 0);
    String hostPart = hostEnd > 0 ? u.substring(0, hostEnd) : u;
    if (String(def.url).indexOf('{') >= 0 &&
        String(def.url).substring(0, hostPart.length()).indexOf('{') >= 0) {
      strlcpy(err, "params are not allowed in the URL host", errLen);
      return false;
    }
    if (!blockUrlAllowed(u, err, errLen)) return false;
  }

  JsonDocument idx;
  blocksList(idx);
  int n = 0;
  for (JsonObjectConst b : idx.as<JsonArrayConst>()) {
    (void)b;
    n++;
  }
  bool replacing = false;
  for (JsonObject b : idx.as<JsonArray>())
    if (!strcmp(b["id"] | "", def.id)) {
      replacing = true;
      b["name"] = def.name; b["author"] = def.author; b["version"] = def.version;
      b["sigOk"] = info.sigOk; b["keyid"] = info.keyid;
    }
  if (!replacing) {
    if (n >= BLK_MAX_INSTALLED) {
      strlcpy(err, "too many installed blocks", errLen);
      return false;
    }
    JsonObject b = idx.as<JsonArray>().add<JsonObject>();
    b["id"] = def.id; b["name"] = def.name; b["author"] = def.author;
    b["version"] = def.version; b["sigOk"] = info.sigOk; b["keyid"] = info.keyid;
  }

  char path[48];
  snprintf(path, sizeof(path), "/b/%s.json", def.id);
  if (!writeFileStr(path, payload.c_str(), payload.length()) || !indexWrite(idx)) {
    strlcpy(err, "flash write failed", errLen);
    return false;
  }
  err[0] = 0;
  return true;
}

bool blockRemove(const char* id) {
  if (!fsStoreBegin()) return false;
  JsonDocument idx;
  blocksList(idx);
  JsonDocument out;
  JsonArray arr = out.to<JsonArray>();
  bool removed = false;
  for (JsonObjectConst b : idx.as<JsonArrayConst>()) {
    if (!strcmp(b["id"] | "", id)) {
      removed = true;
      continue;
    }
    arr.add(b);
  }
  if (removed) {
    char path[48];
    snprintf(path, sizeof(path), "/b/%s.json", id);
    LittleFS.remove(path);
    indexWrite(out);
  }
  return removed;
}

bool blockLoadDef(const char* id, BlockDef& def) {
  if (!fsStoreBegin()) return false;
  char path[48];
  snprintf(path, sizeof(path), "/b/%s.json", id);
  String s = readFileStr(path);
  if (!s.length()) return false;
  char err[80];
  return blockParse(s.c_str(), s.length(), def, err, sizeof(err));
}
