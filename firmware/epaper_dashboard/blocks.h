#pragma once
// Declarative block engine: descriptors are DATA, never code.
// A block declares source (https json/text) + extract (paths & whitelisted
// transforms) + render (fixed widget vocabulary). See DESIGN.md.
#include <Arduino.h>
#include <ArduinoJson.h>

#define BLK_MAX_PARAMS   6
#define BLK_MAX_EXTRACTS 8
#define BLK_MAX_DESC     4096   // descriptor size cap (bytes)
#define BLK_MAX_RESP     16384  // fetched response cap (bytes)
#define BLK_MAX_INSTALLED 16

// Widget kinds a contributed block may use (built-ins have their own).
enum BlockWidget : uint8_t {
  BW_BIG_NUMBER, BW_LIST, BW_TEXT, BW_BAR,
  // built-ins (native renderers):
  BW_CLOCK, BW_DATESTATUS, BW_WEATHER_NOW, BW_FORECAST, BW_CALENDAR, BW_INBOX
};

struct BlockParam {
  char key[16];
  char label[28];
  char type[8];       // "string" | "number" | "choice"
  char defval[40];
  char choices[96];   // comma-separated for "choice"
};

struct BlockExtract {
  char name[16];
  char path[64];      // dotted path: a.b[0].c  (or a.0.c)
  // whitelisted transforms (applied in this order):
  bool doRound = false;
  float mult = 1.0f;
  char prefix[8] = "";
  char suffix[12] = "";
  char mapTable[160] = "";  // "0:Clear,1:Cloudy,..." value->label
  // list extraction (path points at an array):
  char fieldPrimary[24] = "";
  char fieldSecondary[24] = "";
  int limit = 0;
};

struct BlockDef {
  char id[28];
  char name[32];
  char author[28];
  char version[10];
  // source
  char srcType[10];   // "json" | "text" | "builtin"
  char url[200];      // may contain {param} placeholders
  // render
  BlockWidget widget = BW_TEXT;
  char title[28];     // frame title ("" = none)
  char wLabel[28];    // big-number label / bar label
  char wValue[20];    // "{name}" binding for big-number/bar value
  char wSub[40];      // sub-line template, may mix text + {bindings}
  char wListSrc[16];  // extract name feeding BW_LIST
  float barMax = 100;
  bool accentRed = false;
  uint8_t minW = 2, minH = 2;
  // params & extracts
  int nParams = 0;
  BlockParam params[BLK_MAX_PARAMS];
  int nExtracts = 0;
  BlockExtract extracts[BLK_MAX_EXTRACTS];
  bool builtin = false;
};

// Extracted, transformed data ready for the widget renderer.
struct BlockValue {
  char name[16];
  char text[48];            // scalar rendering
};
struct BlockListRow {
  char primary[64];
  char secondary[20];
};
struct BlockData {
  bool ok = false;
  char err[64] = "";
  int nValues = 0;
  BlockValue values[BLK_MAX_EXTRACTS];
  int nRows = 0;
  BlockListRow rows[6];
};

// ---- pure functions (host-testable) ----
bool blockParse(const char* json, size_t len, BlockDef& out, char* err, size_t errLen);
// JSONPath-lite: walk doc by "a.b[2].c". Returns null variant if absent.
JsonVariantConst blockPath(JsonVariantConst root, const char* path);
// Apply one extract (incl. transforms / list fields) to a parsed doc.
void blockApplyExtract(const BlockExtract& ex, JsonVariantConst root, BlockData& out);
// Substitute {param} placeholders using an instance's param values (JSON obj).
String blockSubstUrl(const BlockDef& def, JsonObjectConst instParams);
// SSRF guard: https-only, no literal private/loopback IPs, no *.local.
bool blockUrlAllowed(const String& url, char* err, size_t errLen);
// Template "{x} text {y}" -> resolved via values in data.
void blockTemplate(const char* tmpl, const BlockData& d, char* out, size_t outLen);
// True when every char of s has a glyph in the big-number font ('-' '.' '/'
// digits ':'). Anything else measures zero width there and draws blank.
bool blockBigNumDrawable(const char* s);
// Rows a list frame with room for `avail` rows should draw: all of them, or
// avail-1 plus one "+N more" marker. *hidden = N (0 when everything fits).
int blockListVisible(int nRows, int avail, int* hidden);

// ---- device fetch (HTTPClient; stubbed on host) ----
bool blockFetch(const BlockDef& def, JsonObjectConst instParams, BlockData& out);
// Fill BlockData with plausible sample values (portal preview / no-net).
void blockSampleData(const BlockDef& def, BlockData& out);
