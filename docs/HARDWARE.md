# Hardware guide

Any ESP32 that Arduino-ESP32 supports, driving any SPI e-paper panel that
[GxEPD2](https://github.com/ZinggJM/GxEPD2) supports. Two choices in
`firmware/epaper_dashboard/config.h` — a **panel** and a **board/wiring**
preset — are the only compile-time configuration.

## Panels

Uncomment exactly one preset (or pass it as a build flag, which is what CI does):

| Preset | Panel | Resolution | Colors | Full refresh |
| --- | --- | --- | --- | --- |
| `PANEL_75_B_V2` *(default)* | Waveshare 7.5" (B) V2/V3 · GDEW075Z08 | 800×480 | black/white/red | ~20–30 s |
| `PANEL_75_BW_V2` | Waveshare 7.5" V2 · GDEW075T7 | 800×480 | black/white | ~2–5 s |
| `PANEL_75_B_V1` | 7.5" (B) V1 (discontinued) | 640×384 | 3-color | ~30 s |
| `PANEL_75_BW_V1` | 7.5" V1 | 640×384 | black/white | ~5 s |
| `PANEL_75_HD_B` | 7.5" HD (B) | 880×528 | 3-color | ~25 s |
| `PANEL_583_B_V2` | 5.83" (B) V2 | 648×480 | 3-color | ~23 s |
| `PANEL_583_BW_V2` | 5.83" V2 | 648×480 | black/white | ~4 s |
| `PANEL_CUSTOM` | **any GxEPD2 class** | any | either | varies |

For `PANEL_CUSTOM`, set the mapping yourself in `config.h`:

```c
#define PANEL_CUSTOM
#define EPD_DRIVER GxEPD2_420c   // any class from GxEPD2's epd/ or epd3c/ family
#define EPD_IS_3C 1              // 1 = black/white/red(yellow), 0 = black/white
```

How the firmware adapts to the panel:

- **Layout** — the 16×12 block grid scales with resolution; the browser editor's
  canvas takes the panel's real aspect ratio automatically.
- **Color** — on black&white panels every red accent folds to black at compile
  time (GxEPD2 would otherwise render red as *white*, i.e. invisible).
- **Fonts** — fixed-size, tuned for ~800×480. The big clock steps down through
  two smaller faces when its block is too narrow, so it never overflows. Panels
  around 640×384 look good; 400×300 works but is dense — expect to remove
  blocks in the layout editor.
- **Refresh cadence** — the wizard's minimum interval is 3 minutes regardless of
  panel type (panel-life and battery considerations, per Waveshare's ≥180 s
  guidance for tri-color). B/W panels simply spend less of each cycle flashing.

> [!NOTE]
> B/w/**yellow** panels use the same `GxEPD2_3C` classes as red ones — accents
> just come out yellow. 4-color and 7-color ACeP panels and parallel-bus panels
> (e.g. LilyGo T5 4.7", which needs `epdiy`) are **not** supported.

## Boards & wiring

### Waveshare e-Paper ESP32 Driver Board — `BOARD_WAVESHARE_DRIVER` *(default)*

The panel plugs straight into the FPC connector; nothing to wire. This board
routes the panel to non-default SPI pins — the firmware remaps automatically:

| Signal | GPIO |
| --- | --- |
| BUSY | 25 |
| RST | 26 |
| DC | 27 |
| CS | 15 |
| SCK | 13 |
| MOSI (DIN) | 14 |

### Any ESP32 dev board + adapter — `BOARD_GENERIC_ESP32`

For a plain ESP32 DevKit with a Waveshare **universal e-Paper Driver HAT**, a
Good Display **DESPI-C02**, or a bare panel adapter — GxEPD2's conventional
wiring (hardware VSPI + three signal pins):

| Panel / HAT | ESP32 GPIO |
| --- | --- |
| BUSY | 4 |
| RST | 16 |
| DC | 17 |
| CS | 5 |
| CLK / SCK | 18 |
| DIN / MOSI | 23 |
| VCC | 3V3 |
| GND | GND |

> [!WARNING]
> Universal e-Paper Driver HAT **rev 2.2/2.3**: the extra **PWR** pin gates the
> panel's power — tie it to **3V3**. Symptoms of a floating PWR pin include a
> dead panel or washed-out, low-contrast output. Also set the HAT's display
> config switch per its silkscreen (B for most Waveshare raw panels).

### Custom wiring — `BOARD_CUSTOM`

Edit the seven `EPD_*` GPIO defines in `config.h` to match your wiring. Any
free GPIOs work — the firmware creates its own SPI bus on whatever pins you
name.

## Other ESP32 chips (S2 / S3 / C3 / C6)

The firmware compiles for the newer single-core/USB variants: the SPI host and
the BOOT-button GPIO (9 on C3, 0 elsewhere) adjust themselves per target. Use a
partition scheme with **two app slots ≥1.5 MB** (the stock "Minimal SPIFFS"
scheme on 4 MB flash, or anything roomier on 8/16 MB boards). These targets are
compile-checked but not hardware-tested — reports welcome.

## Power notes

Between refreshes the ESP32 deep-sleeps (tens of µA on a bare module; dev-board
regulators/LEDs dominate real-world draw) and e-paper holds its image with zero
power. Quiet hours pause refreshes entirely overnight. A 3-minute refresh
interval on a tri-color panel keeps the panel busy ~15% of the time — for
battery builds prefer a b/w panel (short refreshes) and a longer interval.
