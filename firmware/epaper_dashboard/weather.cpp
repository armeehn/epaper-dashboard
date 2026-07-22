#include "weather.h"
#include "net_util.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* wmoText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: case 56: case 57: return "Drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85: case 86: return "Snow showers";
    case 95: case 96: case 99: return "Thunderstorm";
    default: return "Unknown";
  }
}

static bool httpGetJson(const String& url, JsonDocument& doc, char* msg, size_t msgLen) {
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  if (!http.begin(cli, url)) {
    strlcpy(msg, "bad URL", msgLen);
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    snprintf(msg, msgLen, "HTTP %d", code);
    http.end();
    return false;
  }
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) {
    snprintf(msg, msgLen, "bad JSON: %s", err.c_str());
    return false;
  }
  return true;
}

bool weatherFetch(const Settings& s, WxResult& r) {
  memset(&r, 0, sizeof(r));
  if (s.lat == 0 && s.lon == 0) {
    strlcpy(r.msg, "no location configured", sizeof(r.msg));
    return false;
  }
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(s.lat, 4) +
               "&longitude=" + String(s.lon, 4) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day" +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max" +
               "&temperature_unit=" + (s.unitT[0] == 'c' ? "celsius" : "fahrenheit") +
               "&wind_speed_unit=" + (strcmp(s.unitW, "kmh") == 0 ? "kmh" : "mph") +
               "&timezone=auto&forecast_days=3";
  JsonDocument doc;
  if (!httpGetJson(url, doc, r.msg, sizeof(r.msg))) return false;

  JsonObject cur = doc["current"];
  if (cur.isNull()) {
    strlcpy(r.msg, "no current weather in response", sizeof(r.msg));
    return false;
  }
  WeatherT& w = r.wx;
  w.temp = (int16_t)round(cur["temperature_2m"] | 0.0f);
  w.feels = (int16_t)round(cur["apparent_temperature"] | 0.0f);
  w.hum = (int16_t)(cur["relative_humidity_2m"] | 0);
  w.wind = (int16_t)round(cur["wind_speed_10m"] | 0.0f);
  w.code = (int16_t)(cur["weather_code"] | 3);
  w.isDay = (cur["is_day"] | 1) ? 1 : 0;
  strlcpy(w.cond, wmoText(w.code), sizeof(w.cond));

  JsonArray days = doc["daily"]["time"];
  static const char* NAMES[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  for (int i = 0; i < 3 && i < (int)days.size(); i++) {
    const char* date = days[i] | "";
    int y, mo, d;
    DayFcT& f = w.d[i];
    if (sscanf(date, "%d-%d-%d", &y, &mo, &d) == 3) {
      long epochDays = (long)(timegmCivil(y, mo, d, 0, 0, 0) / 86400);
      strlcpy(f.dow, (i == 0) ? "Today" : NAMES[(epochDays + 4) % 7], sizeof(f.dow));
    } else {
      strlcpy(f.dow, "?", sizeof(f.dow));
    }
    f.hi = (int16_t)round(doc["daily"]["temperature_2m_max"][i] | 0.0f);
    f.lo = (int16_t)round(doc["daily"]["temperature_2m_min"][i] | 0.0f);
    f.code = (int16_t)(doc["daily"]["weather_code"][i] | 3);
    f.pop = (int16_t)(doc["daily"]["precipitation_probability_max"][i] | 0);
  }
  r.ok = true;
  return true;
}

String weatherGeocode(const String& query) {
  JsonDocument doc;
  char msg[64];
  String url = "https://geocoding-api.open-meteo.com/v1/search?count=6&language=en&format=json&name=" +
               urlEncode(query);
  JsonDocument out;
  if (!httpGetJson(url, doc, msg, sizeof(msg))) {
    out["ok"] = false;
    out["msg"] = msg;
  } else {
    out["ok"] = true;
    JsonArray res = out["results"].to<JsonArray>();
    for (JsonObject g : doc["results"].as<JsonArray>()) {
      JsonObject o = res.add<JsonObject>();
      o["name"] = String(g["name"] | "?");
      o["admin1"] = String(g["admin1"] | "");
      o["country"] = String(g["country_code"] | "");
      o["lat"] = g["latitude"] | 0.0f;
      o["lon"] = g["longitude"] | 0.0f;
      o["tz"] = String(g["timezone"] | "");
    }
  }
  String s;
  serializeJson(out, s);
  return s;
}
