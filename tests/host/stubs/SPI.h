// Host-compile stub of ESP32 SPI API (shadows EpoxyDuino's AVR-style SPI.h)
#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED
#include <Arduino.h>
#ifndef MSBFIRST
#define MSBFIRST 1
#endif
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
#define SPI_HAS_TRANSACTION 1
#define HSPI 1
#define VSPI 2
class SPISettings {
 public:
  SPISettings() {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) { (void)clock; (void)bitOrder; (void)dataMode; }
};
class SPIClass {
 public:
  SPIClass(uint8_t bus = 0) { (void)bus; }
  void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1) { (void)sck; (void)miso; (void)mosi; (void)ss; }
  void end() {}
  void beginTransaction(SPISettings s) { (void)s; }
  void endTransaction() {}
  uint8_t transfer(uint8_t d) { return d; }
  uint16_t transfer16(uint16_t d) { return d; }
  void transfer(void* buf, size_t n) { (void)buf; (void)n; }
};
extern SPIClass SPI;
#endif
