// Host stub of ESP32 WebServer
#pragma once
#include <Arduino.h>
#include <functional>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_HEAD, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_OPTIONS };
typedef enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED } HTTPUploadStatus;
#define HTTP_UPLOAD_BUFLEN 1436
class HTTPUpload {
 public:
  HTTPUploadStatus status = UPLOAD_FILE_START;
  String filename, name, type;
  size_t totalSize = 0, currentSize = 0;
  uint8_t buf[HTTP_UPLOAD_BUFLEN];
};
class WebServer {
 public:
  typedef std::function<void()> THandlerFunction;
  explicit WebServer(int port = 80) { (void)port; }
  void on(const String& uri, THandlerFunction fn) { (void)uri; (void)fn; }
  void on(const String& uri, HTTPMethod m, THandlerFunction fn) { (void)uri; (void)m; (void)fn; }
  void on(const String& uri, HTTPMethod m, THandlerFunction fn, THandlerFunction ufn) {
    (void)uri; (void)m; (void)fn; (void)ufn;
  }
  HTTPUpload& upload() { static HTTPUpload u; return u; }
  void onNotFound(THandlerFunction fn) { (void)fn; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  void send(int code, const char* type, const String& body) { (void)code; (void)type; (void)body; }
  void send_P(int code, const char* type, const char* content, size_t len) { (void)code; (void)type; (void)content; (void)len; }
  void sendHeader(const String& n, const String& v, bool first = false) { (void)n; (void)v; (void)first; }
  bool hasArg(const String& n) { (void)n; return false; }
  String arg(const String& n) { (void)n; return String(); }
};
