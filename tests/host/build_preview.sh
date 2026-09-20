#!/usr/bin/env bash
# Builds the pixel-exact preview renderer (real drawing code on a mock
# framebuffer) and renders the four scenes to PPM in the repo root.
# Run tests/host/run_tests.sh at least once first (it fetches the deps).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS="$ROOT/tests/host/.deps"
STUBS="$ROOT/tests/host/stubs"
FW="$ROOT/firmware/epaper_dashboard"
BUILD="$ROOT/tests/host/.build"
mkdir -p "$BUILD"
DEFS="-DARDUINO=100 -DEPOXY_DUINO -DEPOXY_CORE_ESP8266 -DMEGATINYCORE"
CORE=$(ls "$DEPS"/EpoxyDuino/cores/epoxy/*.cpp | grep -v main.cpp)
g++ -std=gnu++17 -w $DEFS \
  -I "$ROOT/tests/host/mockepd" -I "$STUBS" -I "$DEPS/EpoxyDuino/cores/epoxy" \
  -I "$DEPS/Adafruit-GFX-Library" -I "$DEPS/Adafruit_BusIO" \
  -I "$DEPS/ArduinoJson/src" -I "$DEPS/mbedtls/include" \
  -include "$STUBS/prelude.h" \
  -o "$BUILD/render_preview" "$ROOT/tools/render_preview.cpp" \
  "$FW/net_util.cpp" "$FW/settings.cpp" "$FW/imap.cpp" "$FW/ics.cpp" \
  "$FW/caldav.cpp" "$FW/weather.cpp" "$FW/blocks.cpp" "$FW/blocksig.cpp" \
  "$FW/fsstore.cpp" "$FW/portal.cpp" "$FW/style.cpp" \
  "$DEPS/Adafruit-GFX-Library/Adafruit_GFX.cpp" $CORE \
  -L "$DEPS/mbedtls/library" -lmbedcrypto
(cd "$ROOT" && "$BUILD/render_preview")
echo "PPMs written to $ROOT (convert with ImageMagick/PIL as you like)"
