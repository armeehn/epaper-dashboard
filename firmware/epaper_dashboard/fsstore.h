#pragma once
// LittleFS-backed storage for the layout and installed blocks.
// Installed-block index kept in /b/index.json (no directory iteration needed).
#include <Arduino.h>
#include <ArduinoJson.h>
#include "blocks.h"
#include "blocksig.h"

bool fsStoreBegin();

// ---- layout ----
// Load layout JSON (array of {inst,block,x,y,w,h,params{}}) into doc.
// Falls back to the built-in default (classic screen) if none stored.
bool layoutLoad(JsonDocument& doc);
// Validate + persist. err filled on failure.
bool layoutSave(const char* json, size_t len, char* err, size_t errLen);
const char* layoutDefaultJson();

// ---- installed blocks ----
// index doc: [{id,name,author,version,sigOk,keyid}]
bool blocksList(JsonDocument& doc);
// Verify envelope per policy, parse descriptor, persist. err on failure.
bool blockInstall(const char* epbJson, size_t len, bool allowUnsigned,
                  char* err, size_t errLen, EpbInfo* infoOut = nullptr);
bool blockRemove(const char* id);
// Load an installed block's descriptor. Returns false if absent/corrupt.
bool blockLoadDef(const char* id, BlockDef& def);
