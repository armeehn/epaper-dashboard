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
  The default anchor is the key of the official registry
  ([armeehn/epaper-blocks](https://github.com/armeehn/epaper-blocks)), whose
  private half is offline and in no repository. To trust your own registry
  instead, mint a key with `registry/tools/block_sign.py keygen`, replace the
  entry in `trusted_keys.h`, and reflash.
- **IMAP auto-detection** turns a typed mail domain into TLS connections from
  the device. `mailDomainAllowed()` restricts it to public DNS names — no
  `.local`, no `localhost`, no IP literals — so the setup page cannot be used
  to probe the LAN. No credentials are sent during detection.
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

## Supported versions

Only the latest release is supported. Fixes ship on `main` and in the next
tagged build; there are no backports.

## Reporting

Report vulnerabilities privately through
[GitHub security advisories](https://github.com/armeehn/epaper-dashboard/security/advisories/new),
with reproduction steps. Please do not open a public issue for anything
affecting signature verification, the block sandbox, or credential handling.

Issues in the blocks themselves, or in the registry tooling, belong in
[armeehn/epaper-blocks](https://github.com/armeehn/epaper-blocks/security/advisories/new).

This is a hobby project — no bounties, but fixes ship quickly and credit is
given. Expect a first response within a week.
