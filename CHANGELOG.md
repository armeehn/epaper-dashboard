# Changelog

Notable changes per release. Dates are release dates; `Unreleased` is what is
on `main`.

## Unreleased

### Added
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
