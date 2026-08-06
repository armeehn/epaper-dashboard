# E-Paper Dashboard v2 — turn-key edition

A self-configuring dashboard for the Waveshare 7.5" red/black/white e-paper panel
on the Waveshare e-Paper ESP32 Driver Board. Flash it once; everything else is
configured **on the device itself** through a Bootstrap setup wizard — no code
edits, no cloud backend, no Google account required.

| Custom layout with signed community blocks | Drag-and-drop layout editor |
| --- | --- |
| ![blocks](docs/dashboard_blocks.png) | ![editor](docs/editor_layout.png) |

*(Previews are rendered by the actual firmware drawing code on a mock
framebuffer — `tests/host/build_preview.sh` regenerates them.)*

On every refresh cycle the ESP32 wakes from deep sleep and fetches, all by itself:

- **Email** over **IMAP** (works with Migadu, Fastmail, and any standard IMAP
  server) — unread count plus your newest unread/flagged messages
- **Calendar** over **CalDAV** (Migadu's `cdav.migadu.com` and other SabreDAV /
  Radicale / Nextcloud servers, with automatic calendar discovery) or any
  **ICS/iCal link** — today + tomorrow, recurring events included
- **Weather** from Open-Meteo (no API key) — current conditions + 3-day forecast

It then renders the screen (big clock, date, weather with drawn icons, agenda
with the *happening-now* event in red, inbox with red unread dots) and deep-sleeps
until the next aligned wall-clock boundary. Credentials live only in the
device's flash. Last-good data is cached in RTC memory, so network hiccups show
a red OFFLINE flag with per-section error messages instead of a blank panel.

## The turn-key flow

1. **Flash the firmware** (once — see below).
2. The e-paper shows a welcome screen: *join the WiFi network
   `EPaper-Dashboard`, then open `http://192.168.4.1`*. On phones the setup
   page usually pops up by itself (captive portal).
3. **Answer the wizard's questions** — it walks through WiFi → email →
   calendar → weather → clock & refresh, and **tests each step live** on the
   device: it scans and joins your WiFi (staying reachable through its own
   hotspot), logs into IMAP and shows your newest matching message, discovers
   and lists your CalDAV calendars so you can pick one, geocodes your city, and
   suggests your timezone. Errors appear right there, in plain language.
4. **Save & start** — the device reboots and draws the first dashboard within
   about a minute.

To change settings later: **tap RST, then immediately press & hold BOOT for
~2 seconds** — the setup network comes back with your existing values
pre-filled (passwords are kept on the device and never echoed to the
browser). Order matters: holding BOOT *while* RST is released puts the ESP32
into its ROM flashing mode instead (the chip sits silent until the next plain
reset — if that happens, just tap RST alone). The portal also reopens
automatically after 5 consecutive failed WiFi cycles, so a changed router
password can't strand the display.

### Migadu specifics

The wizard's Migadu preset fills in everything: IMAP `imap.migadu.com:993`
(username = full address, your normal mailbox password) and CalDAV
`https://cdav.migadu.com/` — "Find my calendars" then lists your actual
calendars to choose from. Gmail users: IMAP needs an app password, and for
calendar use the ICS "secret address" from Google Calendar settings instead of
CalDAV.

## Flashing (the only manual step, once)

Arduino IDE 2.x:

1. Boards Manager → install **esp32 by Espressif Systems**.
2. Library Manager → install **GxEPD2** (accept "Install all" for Adafruit GFX
   dependencies) and **ArduinoJson** (v7).
3. Open `firmware/epaper_dashboard/epaper_dashboard.ino`.
4. Tools → Board: **ESP32 Dev Module** · Partition scheme: **Huge APP (3MB)**
   (the embedded Bootstrap UI + TLS stacks need the room).
5. Upload. Done — everything else happens in the wizard.

PlatformIO: `cd firmware && pio run -t upload` (Huge APP partitions are
preconfigured in `platformio.ini`).

### Or skip local compiling entirely

