#pragma once
// The panel's look: which faces and rule weights the drawing code uses.
// Two looks ship. "classic" is the stock screen (FreeSans text, DejaVu Serif
// digits, 3 px underlines, round bullets). "riposte" is the Riposte
// Laboratories design system as far as a 1-bit panel carries it (BRAND.md):
// JetBrains Mono throughout, 2 px rules across the whole column, upper-case
// tracked labels, square bullets, radius 0. Colour is never the only signal
// in either, which is what makes the same layouts work on b/w and 3-colour.
#include <Arduino.h>
#include <Adafruit_GFX.h>

enum class Look : uint8_t { CLASSIC, RIPOSTE };

#define LOOK_CLASSIC "classic"
#define LOOK_RIPOSTE "riposte"

struct Style {
  const GFXfont* body;     // 9 pt regular: rows, sub-lines, stats
  const GFXfont* label;    // 9 pt bold: section titles, senders, day names
  const GFXfont* bodyL;    // 12 pt regular: event titles, messages
  const GFXfont* labelL;   // 12 pt bold: AM/PM, conditions
  const GFXfont* display;  // 18 pt bold: the date, compact big-numbers
  const GFXfont* clock;    // clock digits ('0'-'9' ':')
  const GFXfont* big;      // big-number and temperature digits ('-' '.' '/' '0'-'9' ':')
  uint8_t rule;            // thickness of section and block rules, px
  uint8_t tracking;        // extra px between the glyphs of a label
  bool upper;              // labels and the date are set in capitals
  bool fullRule;           // a section rule spans its column, not just its word
  bool square;             // bullets are squares (radius 0), not discs
};

// ---- pure functions (host-testable) ----
// "riposte" -> RIPOSTE; anything else, including "", -> CLASSIC.
Look lookFromName(const char* name);
const char* lookName(Look look);
const Style& styleFor(Look look);
