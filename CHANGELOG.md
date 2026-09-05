# Changelog

Notable changes per release. Dates are release dates; `Unreleased` is what is
on `main`.

## Unreleased

### Added
- `hardware/case/fit_check.sh` and `hardware/case/render.sh`: the case's fit
  invariants as a runnable check, and one command to regenerate all four STLs
  and all six previews from the model.
- Optional **store** step in the setup wizard: browse the signed block registry
  by category and install during first-run setup, instead of only afterwards.
- `GET /api/imap/detect` — credential-free IMAP host detection for any mail
  domain, so providers that are in no preset list still configure themselves.
- `device{}` in `GET /api/state`: panel name, resolution, colours, refresh
  seconds, board, chip, grid, hostname and default registry. The setup page
  renders itself from this and states no hardware fact of its own.
- Separate mail and calendar provider catalogues (~18 and ~15 entries), so one
  provider's mail can be used with another's calendar. Providers that have
  withdrawn password logins are listed as such rather than omitted.
- `PANEL_NAME`, `PANEL_REFRESH_S`, `BOARD_NAME`, `CHIP_NAME`, `GRID_COLS`,
  `GRID_ROWS`, `DEVICE_HOSTNAME` and `DEFAULT_REGISTRY_URL` in `config.h`.
- `mailDomainAllowed()` guard plus nine unit checks (126 total).
- `CODE_OF_CONDUCT.md`, PR template, feature-request and config issue templates.

### Changed
- **The block registry moved to its own public repository,**
  [armeehn/epaper-blocks](https://github.com/armeehn/epaper-blocks), vendored
  here as the `registry/` submodule. Clone with `--recurse-submodules`; CI does.
  Block contributions go there and need no firmware change.
- `tools/block_sign.py` moved into that repository, joined by
  `tools/validate_blocks.py`, which enforces the review checklist in CI.
- The signed index now carries `category`, `minW`, `minH` and a screenshot URL
  per block, which is what the store front renders.
- README trimmed by ~40%; detail lives in `docs/`.

### Fixed
- **The case could not be assembled: the panel had no way in.** The four lid
  screw pillars stood inside the panel pocket, 8.3 mm into it at each corner,
  narrowing the 170.2 mm opening to 154.2 mm of clear span. 4.1 cm3 of plastic
  sat in the path of a sheet of glass that cannot be trimmed or bent, so the
  only way to fit the panel was to cut a side off the frame. The pillars are
  gone; the screws now tap into the solid left and right side walls, and the
  lid became a full-footprint cap that lands on the body's rear shoulder
  instead of a plug that reached into the pocket. `hardware/case/fit_check.sh`
  intersects the body with the panel's swept volume, and the body with the
  lid, and fails on any shared solid, so nothing can be put back.
- **The panel pocket was tighter than the printer.** 0.3 mm clearance per side
  on a 170 mm span is inside the dimensional error of an FDM part, so a
  correct print could still come out as an interference fit. Now 0.6 mm, with
  a 45 degree lead-in at the rear mouth that guides both the glass and the
  lid rim.
- **Square glass corners were being asked to seat in filleted pocket corners.**
  Interference was small enough to look like a tight fit and large enough to
  bind. Four corner reliefs give them room, and an assert keeps the relief
  sized against the actual clearance.
- **The front foam was in the instructions but not in the model.** Assembly
  called for foam tape on the front ledge as well as the rear border, but the
  depth stack only counted one layer, so tightening the lid squeezed 1 mm of
  foam that had nowhere to go, straight into the glass. `foam_fr` is now a
  parameter and the case is 1 mm deeper for it.
- **The Layout, Blocks and Update tabs could not reach the device.** Portal
  requests are served from `setup()` on the Arduino loop task, whose stack is
  8 KB, and a `BlockDef` is ~4 KB. `GET /api/blocks` held one on the stack and
  called `blockParse()`, which built a second as a temporary — a 9.3 KB chain
  that overflowed the stack and panicked the device mid-response. Installing a
  block was worse at 10.5 KB. The browser saw the connection drop, so all three
  tabs reported the device as unreachable: the Layout tab loads `/api/blocks`
  too, and the reboot took the whole portal down, so the Update tab's next
  request failed as well. These descriptors are heap-allocated now (the render
  path already did this), taking the two chains to 1.2 KB and 2.4 KB.
- `tests/host/run_tests.sh` now measures frame sizes with `-fstack-usage` and
  fails if anything in `portal.cpp`, `blocks.cpp` or `fsstore.cpp` exceeds a
  2560-byte budget, so a large stack local cannot silently return.
- Block installs now `mkdir` `/b` on mount. LittleFS, unlike the SPIFFS it
  replaced, has real directories and will not create a missing parent on
  open-for-write, which failed installs with "flash write failed".
- **Loading the block registry failed with "not a block index".** An index is
  the same signed `.epb` envelope as a block, so it was being opened with
  `BLK_MAX_DESC` — the 4 KB cap for a *single* descriptor. At 13 blocks the
  index decodes to 5.7 KB, so the open failed, the portal fell back to parsing
  the envelope as a bare index and reported the format mismatch it found
  there. `epbOpen()` now takes the cap as an argument and the registry passes
  `REGISTRY_MAX_PAYLOAD`, derived from the download cap so the two can't drift.
  An envelope that fails to open is no longer silently reparsed as bare JSON;
  the real error is reported instead.
- Block author strings were 29 characters against a 28-byte field and were
  being truncated on-device to `epaper-dashboard contribut`.

### Security
- The registry is signed with a maintainer key held offline
  (keyid `epaper-blocks-2026`) instead of a demo key whose private half was
  committed to the repository. **Devices flashed before this need one reflash**
  to pick up the new anchor in `trusted_keys.h`.
- The keyid assertion in the test suite now reads `TRUSTED_KEYS[0]`, so the
  trust anchor and the registry cannot drift apart unnoticed.

## 3.2

- Blocks: declarative engine, 16×12 grid editor, signed `.epb` install from
  URL, paste or registry index.
- OTA updates from the portal, with an optional SHA-256 check.
- Panel presets for 7.5", 7.5" HD and 5.83" in 3-colour and black/white, plus
  `PANEL_CUSTOM` for any other GxEPD2 driver.
- Printable enclosure: parametric OpenSCAD source and STLs.
