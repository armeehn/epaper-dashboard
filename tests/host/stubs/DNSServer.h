#pragma once
#include <Arduino.h>
#include <IPAddress.h>
enum class DNSReplyCode { NoError, FormError, ServerFailure, NonExistentDomain };
class DNSServer {
 public:
  void setErrorReplyCode(DNSReplyCode c) { (void)c; }
  bool start(uint16_t port, const String& domain, const IPAddress& ip) { (void)port; (void)domain; (void)ip; return true; }
  void stop() {}
  void processNextRequest() {}
};
