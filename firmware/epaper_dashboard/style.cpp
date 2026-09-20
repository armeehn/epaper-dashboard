#include "style.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "ClockFont.h"      // DejaVu Serif Bold digits (generated)
#include "TempFont.h"       // DejaVu Serif Bold digits (generated)
#include "RiposteFonts.h"   // JetBrains Mono, seven faces (generated)

// Classic: what every panel drew before looks existed. Kept pixel-identical.
static const Style CLASSIC = {
  &FreeSans9pt7b, &FreeSansBold9pt7b, &FreeSans12pt7b, &FreeSansBold12pt7b,
  &FreeSansBold18pt7b, &DashClockFont, &DashTempFont,
  /*rule*/ 3, /*tracking*/ 0, /*upper*/ false, /*fullRule*/ false, /*square*/ false,
};

// Riposte: BRAND.md quick reference, translated to pixels. Rules are 2 px
// solid; a label is tracked by 1 px because a 9 pt mono face reads tight in
// caps; bullets are squares because the system has no radius.
static const Style RIPOSTE = {
  &RiposteMono9, &RiposteMonoBold9, &RiposteMono12, &RiposteMonoBold12,
  &RiposteMonoBold18, &RiposteClockFont, &RiposteTempFont,
  /*rule*/ 2, /*tracking*/ 1, /*upper*/ true, /*fullRule*/ true, /*square*/ true,
};

Look lookFromName(const char* name) {
  if (name && strcmp(name, LOOK_RIPOSTE) == 0) {
    return Look::RIPOSTE;
  }
  return Look::CLASSIC;
}

const char* lookName(Look look) {
  return look == Look::RIPOSTE ? LOOK_RIPOSTE : LOOK_CLASSIC;
}

const Style& styleFor(Look look) {
  return look == Look::RIPOSTE ? RIPOSTE : CLASSIC;
}
