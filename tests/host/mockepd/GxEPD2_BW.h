// Host-preview mock of GxEPD2_BW: real Adafruit_GFX rendering into a
// 1-bit framebuffer, dumped as PPM. Pixel behavior matches the real
// driver: only GxEPD_BLACK darkens a pixel (the firmware folds red to
// black on b/w builds before drawing, exactly like on hardware).
#pragma once
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef GxEPD_WHITE
#define GxEPD_WHITE 0xFFFF
#define GxEPD_BLACK 0x0000
#define GxEPD_RED   0xF800
#endif

class GxEPD2_750_T7 {
 public:
  static const uint16_t WIDTH = 800, HEIGHT = 480;
  GxEPD2_750_T7(int16_t, int16_t, int16_t, int16_t) {}
  void selectSPI(SPIClass&, SPISettings) {}
};
class GxEPD2_750 {
 public:
  static const uint16_t WIDTH = 640, HEIGHT = 384;
  GxEPD2_750(int16_t, int16_t, int16_t, int16_t) {}
  void selectSPI(SPIClass&, SPISettings) {}
};
class GxEPD2_583_T8 {
 public:
  static const uint16_t WIDTH = 648, HEIGHT = 480;
  GxEPD2_583_T8(int16_t, int16_t, int16_t, int16_t) {}
  void selectSPI(SPIClass&, SPISettings) {}
};

template <typename EPD, const uint16_t page_height>
class GxEPD2_BW : public Adafruit_GFX {
 public:
  EPD epd2;
  uint8_t* fb;
  explicit GxEPD2_BW(EPD e) : Adafruit_GFX(EPD::WIDTH, EPD::HEIGHT), epd2(e) {
    fb = (uint8_t*)calloc((size_t)EPD::WIDTH * EPD::HEIGHT, 1);
  }
  void init(uint32_t = 0, bool = true, uint16_t = 10, bool = false) {}
  void hibernate() {}
  void setFullWindow() {}
  void firstPage() {}
  bool nextPage() { return false; }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= EPD::WIDTH || y >= EPD::HEIGHT) return;
    fb[(int32_t)y * EPD::WIDTH + x] = (color == GxEPD_BLACK) ? 1 : 0;
  }
  void dumpPPM(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", EPD::WIDTH, EPD::HEIGHT);
    static const uint8_t pal[2][3] = {{243, 240, 231}, {24, 24, 24}};
    for (int32_t i = 0; i < (int32_t)EPD::WIDTH * EPD::HEIGHT; i++)
      fwrite(pal[fb[i]], 1, 3, f);
    fclose(f);
  }
};
