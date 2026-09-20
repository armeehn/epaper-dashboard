#!/usr/bin/env bash
# Host-side verification for the whole firmware — no ESP32 required.
#   1. strict-compiles every translation unit against the REAL libraries
#   2. emulates the Arduino IDE's prototype-hoisting (catches ordering bugs)
#   3. runs the unit-test suite, including a real ECDSA P-256 signature
#      round-trip through mbedTLS
# Dependencies are cloned into tests/host/.deps on first run (git + g++ +
# python3 required; mbedTLS's build-time codegen also imports jsonschema —
# `pip install jsonschema`). This is exactly what CI runs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/tests/host/.deps"
STUBS="$ROOT/tests/host/stubs"
FW="$ROOT/firmware/epaper_dashboard"
BUILD="$ROOT/tests/host/.build"
mkdir -p "$DEPS" "$BUILD"

# The signature round-trip verifies a real .epb from the block registry, which
# is a submodule. Without it the suite would "pass" having skipped that check.
if [ ! -f "$ROOT/registry/index.json" ]; then
  echo "registry/ is empty — run: git submodule update --init" >&2
  exit 1
fi

clone() { [ -d "$DEPS/$2" ] || git clone -q --depth 1 "https://github.com/$1" "$DEPS/$2"; }
echo "== deps =="
clone bxparks/EpoxyDuino EpoxyDuino
clone bblanchon/ArduinoJson ArduinoJson
clone ZinggJM/GxEPD2 GxEPD2
clone adafruit/Adafruit-GFX-Library Adafruit-GFX-Library
clone adafruit/Adafruit_BusIO Adafruit_BusIO
[ -d "$DEPS/mbedtls" ] || git clone -q --depth 1 --branch mbedtls-3.6 "https://github.com/Mbed-TLS/mbedtls" "$DEPS/mbedtls"   # LTS: classic make-lib build
if [ ! -f "$DEPS/mbedtls/library/libmbedcrypto.a" ]; then
  echo "   building mbedtls (once)..."
  (cd "$DEPS/mbedtls" && git submodule update --init --depth 1 >/dev/null \
     && make lib -j"$(nproc)" >/dev/null)
fi

DEFS="-DARDUINO=100 -DEPOXY_DUINO -DEPOXY_CORE_ESP8266 -DMEGATINYCORE"
INC="-I $STUBS -I $DEPS/EpoxyDuino/cores/epoxy -I $DEPS/GxEPD2/src \
     -I $DEPS/Adafruit-GFX-Library -I $DEPS/Adafruit_BusIO \
     -I $DEPS/ArduinoJson/src -I $DEPS/mbedtls/include"
CXX="g++ -std=gnu++17 $DEFS $INC -include $STUBS/prelude.h"
CORE=$(ls "$DEPS"/EpoxyDuino/cores/epoxy/*.cpp | grep -v main.cpp)

echo "== strict compile (12 translation units) =="
for f in net_util settings imap ics caldav weather blocks blocksig fsstore portal style; do
  $CXX -Wall -Wextra -Wno-unused-parameter -c "$FW/$f.cpp" -o "$BUILD/$f.o"
done
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ "$FW/epaper_dashboard.ino" -o "$BUILD/ino.o"

echo "== Arduino IDE prototype-hoist emulation =="
python3 "$ROOT/tools/arduino_proto_check.py" "$FW/epaper_dashboard.ino" "$BUILD/ino_ard.cpp" >/dev/null
$CXX -w -I "$FW" -I "$ROOT/tests/host/mockepd" -c "$BUILD/ino_ard.cpp" -o "$BUILD/ino_ard.o"

echo "== panel variants (b/w + other sizes compile against real GxEPD2) =="
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ -DPANEL_75_BW_V2 \
  "$FW/epaper_dashboard.ino" -o "$BUILD/ino_bw.o"
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ -DPANEL_583_B_V2 \
  "$FW/epaper_dashboard.ino" -o "$BUILD/ino_583.o"
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ -DPANEL_75_BW_V1 -DBOARD_GENERIC_ESP32 \
  "$FW/epaper_dashboard.ino" -o "$BUILD/ino_bw_v1.o"
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ -DPANEL_75_BW_GDEY -DBOARD_XIAO_EPAPER \
  "$FW/epaper_dashboard.ino" -o "$BUILD/ino_gdey_xiao.o"
$CXX -Wall -Wextra -Wno-unused-parameter -c -x c++ -DPANEL_42_BW -DBOARD_XIAO_EPAPER \
  "$FW/epaper_dashboard.ino" -o "$BUILD/ino_42_xiao.o"

echo "== stack budget (portal handlers run on the 8 KB Arduino loop task) =="
# Portal requests are served from setup() on loopTask, whose stack is 8192
# bytes -- and the whole call chain (WebServer parse -> handler -> parser) has
# to fit. A BlockDef is ~4 KB, so one on the stack is already most of the
# budget; two in a chain overflowed it and rebooted the device mid-request,
# which is what "the portal can't reach the device" looked like from the
# browser. Frame sizes here are the host compiler's, not xtensa's, so this is
# a relative regression guard rather than an absolute measurement.
STACK_MAX=2560
rm -f "$BUILD"/*.su
(cd "$BUILD" && $CXX -Wall -Wextra -Wno-unused-parameter -fstack-usage \
   -c "$FW/portal.cpp" -o "$BUILD/portal_su.o" \
   && $CXX -Wall -Wextra -Wno-unused-parameter -fstack-usage \
   -c "$FW/fsstore.cpp" -o "$BUILD/fsstore_su.o" \
   && $CXX -Wall -Wextra -Wno-unused-parameter -fstack-usage \
   -c "$FW/blocks.cpp" -o "$BUILD/blocks_su.o")
over=$(cat "$BUILD"/*.su | awk -F'\t' -v m="$STACK_MAX" '$2+0 > m {print "   " $2 " bytes  " $1}' | sort -rn)
if [ -n "$over" ]; then
  echo "FAIL: these frames exceed the ${STACK_MAX}-byte budget:" >&2
  echo "$over" >&2
  echo "   (heap-allocate large structs such as BlockDef instead)" >&2
  exit 1
fi
echo "   all portal/blocks/fsstore frames within ${STACK_MAX} bytes"

echo "== unit tests (incl. real mbedTLS signature verification) =="
$CXX -Wall -Wno-unused-parameter -o "$BUILD/test_parsers" "$ROOT/tests/test_parsers.cpp" \
  "$FW/net_util.cpp" "$FW/ics.cpp" "$FW/imap.cpp" "$FW/caldav.cpp" \
  "$FW/settings.cpp" "$FW/blocks.cpp" "$FW/blocksig.cpp" "$FW/fsstore.cpp" "$FW/style.cpp" \
  $CORE -L "$DEPS/mbedtls/library" -lmbedcrypto
(cd "$ROOT" && "$BUILD/test_parsers")

echo "ALL CHECKS PASSED"
