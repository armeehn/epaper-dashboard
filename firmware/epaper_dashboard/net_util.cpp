#include "net_util.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

String urlEncode(const String& s) {
  String o;
  o.reserve(s.length() * 3);
  const char* hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else { o += '%'; o += hex[(c >> 4) & 15]; o += hex[c & 15]; }
  }
  return o;
}

void xmlUnescape(String& s) {
  s.replace("&#13;", "");
  s.replace("&#xD;", "");
  s.replace("&#10;", "\n");
  s.replace("&#xA;", "\n");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&apos;", "'");
  s.replace("&amp;", "&");   // last!
}

void icalUnescapeInto(const char* src, char* dst, size_t dstLen) {
  size_t o = 0;
  for (size_t i = 0; src[i] && o + 1 < dstLen; i++) {
    if (src[i] == '\\' && src[i + 1]) {
      i++;
      char c = src[i];
      if (c == 'n' || c == 'N') dst[o++] = ' ';
      else dst[o++] = c;                       // handles backslash, comma, semicolon
    } else {
      dst[o++] = src[i];
    }
  }
  dst[o] = 0;
}

static int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

int b64decode(const char* in, int inLen, uint8_t* out, int outCap) {
  int acc = 0, bits = 0, o = 0;
  for (int i = 0; i < inLen; i++) {
    int v = b64val(in[i]);
    if (v < 0) continue;               // skip '=', whitespace
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (o < outCap) out[o++] = (acc >> bits) & 0xFF;
    }
  }
  return o;
}

void utf8ToAscii(const char* src, char* dst, size_t dstLen) {
  // transliterate common Latin-1 letters (0xC3 x / 0xC2 x sequences), '?' others
  size_t o = 0;
  for (size_t i = 0; src[i] && o + 1 < dstLen;) {
    uint8_t c = (uint8_t)src[i];
    if (c < 0x80) { if (c == '\r' || c == '\n' || c == '\t') { dst[o++] = ' '; } else dst[o++] = (char)c; i++; continue; }
    if (c == 0xC2 && src[i + 1]) { i += 2; dst[o++] = ' '; continue; }        // nbsp & friends
    if (c == 0xC3 && src[i + 1]) {
      uint8_t d = (uint8_t)src[i + 1];
      char r = '?';
      if ((d >= 0x80 && d <= 0x85) || (d >= 0xA0 && d <= 0xA5)) r = (d < 0xA0) ? 'A' : 'a';
      else if (d == 0x87 || d == 0xA7) r = (d == 0x87) ? 'C' : 'c';
      else if ((d >= 0x88 && d <= 0x8B) || (d >= 0xA8 && d <= 0xAB)) r = (d < 0xA0) ? 'E' : 'e';
      else if ((d >= 0x8C && d <= 0x8F) || (d >= 0xAC && d <= 0xAF)) r = (d < 0xA0) ? 'I' : 'i';
      else if (d == 0x91 || d == 0xB1) r = (d == 0x91) ? 'N' : 'n';
      else if ((d >= 0x92 && d <= 0x96) || d == 0x98 || (d >= 0xB2 && d <= 0xB6) || d == 0xB8) r = (d < 0xA0) ? 'O' : 'o';
      else if ((d >= 0x99 && d <= 0x9C) || (d >= 0xB9 && d <= 0xBC)) r = (d < 0xA0) ? 'U' : 'u';
      else if (d == 0x9F) r = 's';
      else if (d == 0xBF) r = 'y';
      dst[o++] = r;
      i += 2;
      continue;
    }
    // other multi-byte lead: skip sequence, emit one '?'
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
    for (int k = 0; k < len && src[i]; k++) i++;
    dst[o++] = '?';
  }
  dst[o] = 0;
  // trim trailing spaces
  while (o > 0 && dst[o - 1] == ' ') dst[--o] = 0;
}

static int qpVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