Every push builds the firmware on GitHub Actions (`firmware` workflow) with
the real ESP32 toolchain and uploads ready-to-flash binaries: open the run's
**Artifacts**, grab `epaper-dashboard-factory.bin`, and flash it with nothing
but Python's esptool —

```sh
pip install esptool==4.8.1
python3 -m esptool --chip esp32 --port <PORT> --baud 921600 \
    write_flash 0x0 epaper-dashboard-factory.bin
```

`epaper-dashboard-app.bin` written at `0x10000` instead updates the firmware
while **keeping** your WiFi/settings/blocks/layout. A browser-based flasher
(Chrome/Edge, e.g. esptool-js) can also write the factory image at `0x0` with
zero installs — full instructions ship in the artifact's `FLASHING.txt`.
Pushing a tag like `v3.0.0` attaches the same binaries to a GitHub Release.

If your panel isn't the 800×480 (B) V2/V3, switch the `PANEL_…` define in
`firmware/epaper_dashboard/config.h` — that and pins are the only compile-time
settings left.

## Repo layout

```
firmware/epaper_dashboard/
    epaper_dashboard.ino    boot flow, dashboard rendering, deep sleep
    portal.cpp/.h           captive-portal wizard server (AP+STA, REST API)
    portal_assets.h         embedded gzipped Bootstrap UI (generated)
    imap.cpp/.h             minimal IMAP client (TLS, SEARCH, header fetch)
    caldav.cpp/.h           CalDAV discovery + REPORT, ICS streaming
    ics.cpp/.h              iCalendar parser + recurrence expansion
    weather.cpp/.h          Open-Meteo forecast + geocoding proxy
    settings.cpp/.h         runtime config in NVS flash
    net_util.cpp/.h         encodings, timestamps, URL helpers
    config.h                pins / panel / portal name (compile-time only)
    ClockFont.h, TempFont.h generated DejaVu Serif Bold digit fonts
web/                        wizard source (index.html, app.js, bootstrap css)
tools/make_assets.py        regenerate portal_assets.h after editing web/
tools/make_gfx_font.py      regenerate the clock fonts
tools/arduino_proto_check.py emulates the IDE's prototype-hoisting to catch
                            "does not name a type" errors before flashing
tests/test_parsers.cpp      host unit tests (111 checks: parsers, block
                            engine, signature verification, storage)
tests/host/                 self-contained verification harness: stubs,
                            run_tests.sh (what CI runs), build_preview.sh
registry/                   signed example blocks + signing tool docs
DESIGN.md                   block system spec, trust model, threat model
```

## Editing the layout after setup

Tap **RST** once. The device runs a normal refresh, and because that was a
manual reset (not a timer wake) it then keeps its web server up on your home
WiFi for an editing session: open **http://epaper-dashboard.local/** (or the
IP shown in the dashboard footer) and use the Layout / Blocks tabs directly
from your computer — with your real data loaded, so the preview shows the
actual dashboard, not samples. **Save & preview on panel** first saves the
canvas you're looking at and then renders exactly that, so the panel never
shows a stale layout. The window closes after 5 idle minutes
(30 min hard cap) and the device goes back to deep sleep; saved layouts
apply from the very next refresh.

The full settings portal (RST, then hold BOOT ~2 s) still exists for
changing WiFi/accounts — and it now auto-joins your home network with the
stored credentials, so registry installs and tests work there immediately.

## Blocks: movable, community-contributed widgets (v3)

The screen is a **16×12 grid** of movable blocks. The portal's **Layout** tab
is a drag-and-drop editor (move, resize, per-block settings, live preview on
the panel); the **Blocks** tab installs community blocks. The six classic
sections (clock, date, weather, forecast, calendar, inbox) are built-in
blocks on the same grid — the default layout is exactly the classic screen.

Contributed blocks are **declarative JSON, never code**: an HTTPS source,
value extractions, and a widget from a fixed vocabulary — interpreted by the
firmware's engine with SSRF guards and size caps. They're distributed as
signed `.epb` files verified on-device (ECDSA P-256) against
`trusted_keys.h`; unsigned installs are refused unless explicitly allowed.
Four signed examples ship in `registry/` (Hacker News, crypto price, air
quality, GitHub stars) along with `tools/block_sign.py` (keygen/sign/index)
and a contribution guide. **The bundled demo signing key is public** — mint
your own before trusting real registries. Full spec, trust model, and threat
model: `DESIGN.md`.

