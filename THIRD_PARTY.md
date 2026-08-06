# Third-party components

Bundled in this repository:

- **Bootstrap 5.3** (`web/bootstrap.min.css`, embedded gzipped in
  `portal_assets.h`) — MIT License, © The Bootstrap Authors.
- **DejaVu Serif Bold** — the generated digit glyphs in `ClockFont.h` /
  `TempFont.h` are rasterized from DejaVu fonts (Bitstream Vera license +
  public-domain additions; free to embed and redistribute).

Fetched at build/verification time (not vendored):

- **GxEPD2** (display driver) — **GPL-3.0**. Note: this repo's own code is
  MIT, but a *compiled firmware binary* links GxEPD2, so distributing
  binaries falls under GPLv3 terms — publishing this source repository is
  the straightforward way to comply.
- **Adafruit GFX / BusIO** — BSD.
- **ArduinoJson** — MIT.
- **mbedTLS** (host tests; on-device it ships inside ESP-IDF) — Apache-2.0.
- **EpoxyDuino** (host tests only) — MIT.
