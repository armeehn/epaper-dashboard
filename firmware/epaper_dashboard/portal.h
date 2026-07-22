#pragma once
// Captive-portal setup wizard: AP + DNS + Bootstrap web app + REST API.
#include <Arduino.h>

enum PortalReason { PORTAL_FIRST_BOOT, PORTAL_BUTTON, PORTAL_FAILURES };

// Blocks forever serving the wizard; reboots the device on "finish".
void portalRun(PortalReason reason);