void rfc2047ToAscii(const char* src, char* dst, size_t dstLen) {
  // Decode any =?charset?B|Q?data?= words into a temp UTF-8 buffer, then fold.
  char tmp[256];
  size_t o = 0;
  const char* p = src;
  bool lastWasWord = false;
  while (*p && o + 1 < sizeof(tmp)) {
    const char* w = strstr(p, "=?");
    if (!w) break;
    // copy text before the word (unless it's just whitespace between two words)
    const char* q1 = strchr(w + 2, '?');
    const char* q2 = q1 ? strchr(q1 + 1, '?') : nullptr;
    const char* end = q2 ? strstr(q2 + 1, "?=") : nullptr;
    if (!q1 || !q2 || !end) break;
    bool gapIsSpace = true;
    for (const char* t = p; t < w; t++) if (!isspace((unsigned char)*t)) { gapIsSpace = false; break; }
    if (!(lastWasWord && gapIsSpace)) {
      for (const char* t = p; t < w && o + 1 < sizeof(tmp); t++) tmp[o++] = *t;
    }
    char enc = toupper((unsigned char)q1[1]);
    const char* data = q2 + 1;
    int dataLen = (int)(end - data);
    if (enc == 'B') {
      uint8_t buf[192];
      int n = b64decode(data, dataLen, buf, sizeof(buf) - 1);
      for (int i = 0; i < n && o + 1 < sizeof(tmp); i++) tmp[o++] = (char)buf[i];
    } else if (enc == 'Q') {
      for (int i = 0; i < dataLen && o + 1 < sizeof(tmp); i++) {
        char c = data[i];
        if (c == '_') tmp[o++] = ' ';
        else if (c == '=' && i + 2 < dataLen) {
          int hi = qpVal(data[i + 1]), lo = qpVal(data[i + 2]);
          if (hi >= 0 && lo >= 0) { tmp[o++] = (char)((hi << 4) | lo); i += 2; }
        } else tmp[o++] = c;
      }
    }
    lastWasWord = true;
    p = end + 2;
  }
  while (*p && o + 1 < sizeof(tmp)) tmp[o++] = *p++;
  tmp[o] = 0;
  utf8ToAscii(tmp, dst, dstLen);
}

// days-from-civil (Howard Hinnant), no TZ involvement
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

time_t timegmCivil(int y, int mo, int d, int h, int mi, int s) {
  return (time_t)daysFromCivil(y, mo, d) * 86400 + h * 3600 + mi * 60 + s;
}

time_t rfc2822ToEpoch(const char* s) {
  // optional "Day, " prefix
  const char* p = strchr(s, ',');
  p = p ? p + 1 : s;
  while (*p == ' ') p++;
  int d = atoi(p);
  if (d <= 0) return 0;
  while (*p && *p != ' ') p++;
  while (*p == ' ') p++;
  static const char* mons = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int mo = 0;
  for (int i = 0; i < 12; i++)
    if (strncasecmp(p, mons + i * 3, 3) == 0) { mo = i + 1; break; }
  if (!mo) return 0;
  while (*p && *p != ' ') p++;
  while (*p == ' ') p++;
  int y = atoi(p);
  if (y < 100) y += 2000;
  while (*p && *p != ' ') p++;
  while (*p == ' ') p++;
  int hh = 0, mm = 0, ss = 0;
  if (sscanf(p, "%d:%d:%d", &hh, &mm, &ss) < 2) return 0;
  // timezone
  const char* z = strchr(p, ' ');
  long off = 0;
  if (z) {
    while (*z == ' ') z++;
    if (*z == '+' || *z == '-') {
      int sign = (*z == '-') ? -1 : 1;
      int zh = (z[1] - '0') * 10 + (z[2] - '0');
      int zm = (z[3] - '0') * 10 + (z[4] - '0');
      off = sign * (zh * 3600L + zm * 60L);
    } else if (strncmp(z, "GMT", 3) == 0 || strncmp(z, "UT", 2) == 0) {
      off = 0;
    } else if (strncmp(z, "EDT", 3) == 0) off = -4 * 3600L;
    else if (strncmp(z, "EST", 3) == 0 || strncmp(z, "CDT", 3) == 0) off = -5 * 3600L;
    else if (strncmp(z, "CST", 3) == 0 || strncmp(z, "MDT", 3) == 0) off = -6 * 3600L;
    else if (strncmp(z, "MST", 3) == 0 || strncmp(z, "PDT", 3) == 0) off = -7 * 3600L;
    else if (strncmp(z, "PST", 3) == 0) off = -8 * 3600L;
  }
  return timegmCivil(y, mo, d, hh, mm, ss) - off;
}

