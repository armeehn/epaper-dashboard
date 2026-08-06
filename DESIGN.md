# Blocks: a safe, community-extensible dashboard system

*(v3 design — inspired by i3blocks-contrib, redesigned around "no code, ever")*

## The idea, expanded

i3blocks-contrib works because contributing is trivial: drop a small blocklet
in a repo, users copy it into their bar. Its weakness for our purpose is that
blocklets are *programs* — installing one means running someone's code. On a
device that holds your email and calendar credentials, that's unacceptable.

So this system inverts the model: **a block is data, not code.** A block
descriptor is a small JSON file that *declares* three things:

1. **Source** — where the data comes from (an HTTPS URL returning JSON or
   text), with user parameters substituted into the URL.
2. **Extract** — which values to pull out (a dotted-path selector like
   `current.temperature_2m` or `hits[0].title`) and which fixed, whitelisted
   transforms to apply (round, scale, prefix/suffix, value→label map, list
   fields, limit).
3. **Render** — which widget from a fixed vocabulary draws it (`big-number`,
   `list`, `text`, `bar`), with declarative options (label, red accent,
   alignment).

The firmware ships a fixed interpreter for exactly this vocabulary. A block
can never loop, compute, execute, or touch anything outside its own fetched
response. The *worst* a malicious descriptor can do is show wrong text or
fetch a useless URL — and the URL is constrained too (see threat model).
Built-in blocks (clock, date, weather, forecast, calendar, inbox) are native
code that ships with the firmware and render the same data as today; they are
placed and moved on the same grid as contributed blocks.

## Layout

The panel is a **16 × 12 grid** — cells are `width/16 × height/12` px, so the
same layout scales to any supported resolution (50 × 40 px on the classic
800×480). A layout is a list of placed block instances:
`{instance id, block id, x, y, w, h, params}`.
The web portal's **Layout** tab is a drag-and-drop editor: drag tiles to move,
drag the corner handle to resize, click a tile to edit its parameters (the
form is generated from the block's declared params — types, labels, choices).
Layouts are stored on the device (LittleFS) and survive reflashes. A
"Preview" button renders the saved layout to the actual e-paper with sample
data, so you see the real thing before leaving the editor. The default layout
reproduces the classic screen exactly.

## Distribution & trust: signed blocks

Blocks are distributed as **`.epb` files** — a tiny envelope:

```json
{ "format": "epb1",
  "payload": "<base64 of the block JSON bytes>",
  "sigs": [ { "keyid": "registry-2026", "alg": "ecdsa-p256-sha256",
              "sig": "<base64 DER signature over the payload bytes>" } ] }
```

The signature covers the exact payload bytes (no canonicalization games).
The device verifies it **before** parsing or storing anything, using
ECDSA P-256 + SHA-256 via mbedTLS — already in the ESP32 SDK, hardware-
accelerated, no new dependencies. Trusted public keys are baked into the
firmware (`trusted_keys.h`). The default policy is **signed-only**; a
settings toggle allows unsigned blocks after an explicit warning, for local
development.

Ed25519 was considered and would be preferable in a vacuum; P-256 was chosen
because mbedTLS ships it on-device today, while Ed25519 would mean vendoring
a third-party implementation into the trust root. Revisit if the ESP-IDF
gains native Ed25519.

### The contrib workflow (the i3blocks-contrib part)

A community repo, `registry/` in this project (intended to become its own
GitHub repo):

- One directory per block: `blocks/<name>/block.json` + README + screenshot.
- Contributors open a PR. Review is *feasible* because a block is ~40 lines
  of declarative JSON — a reviewer checks the URL, the paths, the params.
  There is no code to audit.
- On merge, CI (or a maintainer) signs the block with the **registry key**
  and regenerates `index.json` (name, description, author, sizes, .epb URL),
  which is itself signed. This is a distro model: authors contribute,
  the registry vouches.
- On the device, the Blocks tab can browse any registry index URL (default:
  this repo's), or install a single `.epb` by URL or paste. The UI shows the
  signature status and signing key for every installed block.

`tools/block_sign.py` does keygen / sign / verify / make-index, so a fork can
run its own registry with its own key — federation by construction: the
firmware trusts whatever keys its owner bakes in.

## Threat model (what can a hostile block do?)

- **Execute code** — impossible by construction; there is no interpreter for
  anything Turing-shaped. This is the core guarantee.
- **Steal credentials** — contributed blocks cannot reference secrets at all
  (v1 has no secret-typed params; IMAP/CalDAV credentials are never
  accessible to the block engine). Nothing from settings is substitutable
  into a block URL except the block's own user-entered params.
- **Probe your LAN (SSRF)** — block fetches must be `https://`, and literal
  private/loopback/link-local IPs and `.local` hosts are refused. (DNS
  rebinding is documented as out of scope for v1; the fetch has no auth to
  carry anyway.)
- **Resource abuse** — descriptor ≤ 4 KB, ≤ 16 installed blocks, response
  capped at 16 KB, one fetch per block per wake cycle, 10 s timeout. A slow
  URL costs seconds of battery, nothing more.
- **Look wrong / lie** — a block can always render misleading text. Signing
  doesn't fix taste; the registry review is the curation layer, and the
  screen shows data, never actions.
- **Compromised registry key** — rotate by shipping new `trusted_keys.h` in a
  firmware update; the settings toggle lets users refuse unsigned/unknown
  keys entirely. Key ids appear next to every block in the UI.

## What ships now vs. roadmap

Now: the engine (json/text sources, 4 widgets + 6 built-ins), grid editor
with move/resize/params, signed install from URL/paste/registry, example
signed blocks (Hacker News top stories, crypto price, air quality, GitHub
stars), signing tooling, default layout matching the classic screen.

Roadmap, in rough order of value: per-block refresh intervals (fetch less
often than the panel refreshes); a `sparkline` widget fed by a numeric array;
RSS/Atom source type; multi-page layouts rotating per wake; block-level
"quiet hours"; author-key countersignatures alongside the registry key; a
hosted registry browser with screenshots served from the repo; an on-device
key-pinning UI instead of compile-time `trusted_keys.h`.
