#pragma once
// ================================================================
//  Compile-time configuration: which panel, which board, which pins.
//  Everything else — WiFi, email, calendar, weather, clock, layout —
//  is configured at runtime through the setup wizard and stored in
//  flash. Most people only ever touch the two "pick one" sections.
// ================================================================

#define FW_VERSION "3.2"

// ---------------- Panel (pick exactly one) ----------------
// Presets for common dashboard-sized panels. Any other GxEPD2-supported
// SPI panel works through PANEL_CUSTOM. Rules of thumb: 3-color panels
// take ~15-30 s per refresh, black&white ~2-5 s; the default layout and
// fonts are designed for ~800x480 and stay usable down to ~640x384.
//
// Uncomment ONE (or pass -DPANEL_… as a build flag; CI does exactly that):
//#define PANEL_75_BW_V2           // 7.5" V2,     800x480, black/white (GDEW075T7)
//#define PANEL_75_B_V1            // 7.5" (B) V1, 640x384, 3-color (discontinued)
//#define PANEL_75_BW_V1           // 7.5" V1,     640x384, black/white
//#define PANEL_75_HD_B            // 7.5" HD (B), 880x528, 3-color
//#define PANEL_583_B_V2           // 5.83" (B) V2, 648x480, 3-color
//#define PANEL_583_BW_V2          // 5.83" V2,    648x480, black/white
//#define PANEL_75_BW_GDEY         // Good Display / Seeed 7.5", 800x480, b/w (GDEY075T7) - docs/LOW_COST.md
//#define PANEL_42_BW              // Good Display 4.2", 400x300, b/w (GDEY042T81) - small, own layout
//#define PANEL_CUSTOM             // any GxEPD2 driver: fill in the mapping below

#if !defined(PANEL_75_B_V2) && !defined(PANEL_75_BW_V2) && !defined(PANEL_75_B_V1) && \
    !defined(PANEL_75_BW_V1) && !defined(PANEL_75_HD_B) && !defined(PANEL_583_B_V2) && \
    !defined(PANEL_583_BW_V2) && !defined(PANEL_75_BW_GDEY) && !defined(PANEL_42_BW) && \
    !defined(PANEL_CUSTOM)
#define PANEL_75_B_V2              // 7.5" (B) V2/V3, 800x480, black/white/red [default]
#endif

// Panel -> GxEPD2 driver class + color capability + how it describes itself.
// EPD_IS_3C 1 = black/white/red (or /yellow) panel, 0 = black/white
// (red accents fold to black automatically on b/w builds).
// PANEL_NAME and PANEL_REFRESH_S are reported to the setup page over
// /api/state, so the UI describes THIS device instead of assuming a default
// one. Resolution is not listed here — it comes from the driver at runtime.
#if defined(PANEL_75_B_V2)
  #define EPD_DRIVER GxEPD2_750c_Z08
  #define EPD_IS_3C 1
  #define PANEL_NAME "Waveshare 7.5\" (B) V2/V3"
  #define PANEL_REFRESH_S 25
#elif defined(PANEL_75_BW_V2)
  #define EPD_DRIVER GxEPD2_750_T7
  #define EPD_IS_3C 0
  #define PANEL_NAME "Waveshare 7.5\" V2"
  #define PANEL_REFRESH_S 4
#elif defined(PANEL_75_B_V1)
  #define EPD_DRIVER GxEPD2_750c
  #define EPD_IS_3C 1
  #define PANEL_NAME "Waveshare 7.5\" (B) V1"
  #define PANEL_REFRESH_S 25
#elif defined(PANEL_75_BW_V1)
  #define EPD_DRIVER GxEPD2_750
  #define EPD_IS_3C 0
  #define PANEL_NAME "Waveshare 7.5\" V1"
  #define PANEL_REFRESH_S 5
#elif defined(PANEL_75_HD_B)
  #define EPD_DRIVER GxEPD2_750c_Z90
  #define EPD_IS_3C 1
  #define PANEL_NAME "Waveshare 7.5\" HD (B)"
  #define PANEL_REFRESH_S 25
