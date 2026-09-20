# Low-cost build

The cheapest way to put this dashboard on many walls. List prices below were
read from the vendors' own pages on **2026-09-20**, in US dollars, before
shipping and tax; Canadian landed cost runs about 1.5× once courier and duty
are in. Everything here is a supported preset, so a batch is flash-and-go.

## The pick: US$40.49 a panel, 7.5", 800×480

| Part | Vendor | Price |
| --- | --- | --- |
| 7.5" 800×480 b/w panel, GDEY075T7 | [Good Display store](https://www.buy-lcd.com/products/gdey075t7) | $29.69 |
| XIAO ESP32-C3 | [Seeed](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html) | $4.90 |
| ePaper Driver Board for XIAO (24-pin FPC) | [Seeed](https://www.seeedstudio.com/ePaper-breakout-Board-for-XIAO-V2-p-6374.html) | $5.90 |
| **Total** | | **$40.49** |

Firmware: `PANEL_75_BW_GDEY` + `BOARD_XIAO_EPAPER`, or the
`xiao-c3-75in-800x480-bw` image from CI. Wiring is fixed by the driver board
(pins in [HARDWARE.md](HARDWARE.md#xiao-esp32-c3-on-the-xiao-epaper-driver-board--board_xiao_epaper)).
Power is the XIAO's own USB-C; the driver board has a JST-PH battery input if
a panel has to hang where there is no outlet. Seeed's own 7.5" mono panel
([$35.00](https://www.seeedstudio.com/7-5-Monochrome-ePaper-Display-with-800x480-Pixels-p-5788.html))
is the same UC8179 glass and works on the same preset when Good Display is out
of stock.

**Why this and not the default Waveshare pair:** the Waveshare 7.5" raw panel
is [$47.99](https://www.waveshare.com/7.5inch-e-paper.htm) and its ESP32 driver
board [$14.99](https://www.waveshare.com/e-paper-esp32-driver-board.htm),
**$62.98** for the same 800×480. The saving is $22 a panel, all of it on the
glass and the radio, none of it on the screen you look at.

## Zero assembly: US$75, 7.5", cased, with a battery

[Seeed XIAO 7.5" ePaper Panel](https://www.seeedstudio.com/XIAO-7-5-ePaper-Panel-p-6416.html):
the pick above already in an enclosure with a 2 000 mAh cell, a power switch and
USB-C charging. Same firmware preset, no soldering, no case to print. Worth the
extra $35 for the two or three panels that go where guests see them; for the
rest, the bare pick and a printed case ([hardware/case/](../hardware/case/)
with `power = "usb"`) is cheaper.

## Single number on a wall: US$22.53, 4.2", 400×300

| Part | Vendor | Price |
| --- | --- | --- |
| 4.2" 400×300 b/w panel, GDEY042T81 | [Good Display store](https://www.buy-lcd.com/products/42-inch-e-paper-display-morochrome-spi-e-ink-for-digital-price-tags-gdey042t91-578) | $11.73 |
| XIAO ESP32-C3 | Seeed | $4.90 |
| ePaper Driver Board for XIAO | Seeed | $5.90 |
| **Total** | | **$22.53** |

Firmware: `PANEL_42_BW` + `BOARD_XIAO_EPAPER`. At 400×300 the 16×12 grid is
25 px cells and the default six-block layout does not fit: lay out **one or two
blocks** in the editor (a big-number over a list, a clock beside a bar). That is
the right size for "is the estate green", "how many unread", "what is on
Channel Z0", not for the whole dashboard. Waveshare's 4.2" raw panel is
[$15.99](https://www.waveshare.com/4.2inch-e-paper.htm) if Good Display is
out of stock; it is the same GDEY042T81 glass.

## What was ruled out

- **Parallel panels** (LILYGO T5 4.7", the epdiy boards): not SPI, not
  GxEPD2, so not this firmware.
- **Waveshare's universal driver HAT + a bare ESP32** ($9.99 + ~$4): cheaper
  than the XIAO pair only on paper. It is a wired build with a dangling
  breakout, and the classic ESP32 draws more in deep sleep than the C3.
- **3-colour panels**: same glass price, 25 s refreshes, and the Riposte look
  reads as well in one ink. Only worth it where red is the point.
- **Anything needing an API token** for its price: the block system refuses
  secrets, so a data source that wants one is out regardless of the hardware.

## Batch notes

- Order panels and driver boards together; the FPC is the part that gets bent
  in a bag of loose glass.
- Flash all boards with the same CI image, then let each one join the
  `EPaper-Dashboard` hotspot and answer the wizard. A layout is JSON:
  `GET /api/layout` on the first panel, `POST` it to the rest.
- The setup gesture on a XIAO is the same as on any C3: tap **RST**, then hold
  **BOOT** (GPIO 9).
