# Brand conformance

**DOC NO. RL-320-A · REV. A**

This project follows the [Riposte Laboratories design system](https://github.com/armeehn/riposte-brand)
— published at [ripostelabs.xyz/brand](https://ripostelabs.xyz/brand/) — as far as an
open-source project with outside contributors reasonably can.

---

## Conforms

| Surface | Status |
|---|---|
| README, `DESIGN.md`, `docs/` | Sentence-case headings, tables over prose, concrete claims, numbers bolded |
| `registry/BLOCK_SPEC.md` | Already a specification document in the intended register |
| Voice | Plainspoken and precise. No marketing verbs |
| Firmware log output | Fixed-width, one record per line — spec-sheet chrome by default |

## Deliberately diverges

| Divergence | Why |
|---|---|
| **`web/` uses Bootstrap** (`#0d6efd`, `#212529`, Bootstrap's sans stack) | This is a community open-source project. Bootstrap is what an outside contributor can read, patch and test without learning a house design system first. Contributor throughput outranks brand consistency on a surface that ships to other people's hardware. |
| `#c0392b` accent in `web/index.html` | Part of the same Bootstrap-adjacent palette. Superseded if and when the UI is restyled. |
| Standard OSS furniture — shields.io badges, `CODE_OF_CONDUCT.md`, centred header | Conventional signals that a project is safe to contribute to. Replacing them with `DOC NO.` chrome would make the project look closed. |

**This divergence is a decision, not debt.** Do not open a PR restyling `web/` onto
`riposte-brand.css` without discussing it first — the tradeoff above was made on purpose.

## Where the guide still applies

These carry over regardless, because they are correctness rather than taste:

- **Contrast.** Any Riposte accent used in the UI or in generated e-paper layouts obeys the
  table: bone on bright pink is 3.16:1, bone on bright teal is 2.27:1, both failing WCAG AA
  for body text. Use `#d81150` / `#a15e01` / `#0c7a63` under anything sentence-shaped.
- **E-paper rendering is 1-bit or 3-colour.** The palette is largely moot on-panel; what
  survives is the system's actual structural logic — 2px rules, radius 0, uppercase tracked
  labels, glyphs over icons. Those translate perfectly to a monochrome panel and are the
  reason the default layouts already look like Riposte.
- **Colour is never the only signal.** Especially true here: most panels cannot render
  colour at all.

## The Riposte look (shipped)

The brand is on the panel as an opt-in **look**, one of two, chosen at runtime under
*Clock & refresh* (`firmware/epaper_dashboard/style.h`). It is not a reskin of the
contributor-facing UI and not a block set: a block set would only have restyled
contributed widgets, while the six built-ins (clock, date, weather, forecast, calendar,
inbox) are where most of the ink is. What carries over to 1-bit:

| Rule | On the panel |
|---|---|
| JetBrains Mono 400 / 700 / 800 | `RiposteFonts.h`: 9 pt and 12 pt regular and bold, 18 pt extra-bold, digit faces for the clock (105 px EM) and big numbers (65 px EM) |
| Rules 2px solid | Section rules 2 px, spanning the column; the classic 3 px word-length underline stays classic-only |
| Uppercase tracked labels | Titles and the date in capitals, 1 px tracking; the date reads `WED 23 SEP` |
| Radius 0 | Square bullets in inbox and list widgets |
| Colour is never the only signal | Unchanged: red folds to black on b/w panels in both looks |

## Queued

- [ ] Add the brand colophon to generated documentation only, not to the README (which
      should keep its OSS furniture).

## Quick reference

```
ink      #1d1a17      pink     #f0477d   (bone text; display only, 3.16:1)
bone     #f6f1e7      marigold #fe9a0d   (INK text; 8.12:1, safe for prose)
bone-dim #eae4d6      teal     #12b795   (bone text; fills only, 2.27:1)

deep, for accent fills that carry a sentence:
pink-deep #d81150   marigold-deep #a15e01   teal-deep #0c7a63

font     JetBrains Mono 400 / 700 / 800
spacing  4 8 12 16 24 34 48 64 72     radius 0     rules 2px solid / 1px dashed
```

Full guide: <https://github.com/armeehn/riposte-brand>