#elif defined(PANEL_583_B_V2)
  #define EPD_DRIVER GxEPD2_583c_Z83
  #define EPD_IS_3C 1
  #define PANEL_NAME "Waveshare 5.83\" (B) V2"
  #define PANEL_REFRESH_S 23
#elif defined(PANEL_583_BW_V2)
  #define EPD_DRIVER GxEPD2_583_T8
  #define EPD_IS_3C 0
  #define PANEL_NAME "Waveshare 5.83\" V2"
  #define PANEL_REFRESH_S 4
#elif defined(PANEL_75_BW_GDEY)
  // The cheapest 800x480 route (docs/LOW_COST.md): Good Display's own panel,
  // also sold by Seeed as the XIAO 7.5" panel. UC8179 like the Waveshare V2;
  // if a panel ghosts on this driver, PANEL_75_BW_V2 drives it too.
  #define EPD_DRIVER GxEPD2_750_GDEY075T7
  #define EPD_IS_3C 0
  #define PANEL_NAME "Good Display 7.5\" (GDEY075T7)"
  #define PANEL_REFRESH_S 2
#elif defined(PANEL_42_BW)
  // 400x300 on the 16x12 grid is 25 px cells: the default layout does not
  // fit, so lay out one or two blocks in the editor (a big-number and a
  // list, say). Cheapest way to put a single number on a wall.
  #define EPD_DRIVER GxEPD2_420_GDEY042T81
  #define EPD_IS_3C 0
  #define PANEL_NAME "Good Display 4.2\" (GDEY042T81)"
  #define PANEL_REFRESH_S 2
#elif defined(PANEL_CUSTOM)
  // Any class from GxEPD2's epd/ (b/w) or epd3c/ (3-color) family, e.g.:
  #define EPD_DRIVER GxEPD2_420c   // 4.2" 3-color, 400x300 (small — dense layout)
  #define EPD_IS_3C 1
  #define PANEL_NAME "Custom GxEPD2 panel"
  // Rough seconds for one full refresh; only used to set expectations in the
  // setup page's wording. 3-color panels are typically 15-30 s, b/w 2-5 s.
  #define PANEL_REFRESH_S 25
#endif

// ---------------- Board / wiring (pick exactly one) ----------------
// Uncomment ONE (or pass -DBOARD_… as a build flag):
//#define BOARD_GENERIC_ESP32      // any ESP32 dev board wired per GxEPD2 convention
//#define BOARD_XIAO_EPAPER        // Seeed XIAO ESP32-C3 on the XIAO ePaper Driver Board
//#define BOARD_CUSTOM             // set the EPD_* pins yourself below

#if !defined(BOARD_WAVESHARE_DRIVER) && !defined(BOARD_GENERIC_ESP32) && \
    !defined(BOARD_XIAO_EPAPER) && !defined(BOARD_CUSTOM)
#define BOARD_WAVESHARE_DRIVER     // Waveshare e-Paper ESP32 Driver Board [default]
#endif

#if defined(BOARD_WAVESHARE_DRIVER)
// Waveshare e-Paper ESP32 Driver Board: panel is hard-wired to these pins
// (a remapped SPI bus — the firmware handles the remap automatically).
  #define BOARD_NAME "Waveshare e-Paper ESP32 Driver Board"
  #define EPD_BUSY 25
  #define EPD_RST  26
  #define EPD_DC   27
  #define EPD_CS   15
  #define EPD_SCK  13
  #define EPD_MISO 12   // not used by the panel, required by SPI init
  #define EPD_MOSI 14
#elif defined(BOARD_GENERIC_ESP32)
// GxEPD2's suggested wiring for a plain ESP32 dev board + adapter
// (Waveshare universal e-Paper Driver HAT, Good Display DESPI-C02, …):
// hardware VSPI plus three signal pins.
//   NOTE - universal Driver HAT rev2.2+: tie its PWR pin to 3V3.
  #define BOARD_NAME "Generic ESP32 + adapter"
  #define EPD_BUSY 4
  #define EPD_RST  16
  #define EPD_DC   17
  #define EPD_CS   5
  #define EPD_SCK  18
  #define EPD_MISO 19
  #define EPD_MOSI 23
