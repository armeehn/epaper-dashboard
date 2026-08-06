// Host stub of ESP-IDF OTA partition queries — pretends to be a two-slot
// (min_spiffs-style) table so the OTA code paths compile and take the
// "ready" branch.
#pragma once
#include <stdint.h>

typedef struct {
  uint32_t size;
  char label[17];
} esp_partition_t;

inline const esp_partition_t* esp_ota_get_running_partition() {
  static esp_partition_t p = {0x1E0000, "app0"};
  return &p;
}
inline const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* from) {
  (void)from;
  static esp_partition_t p = {0x1E0000, "app1"};
  return &p;
}
