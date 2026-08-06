#pragma once
// Shared data model between fetchers, cache, and renderer.
#include <Arduino.h>
#include <time.h>

#define MAXE 8    // calendar events kept
#define MAXM 5    // emails kept

typedef struct {
  char title[44];
  char when[12];      // preformatted start time ("2:30 PM" / "all day")
  uint32_t ts0, ts1;  // epoch start/end
  uint8_t day;        // 0 = today, 1 = tomorrow
  uint8_t allDay;
} EventT;

typedef struct {
  char from[26];
  char subj[52];
  uint32_t ts;        // epoch of message date (0 = unknown)
} EmailT;

typedef struct {
  char dow[7];
  int16_t hi, lo, code, pop;
} DayFcT;

typedef struct {
  int16_t temp, feels, hum, wind, code;
  uint8_t isDay;
  char cond[20];
  DayFcT d[3];
} WeatherT;

// Result wrappers used by fetchers and the portal test endpoints
typedef struct {
  bool ok;
  int unread;
  int n;
  EmailT emails[MAXM];
  char stage[12];
  char msg[96];
} ImapResult;

typedef struct {
  bool ok;
  int n;
  EventT events[MAXE];
  char msg[96];
} CalResult;

typedef struct {
  bool ok;
  WeatherT wx;
  char msg[96];
} WxResult;
