#pragma once
#include <Arduino.h>
#include <Stream.h>
#include "WiFiClientSecure.h"
typedef enum { HTTPC_DISABLE_FOLLOW_REDIRECTS, HTTPC_STRICT_FOLLOW_REDIRECTS, HTTPC_FORCE_FOLLOW_REDIRECTS } followRedirects_t;
class HTTPClient {
 public:
  bool begin(WiFiClient& client, const String& url) { (void)client; (void)url; return true; }
  void end() {}
  int GET() { return 200; }
  int sendRequest(const char* method, uint8_t* payload, size_t size) { (void)method; (void)payload; (void)size; return 200; }
  String getString() { return String(""); }
  int getSize() { return 0; }
  void setTimeout(uint16_t t) { (void)t; }
  void setConnectTimeout(int32_t t) { (void)t; }
  void setFollowRedirects(followRedirects_t f) { (void)f; }
  void addHeader(const String& n, const String& v) { (void)n; (void)v; }
  void collectHeaders(const char* keys[], size_t n) { (void)keys; (void)n; }
  String header(const char* name) { (void)name; return String(); }
  void setAuthorization(const char* u, const char* p) { (void)u; (void)p; }
  void useHTTP10(bool b = true) { (void)b; }
  int writeToStream(Stream* s) { (void)s; return 0; }
};
