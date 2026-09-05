# 7.5" Tri-Color e-Paper Frame Case

A desk + wall picture-frame case for the Waveshare 7.5" red/black/white e-paper raw panel
(800x480, ASIN B09JSFTGV6), a custom ESP32 carrier board (38-pin DevKitC with USB-C,
headers removed), a TP4056-style USB-C charger module, and a LiPo pouch cell.

Overall size: **189.4 x 124.1 x 24.7 mm**, uniform 11.5 mm frame border around the image.
Both USB-C ports exit the **right edge**: the DevKitC port (lower) and the charger port (upper).
The back lid carries all the electronics, so opening the case never disturbs the panel.

## The panel pocket is empty on purpose

The glass is 170 x 111 x 1.3 mm, loaded from the open back, and cannot be trimmed, bent
or persuaded. So the pocket is the only thing in its path: no pillars, no bosses, no ribs,
nothing. The screws that hold the lid tap into the solid left and right side walls instead
of standing in the corners of the pocket.

Three details exist only to make the glass go in and come out again:

- **0.6 mm clearance per side.** A 170 mm pocket in PETG prints within roughly +/-0.4 mm
  of nominal, so a tighter number can leave the printer as an interference fit.
- **Corner relief.** The pocket has filleted corners and the glass does not. Four 3 mm
  reliefs give the square corners somewhere to sit.
- **Finger scallops** in the pocket side walls, so the panel can be lifted straight out
  once the foam has taken hold, without prying anything against the face.

`./fit_check.sh` proves all of this against the model and fails the build if a future edit
puts anything back into the pocket or makes the lid and body overlap. Run it after any
parameter change.

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

- 4x M3 x 12 self-tapping screws (through the lid, into the body's side walls)
- 4x M3 x 6 self-tapping screws (plate onto the lid bosses)
- 4x M2.5 x 6 self-tapping screws (your board onto the plate standoffs)
- 1x TP4056-style USB-C charger module (~28 x 17.5 mm), slides into the printed rails
- 1x LiPo pouch cell up to 50 x 34 x 11 mm (103450 or smaller)
- 1 mm foam tape strips, two runs: one on the front ledge under the glass, one on the
  panel's rear border. Both are in the model, so do not skip either or add a third.
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

Regenerate everything after editing:

```
./fit_check.sh     # panel path clear, lid clears the body, asserts hold
./render.sh        # all four STLs and all six previews
```

Or one part at a time:

```
openscad -o body.stl -D 'part="body"' epaper_frame.scad
```

`part="assembly"`, `"exploded"`, `"section"` and `"section2"` give preview/inspection
views. `part="obstruction"` and `part="clash"` are the fit checks and must render empty.

If your ports need to exit the LEFT edge instead, just mirror `body.stl` and `lid.stl`
in your slicer (and `plate.stl` if your hole pattern is asymmetric).

## Assembly

1. Stick 1 mm foam tape on the panel pocket ledge inside the body, on the border zone,
   not the visible window area. Lay the panel in face down, ribbon side toward the
   bottom edge. It drops straight in; if it does not, your pocket printed undersize,
   so scale or reprint rather than forcing it. Add foam strips on the panel's rear
   border where the lid rim will land.
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
6. Plug the panel ribbon into the board's FPC connector, lower the lid straight on
   (the rim slides inside the body walls, the flange lands on the rear shoulder),
   and drive the 4 corner screws (M3 x 12).

To take the panel out again: remove the lid, then reach into the finger scallops on the
left and right of the pocket and lift the glass straight back. Never lever against the
face and never pull on the ribbon.

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
