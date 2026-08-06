# Flashing & updating

Three ways to get firmware onto the board — pick whichever matches your setup.
After the first flash, every later update can go **over the air** from your
browser.

## Option A — no toolchain at all (recommended)

Every push builds real ESP32 binaries in GitHub Actions, one set per panel
preset. Open the repo's **Actions → firmware** run and download the artifact for
your panel (e.g. `epaper-dashboard-75in-800x480-3color-…`). It contains:

| File | Purpose |
| --- | --- |
| `epaper-dashboard-<panel>-factory.bin` | complete image — fresh install at offset `0x0` |
| `epaper-dashboard-<panel>-app.bin` | app only — updates (USB or OTA) |
| `epaper-dashboard-partitions.bin`, `boot_app0.bin` | partition table + boot selector (for migration) |
| `sha256sums-<panel>.txt` | digests to verify uploads |
| `FLASHING-<panel>.txt` | these instructions, offline |

Fresh install with nothing but Python:

```sh
pip install esptool==4.8.1
python3 -m esptool --chip esp32 --port <PORT> --baud 921600 \
    write_flash 0x0 epaper-dashboard-<panel>-factory.bin
```

Or zero-install: a WebSerial flasher in Chrome/Edge (e.g.
[esptool-js](https://espressif.github.io/esptool-js/)) writing the factory
image at offset `0x0`.

`<PORT>`: `/dev/ttyUSB0` (Linux), `/dev/cu.usbserial-…` (macOS), `COM5` (Windows).
Tag a release (`v3.2.0`) and the same files attach to a GitHub Release.

## Option B — Arduino IDE 2.x

1. Boards Manager → install **esp32 by Espressif Systems**.
2. Library Manager → install **GxEPD2** (accept "Install all") and **ArduinoJson** (v7).
3. Open `firmware/epaper_dashboard/epaper_dashboard.ino`; pick your panel/board in `config.h`.
4. Tools → Board **ESP32 Dev Module** · Partition scheme **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** — the two app slots are what make OTA possible.
5. Upload.

## Option C — PlatformIO

```sh
cd firmware && pio run -t upload
```

The OTA partition table is preconfigured in `platformio.ini`.

## Updating over the air

Tap <kbd>RST</kbd>, open `http://epaper-dashboard.local/` during the editor
window, and use the **Update** tab: pick the new `…-app.bin` for your panel,
optionally paste its `sha256sums` line, and *Upload & flash*.

The image streams into the **spare** firmware slot; the received bytes are
checked against your SHA-256 and the image is verified before the device
switches slots and reboots. A dropped connection or wrong file changes
nothing — the running firmware stays put. Settings, layout and blocks live in
their own flash partitions and survive every update. If a new build
misbehaves, upload the previous `app.bin` the same way.

> [!NOTE]
> The full settings portal (<kbd>RST</kbd>, then hold <kbd>BOOT</kbd> ~2 s) has
> the same Update tab, so you can also flash from the device's own hotspot.

## Migrating from a pre-OTA install

Firmware v3.0 and earlier used the single-slot **Huge APP** partition table —
no room for a second firmware copy, so the Update tab will tell you a one-time
USB flash is needed. This variant **keeps WiFi and all wizard settings** (NVS
is untouched); only the saved layout and installed blocks reset, because the
data partition moves:

```sh
python3 -m esptool --chip esp32 --port <PORT> --baud 921600 \
    write_flash 0x8000 epaper-dashboard-partitions.bin \
                0xe000 boot_app0.bin \
                0x10000 epaper-dashboard-<panel>-app.bin
```

Every update after this one can go over the air.

## USB app-only update (OTA-table devices)

```sh
python3 -m esptool --chip esp32 --port <PORT> --baud 921600 \
    write_flash 0xe000 boot_app0.bin 0x10000 epaper-dashboard-<panel>-app.bin
```

Writing `boot_app0.bin` resets the boot selector to the first slot — needed in
case an earlier OTA update had switched the device to the second one.
