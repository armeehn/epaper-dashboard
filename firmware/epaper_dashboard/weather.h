#pragma once
// Open-Meteo weather + geocoding (fetched directly by the ESP32).
#include <Arduino.h>
#include "dashboard_data.h"
#include "settings.h"

// current + 3-day forecast for the configured location
bool weatherFetch(const Settings& s, WxResult& r);

// City search for the setup wizard. Returns a JSON string:
// {"ok":true,"results":[{"name","admin1","country","lat","lon","tz"}]}
String weatherGeocode(const String& query);

// WMO weather code -> short label ("Rain", "Partly cloudy", ...)
const char* wmoText(int code);
