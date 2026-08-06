# Contributing

Thanks for wanting to make the dashboard better! The project is built so that
**everything verifies without hardware** — please keep it that way.

## Dev loop

```sh
bash tests/host/run_tests.sh     # the whole gate: what CI runs
```

This strict-compiles every translation unit against the *real* libraries
(GxEPD2, ArduinoJson, mbedTLS — auto-cloned into `tests/host/.deps`), compiles
the sketch for b/w and alternate-size panel variants, emulates the Arduino
IDE's prototype-hoisting quirk (catches "does not name a type" ordering bugs
before they reach a flasher), and runs the 117-check unit suite, including a
real ECDSA P-256 signature round-trip. `tests/host/build_preview.sh` renders
the README screenshots with the actual firmware drawing code.

Regenerating embedded assets after editing `web/`:

```sh
python3 tools/make_assets.py     # -> firmware/epaper_dashboard/portal_assets.h
```

(Commit the regenerated header together with the `web/` change.)

## Adding a panel preset

1. Find the panel's driver class in GxEPD2 (`epd/` = b/w, `epd3c/` = 3-color).
2. Add a `PANEL_…` block to `firmware/epaper_dashboard/config.h` (define
   `EPD_DRIVER` + `EPD_IS_3C`) and a commented line in the "pick one" list.
3. Add the class to `tests/host/mockepd/` (a 5-line dimensions stub) and, if
   you want prebuilt binaries, a matrix line in `.github/workflows/firmware.yml`.
4. Update the table in `docs/HARDWARE.md` with an honest status
   (⚪ compiles / 🟡 CI-built / ✅ hardware-tested).

## Adding an email/calendar provider preset

`web/app.js` → `PROVIDERS` (host, CalDAV base, one-line password hint) and the
`<select>` in `web/index.html`. Only add providers that work with plain
password IMAP — OAuth-only services (Gmail without app passwords, Outlook.com)
can't work on a standalone device; say so in the hint rather than half-working.

## Contributing dashboard blocks

Blocks are declarative JSON, never code — read the spec in
[DESIGN.md](DESIGN.md), then follow [registry/README.md](registry/README.md)
for authoring, signing (`tools/block_sign.py`) and the review checklist.
Community registries are just static files behind HTTPS — you can host your
own with your own signing key.

## Pull requests

- `bash tests/host/run_tests.sh` must pass; add checks for parser/logic changes.
- Firmware changes should keep the no-hardware verification story intact —
  if it can only be tested on a device, structure it so the logic is host-testable.
- Keep the README lean; depth goes in `docs/`.
- One logical change per PR, with a commit message that explains *why*.
