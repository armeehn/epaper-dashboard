#pragma once
// ================================================================
//  Compile-time configuration (hardware + portal identity).
//  Everything else — WiFi, email, calendar, weather, clock — is
//  configured at runtime through the setup wizard and stored in
//  flash (NVS). You normally don't need to edit this file.
// ================================================================

#define FW_VERSION "2.0"

// ---------- Setup portal ----------
#define PORTAL_AP_NAME "EPaper-Dashboard"
#define PORTAL_AP_PASS ""          // "" = open network (join, then captive page)
                                   // set 8+ chars to protect the setup hotspot

// Setup hotspot address — its DHCP server hands out this /24 to your phone
// during setup only. Keep it OFF your home LAN's subnet: your LAN is
// 192.168.1.0/24, so the default below (192.168.4.1) can never collide with it
// while the device is joined to both networks mid-wizard.
// On your home LAN the device is always a plain DHCP client — your router
// assigns its 192.168.1.x lease (shown in the dashboard footer).
#define PORTAL_AP_IP1 192
#define PORTAL_AP_IP2 168
#define PORTAL_AP_IP3 4
#define PORTAL_AP_IP4 1

// Hold this pin LOW at power-on/reset to re-enter the setup portal.
// GPIO0 = the BOOT button on the Waveshare ESP32 driver board.
#define SETUP_BUTTON_PIN 0

// After this many consecutive failed WiFi cycles the portal reopens by itself.
#define FAILS_BEFORE_PORTAL 5

// ---------- Panel (pick exactly one) ----------
#define PANEL_75_B_V2              // 7.5" (B) V2/V3, 800x480  <-- yours
//#define PANEL_75_B_V1            // 7.5" (B) V1, 640x384 (discontinued)
//#define PANEL_75_HD_B            // 7.5" HD (B), 880x528

// ---------- Pins: Waveshare e-Paper ESP32 Driver Board ----------
#define EPD_BUSY 25
#define EPD_RST  26
#define EPD_DC   27
#define EPD_CS   15
#define EPD_SCK  13
#define EPD_MISO 12   // not used by the panel, required by SPI init
#define EPD_MOSI 14
