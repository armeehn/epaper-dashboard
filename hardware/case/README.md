# 7.5" Tri-Color e-Paper Frame Case

A desk + wall picture-frame case for the Waveshare 7.5" red/black/white e-paper raw panel
(800x480, ASIN B09JSFTGV6), a custom ESP32 carrier board (38-pin DevKitC with USB-C,
headers removed), a TP4056-style USB-C charger module, and a LiPo pouch cell.

Overall size: **188.8 x 123.5 x 23.7 mm**, uniform 11.8 mm frame border around the image.
Both USB-C ports exit the **right edge**: the DevKitC port (lower) and the charger port (upper).
The back lid carries all the electronics, so opening the case never disturbs the panel.

## Printed parts

| File        | Part                            | Print orientation        | Supports |
|-------------|---------------------------------|--------------------------|----------|
| `body.stl`  | Front frame                     | Face down (as exported)  | No       |
| `lid.stl`   | Rear lid / electronics chassis  | Back face down           | No       |
| `plate.stl` | Board mounting plate            | Flat, standoffs up       | No       |
| `stand.stl` | Desk cradle, 12 degree lean     | Base down                | No       |

Suggested settings: PETG or PLA, 0.2 mm layers, 3 perimeters, 15 percent infill.
The body needs a 200 x 130 mm bed minimum (fits any 220x220 printer).

## Hardware (BOM)

- 4x M3 x 12 self-tapping screws (lid corners into the body pillars)
- 4x M3 x 6 self-tapping screws (plate onto the lid bosses)
- 4x M2.5 x 6 self-tapping screws (your board onto the plate standoffs)
- 1x TP4056-style USB-C charger module (~28 x 17.5 mm), slides into the printed rails
- 1x LiPo pouch cell up to 50 x 34 x 11 mm (103450 or smaller)
- 1 mm foam tape strips (frame the panel border, front ledge and rear rim)
- Optional: 5V boost module if your carrier board expects 5V rather than raw LiPo voltage

## Before you print: three measurements

The model is fully parametric (`epaper_frame.scad`, top of file). Three values are
guesses about YOUR hardware and are worth checking, everything else is from the panel
datasheet:

1. **`aa_top` (default 3.4)**: distance from the top edge of the glass to the start of
   the active (image) area, in mm. The panel's borders are asymmetric: about 3.5 on the
   sides and top, about 9.9 on the ribbon side. If your measurement differs, edit it,
   otherwise the window may show a sliver of white border.
2. **`board_hole_dx` / `board_hole_dy` (53 x 33)**: mounting hole spacing of your custom
   board, plus `board_l` / `board_w` (60 x 40) for its outline. Only the small plate
   needs reprinting if you got these wrong after printing.
3. **`usb_h` (default 14.8)**: height of the DevKitC USB-C connector center above the
   lid's inner floor once the board is screwed to the plate. Stack it up: boss 3.5 +
   plate 2 + standoff 2.5 + carrier PCB + gap + DevKitC PCB + half the connector.
   The wall slot is 7.5 mm tall so there is about 3 mm of forgiveness.

Regenerate any part after editing:

```
openscad -o body.stl  -D 'part="body"'  epaper_frame.scad
openscad -o lid.stl   -D 'part="lid"'   epaper_frame.scad
openscad -o plate.stl -D 'part="plate"' epaper_frame.scad
openscad -o stand.stl -D 'part="stand"' epaper_frame.scad
```

`part="assembly"`, `"exploded"`, `"section"`, and `"section2"` give preview/inspection views.

If your ports need to exit the LEFT edge instead, just mirror `body.stl` and `lid.stl`
in your slicer (and `plate.stl` if your hole pattern is asymmetric).

## Assembly

1. Stick 1 mm foam tape on the panel pocket ledge inside the body (on the border zone,
   not the visible window area). Lay the panel in face down, ribbon side toward the
   bottom edge. Add foam strips on the panel's rear border where the lid rim will land.
2. Fold the ribbon gently around the panel's bottom edge toward the back. Never crease
   it toward the front of the screen, and fold it only once. It passes through the
   34 mm relief notch in the lid rim's bottom edge.
3. Screw your custom board to the plate standoffs (M2.5). Screw the plate to the two
   lid bosses (M3 x 6) with the DevKitC USB-C facing the right rim wall.
4. Slide the charger module into its rails near the top-right, USB-C outward,
   components facing the lid floor so the charge LEDs show through the peek slot.
   The small nubs snap over the PCB.
5. Put the battery in the fenced pocket (foam pad under it, strap through the fence
   slots if you want it extra secure). Route wires: battery -> charger B+/B-,
   charger OUT -> your board's power input. If your board wants 5V on VIN, put a
   boost module between charger OUT and the board (zip-tie bridges are provided
   on the lid floor next to the fence).
6. Plug the panel ribbon into the board's FPC connector, lower the lid straight in
   (rim slides inside the body walls), and drive the 4 corner screws (M3 x 12).

## Desk and wall

- **Desk**: drop the case into the cradle stand, it leans back 12 degrees. The lip is
  low enough to stay clear of the window, and the side ports stay reachable.
- **Wall**: two keyholes on the back, 80 mm apart horizontally, sized for M4/#8 pan
  head screws (8.5 mm head max). Hang, then slide down.

## Notes

- The charger USB-C charges the battery. The DevKitC USB-C is for flashing; most
  DevKitC boards will also try to power the circuit from it, which is fine briefly.
  Avoid leaving both plugged in long-term unless your carrier handles that properly.
- Use a protected cell or a charger module with protection (most TP4056 boards have it).
- E-paper and LiPo both dislike heat: keep the frame out of direct sun, especially
  wall-mounted.
