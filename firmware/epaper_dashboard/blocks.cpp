#include "blocks.h"
#include "net_util.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ---------------- parsing ----------------

static BlockWidget widgetFromName(const char* w) {
  if (!strcmp(w, "big-number")) return BW_BIG_NUMBER;
  if (!strcmp(w, "list")) return BW_LIST;
  if (!strcmp(w, "bar")) return BW_BAR;
  if (!strcmp(w, "clock")) return BW_CLOCK;
  if (!strcmp(w, "date-status")) return BW_DATESTATUS;
  if (!strcmp(w, "weather-now")) return BW_WEATHER_NOW;
  if (!strcmp(w, "forecast")) return BW_FORECAST;
  if (!strcmp(w, "calendar")) return BW_CALENDAR;
  if (!strcmp(w, "inbox")) return BW_INBOX;
  return BW_TEXT;
}

static bool idOk(const char* s) {
  if (!s[0] || strlen(s) > 26) return false;
  for (const char* p = s; *p; p++)
    if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p) && *p != '-') return false;
  return true;
}

bool blockParse(const char* json, size_t len, BlockDef& out, char* err, size_t errLen) {
  out = BlockDef();
  if (len > BLK_MAX_DESC) {
    snprintf(err, errLen, "descriptor too large (%u > %u bytes)", (unsigned)len, BLK_MAX_DESC);
    return false;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json, len);
  if (e) {
    snprintf(err, errLen, "bad JSON: %s", e.c_str());
    return false;
  }
  strlcpy(out.id, doc["id"] | "", sizeof(out.id));
  if (!idOk(out.id)) {
    snprintf(err, errLen, "id must be short lowercase-kebab");
    return false;
  }
  strlcpy(out.name, doc["name"] | out.id, sizeof(out.name));
  strlcpy(out.author, doc["author"] | "unknown", sizeof(out.author));
  strlcpy(out.version, doc["version"] | "0", sizeof(out.version));

  JsonObjectConst src = doc["source"];
  strlcpy(out.srcType, src["type"] | "json", sizeof(out.srcType));
  strlcpy(out.url, src["url"] | "", sizeof(out.url));
  out.builtin = (strcmp(out.srcType, "builtin") == 0);
  if (strcmp(out.srcType, "json") && strcmp(out.srcType, "text") && !out.builtin) {
    snprintf(err, errLen, "source.type must be json|text|builtin");
    return false;
  }
  if (!out.builtin && !out.url[0]) {
    snprintf(err, errLen, "source.url required");
    return false;
  }

  JsonObjectConst rend = doc["render"];
  out.widget = widgetFromName(rend["widget"] | "text");
  if (!out.builtin && out.widget >= BW_CLOCK) {
    snprintf(err, errLen, "builtin widgets need source.type=builtin");
    return false;
  }
  strlcpy(out.title, rend["title"] | "", sizeof(out.title));
  strlcpy(out.wLabel, rend["label"] | "", sizeof(out.wLabel));
  strlcpy(out.wValue, rend["value"] | "", sizeof(out.wValue));
  strlcpy(out.wSub, rend["sub"] | "", sizeof(out.wSub));
  strlcpy(out.wListSrc, rend["list"] | "", sizeof(out.wListSrc));
  out.barMax = rend["max"] | 100.0f;
  out.accentRed = rend["accent"] | false;
  out.minW = rend["minW"] | 2;
  out.minH = rend["minH"] | 2;

  for (JsonObjectConst p : doc["params"].as<JsonArrayConst>()) {
    if (out.nParams >= BLK_MAX_PARAMS) break;
    BlockParam& bp = out.params[out.nParams++];
    strlcpy(bp.key, p["key"] | "", sizeof(bp.key));
    strlcpy(bp.label, p["label"] | bp.key, sizeof(bp.label));
    strlcpy(bp.type, p["type"] | "string", sizeof(bp.type));
    strlcpy(bp.defval, p["default"] | "", sizeof(bp.defval));
    strlcpy(bp.choices, p["choices"] | "", sizeof(bp.choices));
    if (!strcmp(bp.type, "secret")) {   // v1: no secrets in contributed blocks
      snprintf(err, errLen, "secret params are not allowed in blocks");
      return false;
    }
  }

  for (JsonObjectConst x : doc["extract"].as<JsonArrayConst>()) {
    if (out.nExtracts >= BLK_MAX_EXTRACTS) break;
    BlockExtract& ex = out.extracts[out.nExtracts++];
    strlcpy(ex.name, x["name"] | "", sizeof(ex.name));
    strlcpy(ex.path, x["path"] | "", sizeof(ex.path));
    ex.doRound = x["round"] | false;
    ex.mult = x["mult"] | 1.0f;
    strlcpy(ex.prefix, x["prefix"] | "", sizeof(ex.prefix));
    strlcpy(ex.suffix, x["suffix"] | "", sizeof(ex.suffix));
    strlcpy(ex.mapTable, x["map"] | "", sizeof(ex.mapTable));
    strlcpy(ex.fieldPrimary, x["primary"] | "", sizeof(ex.fieldPrimary));
    strlcpy(ex.fieldSecondary, x["secondary"] | "", sizeof(ex.fieldSecondary));
    ex.limit = x["limit"] | 0;
  }
  err[0] = 0;
  return true;
}