#elif defined(BOARD_XIAO_EPAPER)
// Seeed XIAO ePaper Driver Board (24-pin FPC) with a XIAO ESP32-C3 on it,
// which is also what the XIAO 7.5" ePaper Panel is inside. GPIO numbers
// are the C3's; the D-names are the board's silkscreen. D9 (GPIO9) is
// both the unused MISO and the C3's BOOT button, so MISO is left
// unattached and GPIO9 stays the setup button.
  #define BOARD_NAME "XIAO ESP32-C3 + XIAO ePaper Driver Board"
  #define EPD_BUSY 4    // D2
  #define EPD_RST  2    // D0
  #define EPD_DC   5    // D3
  #define EPD_CS   3    // D1
  #define EPD_SCK  8    // D8
  #define EPD_MISO -1   // D9, not attached (see above)
  #define EPD_MOSI 10   // D10
#elif defined(BOARD_CUSTOM)
// Your wiring here:
  #define BOARD_NAME "Custom wiring"
  #define EPD_BUSY 4
  #define EPD_RST  16
  #define EPD_DC   17
  #define EPD_CS   5
  #define EPD_SCK  18
  #define EPD_MISO 19
  #define EPD_MOSI 23
#endif

// Which ESP32 variant this build targets. Compile-time rather than
// ESP.getChipModel() so the host verification build resolves it too.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #define CHIP_NAME "ESP32-S3"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define CHIP_NAME "ESP32-S2"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  #define CHIP_NAME "ESP32-C3"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  #define CHIP_NAME "ESP32-C6"
#else
  #define CHIP_NAME "ESP32"
#endif

// Hold this pin LOW right after reset to open the setup portal.
// GPIO0 = the BOOT button on ESP32 / S2 / S3 dev boards; C3 boards use GPIO9.
#ifndef SETUP_BUTTON_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define SETUP_BUTTON_PIN 9
#else
  #define SETUP_BUTTON_PIN 0
#endif
#endif

// ---------------- Setup portal ----------------
#define PORTAL_AP_NAME "EPaper-Dashboard"
#define PORTAL_AP_PASS ""          // "" = open network (join, then captive page)
                                   // set 8+ chars to protect the setup hotspot

// Setup hotspot address — its DHCP server hands out this /24 to your phone
// during setup only. Keep it OFF your home LAN's subnet (the device is joined
// to both networks mid-wizard; overlapping subnets would break routing).
// On your home LAN the device is always a plain DHCP client.
#define PORTAL_AP_IP1 192
#define PORTAL_AP_IP2 168
#define PORTAL_AP_IP3 4
#define PORTAL_AP_IP4 1

// After this many consecutive failed WiFi cycles the portal reopens by itself.
#define FAILS_BEFORE_PORTAL 5

// mDNS name: the editor window is reachable at http://<this>.local/
#define DEVICE_HOSTNAME "epaper-dashboard"

// ---------------- Layout grid ----------------
// Every panel gets the same number of cells regardless of resolution, so a
// layout is portable between devices; cell pixels fall out of the panel size.
#define GRID_COLS 16
#define GRID_ROWS 12

// ---------------- Block registry ----------------
// Index offered by default in the portal's store. Any HTTPS index works —
// this is only a default, and it is orthogonal to trust, which is governed
// by trusted_keys.h.
#define DEFAULT_REGISTRY_URL \
  "https://raw.githubusercontent.com/armeehn/epaper-blocks/main/index.json"

// An index is the same signed .epb envelope as a block, but it describes
// EVERY block in the registry, so it is many times larger than the
// BLK_MAX_DESC cap that bounds a single descriptor. These two are halves of
// one limit: base64 expands 4:3, so a payload decoded from an envelope that
// passed the download cap can never exceed the payload cap — the download is
// always the binding constraint, and growing the registry cannot silently
// start failing the decode.
#define REGISTRY_MAX_BYTES   24576
#define REGISTRY_MAX_PAYLOAD ((REGISTRY_MAX_BYTES * 3) / 4)
