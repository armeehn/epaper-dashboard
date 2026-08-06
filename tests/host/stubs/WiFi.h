// Host-compile stub of ESP32 WiFi API
#pragma once
#include <Arduino.h>
#include <IPAddress.h>
typedef enum { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4, WL_DISCONNECTED = 6 } wl_status_t;
typedef enum { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 } wifi_mode_t;
typedef enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK } wifi_auth_mode_t;
class WiFiClass {
 public:
  bool mode(wifi_mode_t m) { (void)m; return true; }
  void persistent(bool p) { (void)p; }
  void begin(const char* ssid, const char* pass) { (void)ssid; (void)pass; }
  wl_status_t status() { return WL_CONNECTED; }
  bool disconnect(bool wifioff = false) { (void)wifioff; return true; }
  int32_t RSSI() { return -50; }
  int32_t RSSI(int i) { (void)i; return -50; }
  IPAddress localIP() { return IPAddress(192, 168, 1, 2); }
  bool softAP(const char* ssid, const char* pass = nullptr) { (void)ssid; (void)pass; return true; }
  IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
  int16_t scanNetworks(bool async = false, bool hidden = false) { (void)async; (void)hidden; return 0; }
  void scanDelete() {}
  String SSID(int i) { (void)i; return String(); }
  wifi_auth_mode_t encryptionType(int i) { (void)i; return WIFI_AUTH_OPEN; }
  bool config(IPAddress ip, IPAddress gw = IPAddress(), IPAddress sn = IPAddress(),
              IPAddress d1 = IPAddress(), IPAddress d2 = IPAddress()) {
    (void)ip; (void)gw; (void)sn; (void)d1; (void)d2; return true;
  }
  bool softAPConfig(IPAddress ip, IPAddress gw, IPAddress sn) { (void)ip; (void)gw; (void)sn; return true; }
};
#ifndef INADDR_NONE
#define INADDR_NONE IPAddress(255, 255, 255, 255)
#endif
inline WiFiClass WiFi;
