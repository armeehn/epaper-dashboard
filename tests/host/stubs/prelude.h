// Forced-include prelude: ESP32-isms for host compile check
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define RTC_DATA_ATTR
static inline size_t host_strlcpy(char* dst, const char* src, size_t size) {
  size_t len = strlen(src);
  if (size) { size_t n = (len >= size) ? size - 1 : len; memcpy(dst, src, n); dst[n] = 0; }
  return len;
}
#define strlcpy host_strlcpy
static inline int esp_sleep_enable_timer_wakeup(uint64_t us) { (void)us; return 0; }
static inline void esp_deep_sleep_start() {}
#include <stdio.h>
