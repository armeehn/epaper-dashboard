# Troubleshooting

**Setup network never appears** — once configured, the firmware boots straight
to the dashboard. Tap <kbd>RST</kbd>, then hold <kbd>BOOT</kbd> ~2 s to force
the portal back open.

**Screen frozen / device seems dead / won't reset** — first tap <kbd>RST</kbd>
*alone*: if <kbd>BOOT</kbd> was held during a reset the chip is sitting in ROM
flashing mode, and a plain reset exits it. If the screen still never refreshes,
open the Serial Monitor at **115200** and tap <kbd>RST</kbd> — the firmware
prints every stage (wifi, mail, calendar, weather, render) with free-heap
numbers, and a crash prints a backtrace; the last line tells you which stage
failed. The firmware also self-heals: two dead cycles in a row skip the
calendar fetch (heaviest step), four skip all fetches — the clock always comes
back.

**Blank panel or "Busy timeout" in the serial log** — reseat the ribbon cable;
check the panel preset in `config.h` matches the sticker on your panel. On a
universal Driver HAT, check the display-config switch position.

**Washed-out / low-contrast output on a Driver HAT rev 2.2/2.3** — the HAT's
**PWR** pin must be tied to 3V3 (see [HARDWARE.md](HARDWARE.md)).

**Garbage pixels, shifted image, or mirrored output** — wrong panel preset:
same-size panels from different generations use different controllers. Try the
other preset for your size (e.g. `PANEL_75_B_V2` vs `PANEL_75_B_V1`).

**Wizard page stalls right after joining WiFi** — expected for ~10 s: the
hotspot hops to your router's channel. Stay on the setup WiFi; the page
retries by itself.

**IMAP test fails with "login rejected"** — Migadu/mailbox.org: full address +
normal password. Fastmail/Gmail/iCloud: you need an app password.
Outlook.com/Hotmail cannot work (OAuth-only — see [SETUP.md](SETUP.md)).

**"Find my calendars" finds nothing** — paste the full calendar URL into the
server field (from your calendar app's settings), or switch to an ICS link.
The test button reports which step failed (401 = credentials, 404 = URL…).

**Layout changes don't show on the panel** — use **Save & preview on panel** in
the Layout tab: it saves the canvas you're looking at, then renders exactly
that. Saved layouts also apply on every scheduled refresh.

**Update tab says there's no spare OTA slot** — the device still runs the old
single-slot `huge_app` table. Do the one-time USB migration (keeps settings):
[FLASHING.md](FLASHING.md#migrating-from-a-pre-ota-install).

**Compile error "text section exceeds available space"** — you skipped the
**Minimal SPIFFS (1.9MB APP with OTA)** partition scheme in Tools.

**`epaper-dashboard.local` doesn't resolve** — some networks block mDNS; use
the IP shown in the dashboard footer instead (give it a DHCP reservation).