time_t icsStampToEpoch(const char* s, bool* isDateOnly, bool* isUtc) {
  size_t len = strlen(s);
  if (isDateOnly) *isDateOnly = false;
  if (isUtc) *isUtc = false;
  if (len < 8) return 0;
  char buf[5];
  auto num = [&](int off, int n) { memcpy(buf, s + off, n); buf[n] = 0; return atoi(buf); };
  int y = num(0, 4), mo = num(4, 2), d = num(6, 2);
  if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  if (len == 8) {                       // VALUE=DATE — local midnight
    if (isDateOnly) *isDateOnly = true;
    struct tm t = {};
    t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
    t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0; t.tm_isdst = -1;
    return mktime(&t);
  }
  if (len < 15 || s[8] != 'T') return 0;
  int h = num(9, 2), mi = num(11, 2), ss = num(13, 2);
  if (len >= 16 && s[15] == 'Z') {      // UTC
    if (isUtc) *isUtc = true;
    return timegmCivil(y, mo, d, h, mi, ss);
  }
  struct tm t = {};                     // floating/TZID -> current local tz
  t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
  t.tm_hour = h; t.tm_min = mi; t.tm_sec = ss; t.tm_isdst = -1;
  return mktime(&t);
}

bool splitUrl(const String& url, String& host, uint16_t& port, String& path, bool& tls) {
  String u = url;
  u.trim();
  if (u.startsWith("https://")) { tls = true; u = u.substring(8); port = 443; }
  else if (u.startsWith("http://")) { tls = false; u = u.substring(7); port = 80; }
  else return false;
  int slash = u.indexOf('/');
  String hostport = (slash < 0) ? u : u.substring(0, slash);
  path = (slash < 0) ? "/" : u.substring(slash);
  int colon = hostport.indexOf(':');
  if (colon >= 0) {
    host = hostport.substring(0, colon);
    port = (uint16_t)hostport.substring(colon + 1).toInt();
  } else {
    host = hostport;
  }
  return host.length() > 0;
}

String resolveHref(const String& baseUrl, const String& href) {
  if (href.startsWith("http://") || href.startsWith("https://")) return href;
  String host, path;
  uint16_t port;
  bool tls;
  if (!splitUrl(baseUrl, host, port, path, tls)) return href;
  String origin = String(tls ? "https://" : "http://") + host;
  bool defPort = (tls && port == 443) || (!tls && port == 80);
  if (!defPort) origin += ":" + String(port);
  if (href.startsWith("/")) return origin + href;
  if (!path.endsWith("/")) {
    int cut = path.lastIndexOf('/');
    path = (cut >= 0) ? path.substring(0, cut + 1) : "/";
  }
  return origin + path + href;
}

bool hexDigestMatches(const char* expectedHex, const uint8_t* digest, size_t digestLen) {
  if (!expectedHex || !expectedHex[0]) return true;   // check is optional
  if (strlen(expectedHex) != digestLen * 2) return false;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < digestLen; i++) {
    int hi = nib(expectedHex[2 * i]), lo = nib(expectedHex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    if ((uint8_t)((hi << 4) | lo) != digest[i]) return false;
  }
  return true;
}

bool mailDomainAllowed(const String& d) {
  if (d.length() < 3 || d.length() > 63 || d.indexOf('.') < 0) return false;
  if (d.endsWith(".local") || d == "localhost") return false;
  bool allDigitsAndDots = true;
  for (size_t i = 0; i < d.length(); i++) {
    char c = d[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
    if (!ok) return false;
    if (!isdigit((unsigned char)c) && c != '.') allDigitsAndDots = false;
  }
  return !allDigitsAndDots;   // reject bare IPv4 literals
}
