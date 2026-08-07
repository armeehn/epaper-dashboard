// Host stub of LittleFS backed by /tmp/fsroot (real files).
#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>

class File : public Print {
 public:
  FILE* f = nullptr;
  File() {}
  explicit File(FILE* fp) : f(fp) {}
  operator bool() const { return f != nullptr; }
  size_t write(uint8_t c) override { return f ? fwrite(&c, 1, 1, f) : 0; }
  size_t write(const uint8_t* b, size_t n) override { return f ? fwrite(b, 1, n, f) : 0; }
  String readString() {
    String s;
    if (!f) return s;
    char buf[256];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
      for (size_t i = 0; i < n; i++) s += buf[i];
    return s;
  }
  void close() {
    if (f) fclose(f);
    f = nullptr;
  }
};

class LittleFSClass {
 public:
  std::string root = "/tmp/fsroot";
  bool begin(bool formatOnFail = false) {
    (void)formatOnFail;
    ::mkdir(root.c_str(), 0777);
    return true;
  }
  // Mirrors FS::mkdir on the device; /b is created through this path now, so
  // the host exercises the same directory handling the firmware relies on.
  bool mkdir(const char* path) { return ::mkdir((root + path).c_str(), 0777) == 0; }
  File open(const char* path, const char* mode = "r") {
    std::string p = root + path;
    return File(fopen(p.c_str(), mode));
  }
  bool exists(const char* path) {
    struct stat st;
    return stat((root + path).c_str(), &st) == 0;
  }
  bool remove(const char* path) { return ::remove((root + path).c_str()) == 0; }
};
inline LittleFSClass LittleFS;
