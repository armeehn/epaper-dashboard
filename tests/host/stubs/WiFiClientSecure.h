#pragma once
#include <Arduino.h>
#include <Print.h>
class WiFiClient : public Print {
 public:
  virtual ~WiFiClient() {}
  size_t write(uint8_t c) override { (void)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override { (void)b; return n; }
  bool connect(const char* host, uint16_t port) { (void)host; (void)port; return false; }
  bool connected() { return false; }
  int available() { return 0; }
  int read() { return -1; }
  int read(uint8_t* buf, size_t n) { (void)buf; (void)n; return -1; }
  void stop() {}
  void setTimeout(uint32_t t) { (void)t; }
};
class WiFiClientSecure : public WiFiClient {
 public:
  void setInsecure() {}
};
