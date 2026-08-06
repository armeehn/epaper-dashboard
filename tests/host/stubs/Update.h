// Host stub of the ESP32 Update (OTA flash) API — accepts everything.
#pragma once
#include <Arduino.h>

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
 public:
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN) { (void)size; return true; }
  size_t write(uint8_t* data, size_t len) { (void)data; return len; }
  bool end(bool evenIfRemaining = false) { (void)evenIfRemaining; return true; }
  void abort() {}
  bool hasError() { return false; }
  const char* errorString() { return "no error"; }
};
inline UpdateClass Update;
