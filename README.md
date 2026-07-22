# E-Paper Dashboard v2 — turn-key edition

A self-configuring dashboard for the Waveshare 7.5" red/black/white e-paper panel
on the Waveshare e-Paper ESP32 Driver Board. Flash it once; everything else is
configured **on the device itself** through a Bootstrap setup wizard — no code
edits, no cloud backend, no Google account required.

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

To change settings later: hold **BOOT**, tap **RST**, release — the setup
network comes back with your existing values pre-filled (passwords are kept
server-side on the device and never echoed to the browser). The portal also
reopens automatically after 5 consecutive failed WiFi cycles, so a changed
router password can't strand the display.

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

PlatformIO: `cd firmware && pio run -t upload` (partitions are set via
`board_build.partitions` if you add it; Huge APP equivalent =
`huge_app.csv`).

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
    ClockFont.h, TempFont.h generated Poppins-Bold digit fonts
web/                        wizard source (index.html, app.js, bootstrap css)
tools/make_assets.py        regenerate portal_assets.h after editing web/
tools/make_gfx_font.py      regenerate the clock fonts
tests/test_parsers.cpp      host unit tests (73 checks) for all parsers
```

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

## Troubleshooting

**Setup network never appears** — the firmware boots straight to the dashboard
once configured; hold BOOT while tapping RST to force the portal.

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
