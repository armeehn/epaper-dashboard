#pragma once
// Small parsing/encoding helpers shared by IMAP, CalDAV/ICS and the portal.
// All functions here are pure (no networking) so they can be unit-tested on host.
#include <Arduino.h>
#include <time.h>

// --- strings/encodings ---
String urlEncode(const String& s);
void   xmlUnescape(String& s);                     // &amp; &lt; &gt; &quot; &apos; &#13; &#xD;
void   icalUnescapeInto(const char* src, char* dst, size_t dstLen);  // \\ \; \, \n
int    b64decode(const char* in, int inLen, uint8_t* out, int outCap);
// Decode RFC2047 encoded-words (=?charset?B/Q?...?=) and fold UTF-8 to ASCII.
void   rfc2047ToAscii(const char* src, char* dst, size_t dstLen);
// Fold UTF-8 to printable ASCII (Latin-1 accents transliterated, rest '?').
void   utf8ToAscii(const char* src, char* dst, size_t dstLen);

// --- time ---
time_t timegmCivil(int y, int mo, int d, int h, int mi, int s);  // UTC fields -> epoch
// "Tue, 22 Jul 2026 07:45:12 -0700" (RFC 2822) -> epoch; 0 on failure
time_t rfc2822ToEpoch(const char* s);
// ICS stamp "20260722T140000[Z]" or "20260722" -> epoch.
// naive/floating stamps are interpreted in the CURRENT local timezone (TZ env).
// isDateOnly set for VALUE=DATE stamps (midnight local).
time_t icsStampToEpoch(const char* s, bool* isDateOnly, bool* isUtc);

// --- URLs ---
// Split "https://host[:port]/path" -> parts. Returns false if not http(s).
bool splitUrl(const String& url, String& host, uint16_t& port, String& path, bool& tls);
// Resolve an href (absolute URL or absolute path) against a base URL.
String resolveHref(const String& baseUrl, const String& href);
