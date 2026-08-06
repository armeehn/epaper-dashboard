#pragma once
#include <Arduino.h>
class Preferences {
 public:
  bool begin(const char* ns, bool readOnly = false) { (void)ns; (void)readOnly; return true; }
  void end() {}
  bool clear() { return true; }
  size_t putString(const char* key, const String& v) { (void)key; return v.length(); }
  String getString(const char* key, const String& def = String()) { (void)key; return def; }
};
