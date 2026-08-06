#pragma once
// Captive-portal setup wizard: AP + DNS + Bootstrap web app + REST API.
#include <Arduino.h>

enum PortalReason { PORTAL_FIRST_BOOT, PORTAL_BUTTON, PORTAL_FAILURES };

// Blocks forever serving the wizard; reboots the device on "finish".
void portalRun(PortalReason reason);

// Hook invoked by POST /api/preview: renders the saved layout to the panel
// (registered by the sketch, which owns the display).
void portalSetPreviewHook(void (*fn)());

// LAN editor window: serve the same portal UI over the EXISTING home-WiFi
// connection (no AP, no captive DNS) until idleMs pass without a request
// (hard cap maxMs). Used after a manual-reset refresh so the layout/blocks
// editor is reachable at the device's LAN IP / epaper-dashboard.local while
// real data is loaded. Returns when the window closes.
void portalServeLan(uint32_t idleMs, uint32_t maxMs);
