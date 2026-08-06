// Host stub of ESP32 mDNS responder
#pragma once
#include <Arduino.h>
class MDNSResponder {
 public:
  bool begin(const char* hostname) { (void)hostname; return true; }
  void addService(const char* s, const char* p, uint16_t port) { (void)s; (void)p; (void)port; }
  void end() {}
};
inline MDNSResponder MDNS;
