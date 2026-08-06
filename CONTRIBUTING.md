# Contributing

Thanks for wanting to make the dashboard better. The project is built so that
**everything verifies without hardware** — please keep it that way.

## Dev loop

```sh
git clone --recurse-submodules https://github.com/armeehn/epaper-dashboard
bash tests/host/run_tests.sh     # the whole gate: what CI runs
```

This strict-compiles every translation unit against the *real* libraries
(GxEPD2, ArduinoJson, mbedTLS — auto-cloned into `tests/host/.deps`), compiles
the sketch for b/w and alternate-size panel variants, emulates the Arduino IDE's
prototype-hoisting quirk (catches "does not name a type" ordering bugs before
they reach a flasher), and runs the 126-check unit suite including a real ECDSA
P-256 signature round-trip against a real `.epb` from `registry/`.
`tests/host/build_preview.sh` renders the README screenshots with the actual
firmware drawing code.

If `registry/` is empty, the suite stops and tells you to run
`git submodule update --init`.

Regenerating embedded assets after editing `web/`:

```sh
python3 tools/make_assets.py     # -> firmware/epaper_dashboard/portal_assets.h
```

Commit the regenerated header together with the `web/` change.

## Adding a panel preset

1. Find the panel's driver class in GxEPD2 (`epd/` = b/w, `epd3c/` = 3-colour).
2. Add a `PANEL_…` block to `firmware/epaper_dashboard/config.h` defining
   `EPD_DRIVER`, `EPD_IS_3C`, `PANEL_NAME` and `PANEL_REFRESH_S`, plus a
   commented line in the "pick one" list. The setup page reads those last two
   over `/api/state`, so it describes the new panel with no UI change.
3. Add the class to `tests/host/mockepd/` (a 5-line dimensions stub) and, for
   prebuilt binaries, a matrix line in `.github/workflows/firmware.yml`.
4. Update the table in `docs/HARDWARE.md` with an honest status
   (⚪ compiles / 🟡 CI-built / ✅ hardware-tested).

Never hard-code a resolution, refresh time, chip or grid size in `web/` — that
is what `device{}` in `/api/state` is for.

## Adding an email or calendar provider

`web/app.js` → `MAIL_PROVIDERS` and `CAL_PROVIDERS`. The two are independent:
somebody can use one provider's mail with another's calendar, so do not assume
a mail entry implies a calendar entry.

Entries are a convenience, not the supported set — any IMAP and any CalDAV
server already work, and `Detect` probes the conventional host names on the
device for domains nobody has listed. So add a preset only when you can state
its settings accurately:

- `host` for a fixed IMAP host, or `hostFromDomain` for the usual prefix.
- `domains` to preselect the entry from the user's address.
- `hint` for what the password actually is (app password? enabled where?).
- `dav` for the CalDAV base URL, or `icsOnly` with a `note` explaining why
  CalDAV cannot work.
- `blocked` for a provider that has withdrawn password logins. Saying so
  plainly is more useful than a preset that half works.

## Contributing dashboard blocks

Blocks live in **[armeehn/epaper-blocks](https://github.com/armeehn/epaper-blocks)**,
vendored here as `registry/`. Open block PRs against that repository — its
`CONTRIBUTING.md` has the review checklist and `BLOCK_SPEC.md` the field
reference. Nothing in the firmware needs to change to add a block.

To point this repository at a newer registry commit:

```sh
git -C registry pull origin main
git add registry && git commit -m "registry: bump to <sha>"
```

## Pull requests

- `bash tests/host/run_tests.sh` must pass; add checks for parser or logic changes.
- Keep the no-hardware verification story intact — if something can only be
  tested on a device, structure it so the logic is host-testable.
- Keep the README lean; depth goes in `docs/`.
- One logical change per PR, with a commit message that explains *why*.

## Code of conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