## Behavior details

A full tri-color refresh takes ~26 s and flashes — that's the panel technology,
and it's why the wizard's minimum refresh interval is 3 minutes (Waveshare
recommends ≥180 s). The clock therefore shows time as of the refresh, aligned
to :00/:05/… boundaries. Overnight the display pauses (configurable hours) —
e-paper holds its image with zero power — and resumes in the morning.

Recurring events: CalDAV servers are asked to expand recurrences server-side;
if a server can't, the device expands DAILY and WEEKLY rules itself (INTERVAL,
BYDAY, COUNT, UNTIL, EXDATE, and moved single occurrences). MONTHLY/YEARLY
rules fall back to their master date only — a deliberate v1 limitation. Event
times with an explicit foreign TZID are interpreted in the display's timezone.

Networking: on your home LAN the device is a pure DHCP client — on a
192.168.1.0/24 network your router hands it a 192.168.1.x lease every wake cycle
(the current lease shows in the dashboard footer, handy for DHCP
reservations). Any static-IP leftovers in the WiFi stack are cleared on every
connect, so DHCP is guaranteed. The setup hotspot deliberately lives on
192.168.4.1/24 — a different subnet than your LAN — because during the wizard
the device is on both networks at once and overlapping subnets would break
routing; `PORTAL_AP_IP*` in `config.h` changes it if ever needed.

Security notes: the setup hotspot is open by default (set `PORTAL_AP_PASS` in
`config.h` to protect it); it only exists during setup. TLS connections skip
certificate validation (no CA store management on-device) — fine for a home
dashboard, worth knowing about. Credentials are stored unencrypted in NVS, as
is normal for ESP32 projects; anyone with physical USB access to the board
could read them.

## Development & CI

`bash tests/host/run_tests.sh` runs the entire verification suite on any
Linux box (or GitHub Actions — see `.github/workflows/ci.yml`): strict
compilation of all translation units against the real libraries, an
emulation of the Arduino IDE's prototype-hoisting quirk, and 111 unit tests
including a real ECDSA signature round-trip through mbedTLS. Dependencies
auto-clone into `tests/host/.deps` on first run. No hardware needed.

## Troubleshooting

**Setup network never appears** — the firmware boots straight to the dashboard
once configured; tap RST, then hold BOOT for ~2 s to force the portal.

**Screen frozen / device seems dead or won't reset** — first, tap RST *alone*
(if BOOT was held during a reset the chip is sitting in its flashing mode and
a plain reset exits it). If the screen still never refreshes, open the Arduino
Serial Monitor at 115200 and tap RST: the firmware prints every stage (wifi,
mail, calendar, weather, render) with free-heap numbers, and any crash prints
a backtrace — the last line before it tells you which stage failed. The
firmware also self-heals: if a cycle dies twice in a row it skips the calendar
fetch (the heaviest step) and flags it on-screen; after four it skips all
fetches and still draws the clock, so the display always comes back.

**Wizard page stalls right after joining WiFi** — expected for ~10 s: the
ESP32's hotspot hops to your router's channel. Stay on the setup WiFi; the page
retries automatically.

**IMAP test fails with "login rejected"** — Migadu: use the full address as
username and your mailbox password. Fastmail/Gmail: you need an app password.

**"Find my calendars" finds nothing** — paste the full calendar URL into the
server field instead (e.g. from your calendar app's settings), or switch to an
ICS link. The test button tells you exactly which step failed (401 = wrong
credentials, 404 = wrong URL...).

**Blank e-paper / "Busy timeout" in Serial Monitor (115200)** — reseat the
ribbon cable; check the `PANEL_…` define matches the sticker on your panel.

**Compile error "text section exceeds available space"** — you skipped the
**Huge APP** partition scheme in Tools.