// ---------------- JSONPath-lite ----------------

JsonVariantConst blockPath(JsonVariantConst root, const char* path) {
  JsonVariantConst cur = root;
  char seg[48];
  const char* p = path;
  while (*p && !cur.isNull()) {
    size_t n = 0;
    while (*p && *p != '.' && *p != '[' && n + 1 < sizeof(seg)) seg[n++] = *p++;
    seg[n] = 0;
    if (n) {
      bool numeric = true;
      for (size_t i = 0; i < n; i++)
        if (!isdigit((unsigned char)seg[i])) { numeric = false; break; }
      cur = numeric ? cur[atoi(seg)] : cur[seg];
    }
    if (*p == '[') {
      p++;
      int idx = atoi(p);
      while (*p && *p != ']') p++;
      if (*p == ']') p++;
      cur = cur[idx];
    }
    if (*p == '.') p++;
  }
  return cur;
}

// ---------------- transforms + extraction ----------------

static void mapLookup(const char* table, const char* key, char* out, size_t outLen) {
  // table: "0:Clear,1:Cloudy,def:Other"
  size_t klen = strlen(key);
  const char* p = table;
  const char* defv = nullptr;
  while (*p) {
    const char* colon = strchr(p, ':');
    if (!colon) break;
    const char* comma = strchr(colon, ',');
    size_t vlen = comma ? (size_t)(comma - colon - 1) : strlen(colon + 1);
    if ((size_t)(colon - p) == klen && strncmp(p, key, klen) == 0) {
      size_t c = vlen < outLen - 1 ? vlen : outLen - 1;
      memcpy(out, colon + 1, c);
      out[c] = 0;
      return;
    }
    if (colon - p == 3 && strncmp(p, "def", 3) == 0) defv = colon + 1;
    if (!comma) break;
    p = comma + 1;
  }
  if (defv) {
    const char* comma = strchr(defv, ',');
    size_t vlen = comma ? (size_t)(comma - defv) : strlen(defv);
    size_t c = vlen < outLen - 1 ? vlen : outLen - 1;
    memcpy(out, defv, c);
    out[c] = 0;
  } else {
    strlcpy(out, key, outLen);
  }
}

