# Security

## Trust model in one page

- **Credentials** (WiFi, IMAP, CalDAV) are entered on the device's own captive
  portal, stored in NVS flash, and never leave the device. They are not
  encrypted at rest — anyone with physical USB access can read them (normal
  for ESP32-class hardware; treat the board like a house key).
- **TLS** connections (IMAP, CalDAV, HTTPS block sources) do not validate
  certificates — there is no practical CA-store management on-device. A
  network-level attacker could therefore impersonate your mail server to the
  dashboard. Acceptable for a home display; know your threat model.
- **Contributed blocks are data, never code.** The firmware interprets a
  declarative description (HTTPS source, extractions, widget) with SSRF
  guards (https-only, no private/localhost targets, parameters never reach
  the host) and hard size caps. Blocks are distributed as `.epb` envelopes
  signed with **ECDSA P-256** and verified on-device against
  `trusted_keys.h`; unsigned installs are refused unless explicitly enabled.
  The bundled demo key is **public by design** — mint your own
  (`tools/block_sign.py keygen`) before trusting a real registry.
- **The portal trusts whoever reaches it.** The setup hotspot exists only
  during setup (set `PORTAL_AP_PASS` in `config.h` to lock it); the LAN
  editor window — including layout changes and OTA upload — is open to your
  LAN for a few minutes after a manual reset. On shared networks, use the
  password-protected settings portal instead.
- **OTA updates** are uploaded by a human from the browser, written to the
  spare slot, hash-checked (optional SHA-256) and image-verified before the
  boot slot switches. There is no auto-update and no phone-home: the device
  only ever fetches what you configured it to fetch.

The full block-system spec and threat analysis live in [DESIGN.md](DESIGN.md).

## Reporting

Found a vulnerability? Please open a GitHub issue (or, if it's sensitive,
email the maintainer listed in `LICENSE`) with reproduction steps. This is a
hobby project — no bounties, but fixes ship quickly and credit is given.