static void scalarToText(const BlockExtract& ex, JsonVariantConst v, char* out, size_t outLen) {
  char raw[40];
  if (v.is<float>() || v.is<double>() || v.is<long>() || v.is<int>()) {
    double d = v.as<double>() * ex.mult;
    if (ex.doRound) snprintf(raw, sizeof(raw), "%ld", lround(d));
    else if (d == (long)d) snprintf(raw, sizeof(raw), "%ld", (long)d);
    else snprintf(raw, sizeof(raw), "%.2f", d);
  } else if (v.is<bool>()) {
    strlcpy(raw, v.as<bool>() ? "yes" : "no", sizeof(raw));
  } else {
    char folded[40];
    utf8ToAscii(v.as<const char*>() ? v.as<const char*>() : "", folded, sizeof(folded));
    strlcpy(raw, folded, sizeof(raw));
  }
  char mapped[40];
  if (ex.mapTable[0]) mapLookup(ex.mapTable, raw, mapped, sizeof(mapped));
  else strlcpy(mapped, raw, sizeof(mapped));
  snprintf(out, outLen, "%s%s%s", ex.prefix, mapped, ex.suffix);
}

void blockApplyExtract(const BlockExtract& ex, JsonVariantConst root, BlockData& out) {
  JsonVariantConst v = blockPath(root, ex.path);
  if (ex.fieldPrimary[0]) {                     // list extraction
    int limit = ex.limit > 0 && ex.limit < 6 ? ex.limit : 6;
    for (JsonVariantConst item : v.as<JsonArrayConst>()) {
      if (out.nRows >= limit || out.nRows >= 6) break;
      BlockListRow& r = out.rows[out.nRows++];
      JsonVariantConst pv = ex.fieldPrimary[0] == '.' ? item : blockPath(item, ex.fieldPrimary);
      char tmp[64];
      utf8ToAscii(pv.as<const char*>() ? pv.as<const char*>() : String(pv.as<double>()).c_str(),
                  tmp, sizeof(tmp));
      strlcpy(r.primary, tmp, sizeof(r.primary));
      r.secondary[0] = 0;
      if (ex.fieldSecondary[0]) {
        JsonVariantConst sv = blockPath(item, ex.fieldSecondary);
        if (sv.is<const char*>()) {
          utf8ToAscii(sv.as<const char*>(), tmp, sizeof(tmp));
          strlcpy(r.secondary, tmp, sizeof(r.secondary));
        } else if (!sv.isNull()) {
          snprintf(r.secondary, sizeof(r.secondary), "%ld", lround(sv.as<double>()));
        }
      }
    }
    return;
  }
  if (out.nValues >= BLK_MAX_EXTRACTS) return;
  BlockValue& bv = out.values[out.nValues++];
  strlcpy(bv.name, ex.name, sizeof(bv.name));
  if (v.isNull()) strlcpy(bv.text, "--", sizeof(bv.text));
  else scalarToText(ex, v, bv.text, sizeof(bv.text));
}

String blockSubstUrl(const BlockDef& def, JsonObjectConst instParams) {
  String url = def.url;
  for (int i = 0; i < def.nParams; i++) {
    const BlockParam& p = def.params[i];
    const char* val = instParams[p.key] | p.defval;
    String needle = String("{") + p.key + "}";
    url.replace(needle, urlEncode(String(val)));
  }
  return url;
}

bool blockUrlAllowed(const String& url, char* err, size_t errLen) {
  String host, path;
  uint16_t port;
  bool tls;
  if (!splitUrl(url, host, port, path, tls) || !tls) {
    strlcpy(err, "block URLs must be https://", errLen);
    return false;
  }
  String h = host;
  h.toLowerCase();
  if (h.endsWith(".local") || h == "localhost") {
    strlcpy(err, "local hosts are not allowed", errLen);
    return false;
  }
  int a, b, c, d;
  if (sscanf(h.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
    bool priv = (a == 10) || (a == 127) || (a == 192 && b == 168) ||
                (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254) || (a == 0);
    if (priv) {
      strlcpy(err, "private/loopback IPs are not allowed", errLen);
      return false;
    }
  }
  return true;
}

void blockTemplate(const char* tmpl, const BlockData& d, char* out, size_t outLen) {
  size_t o = 0;
  for (const char* p = tmpl; *p && o + 1 < outLen;) {
    if (*p == '{') {
      const char* end = strchr(p, '}');
      if (end) {
        char key[20];
        size_t kl = (size_t)(end - p - 1);
        if (kl >= sizeof(key)) kl = sizeof(key) - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        const char* val = "--";
        for (int i = 0; i < d.nValues; i++)
          if (strcmp(d.values[i].name, key) == 0) { val = d.values[i].text; break; }
        while (*val && o + 1 < outLen) out[o++] = *val++;
        p = end + 1;
        continue;
      }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
}

// ---------------- fetch ----------------

bool blockFetch(const BlockDef& def, JsonObjectConst instParams, BlockData& out) {
  out = BlockData();
  if (def.builtin) { out.ok = true; return true; }
  String url = blockSubstUrl(def, instParams);
  if (!blockUrlAllowed(url, out.err, sizeof(out.err))) return false;

  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(cli, url)) {
    strlcpy(out.err, "bad URL", sizeof(out.err));
    return false;
  }
  http.useHTTP10(true);                        // no chunked: getString is bounded
  int code = http.GET();
  if (code != 200) {
    snprintf(out.err, sizeof(out.err), "HTTP %d", code);
    http.end();
    return false;
  }
  int size = http.getSize();
  if (size > BLK_MAX_RESP) {
    strlcpy(out.err, "response too large", sizeof(out.err));
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  if ((int)body.length() > BLK_MAX_RESP) {
    strlcpy(out.err, "response too large", sizeof(out.err));
    return false;
  }

  // params may also appear in extract paths (e.g. CoinGecko keys results
  // by the requested coin id) — substitute before applying
  auto substPath = [&](const BlockExtract& src) {
    BlockExtract ex = src;
    String p = ex.path;
    for (int i = 0; i < def.nParams; i++) {
      const char* val = instParams[def.params[i].key] | def.params[i].defval;
      p.replace(String("{") + def.params[i].key + "}", val);
    }
    strlcpy(ex.path, p.c_str(), sizeof(ex.path));
    return ex;
  };

  if (strcmp(def.srcType, "text") == 0) {
    // wrap plain text as {"text": "..."} so paths/templates work uniformly
    JsonDocument doc;
    doc["text"] = body.substring(0, 300);
    for (int i = 0; i < def.nExtracts; i++) {
      BlockExtract ex = substPath(def.extracts[i]);
      blockApplyExtract(ex, doc.as<JsonVariantConst>(), out);
    }
  } else {
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, body);
    if (e) {
      snprintf(out.err, sizeof(out.err), "bad JSON: %s", e.c_str());
      return false;
    }
    for (int i = 0; i < def.nExtracts; i++) {
      BlockExtract ex = substPath(def.extracts[i]);
      blockApplyExtract(ex, doc.as<JsonVariantConst>(), out);
    }
  }
  out.ok = true;
  return true;
}

void blockSampleData(const BlockDef& def, BlockData& out) {
  out = BlockData();
  out.ok = true;
  for (int i = 0; i < def.nExtracts; i++) {
    const BlockExtract& ex = def.extracts[i];
    if (ex.fieldPrimary[0]) {
      int limit = ex.limit > 0 && ex.limit < 6 ? ex.limit : 4;
      for (int r = 0; r < limit; r++) {
        snprintf(out.rows[out.nRows].primary, sizeof(out.rows[0].primary),
                 "Sample item %d", r + 1);
        snprintf(out.rows[out.nRows].secondary, sizeof(out.rows[0].secondary), "%d", 42 + r);
        out.nRows++;
      }
    } else if (out.nValues < BLK_MAX_EXTRACTS) {
      BlockValue& bv = out.values[out.nValues++];
      strlcpy(bv.name, ex.name, sizeof(bv.name));
      snprintf(bv.text, sizeof(bv.text), "%s42%s", ex.prefix, ex.suffix);
    }
  }
}
