#!/usr/bin/env python3
"""Put a preview PPM in the bezel the docs screenshots use, as PNG.

    python3 tools/frame_preview.py preview_riposte.ppm docs/dashboard_riposte.png

The frame matches docs/dashboard_blocks.png pixel for pixel: a 4 px page
margin, a 19 px dark bezel and a 3 px inner edge round the panel, so a
regenerated shot sits beside the older ones without a seam. Requires Pillow.
"""
import sys
from PIL import Image

MARGIN = (4, (238, 234, 226))
BEZEL = (19, (34, 34, 36))
EDGE = (3, (20, 20, 20))


def frame(src, dst):
    panel = Image.open(src).convert("RGB")
    rings = [EDGE, BEZEL, MARGIN]      # innermost first
    pad = sum(w for w, _ in rings)
    out = Image.new("RGB", (panel.width + 2 * pad, panel.height + 2 * pad), MARGIN[1])

    # Paint the rings from the outside in, each a filled rectangle the next
    # one sits inside, then drop the panel in the middle.
    inset = 0
    for w, colour in reversed(rings):
        box = Image.new("RGB", (out.width - 2 * inset, out.height - 2 * inset), colour)
        out.paste(box, (inset, inset))
        inset += w
    out.paste(panel, (pad, pad))
    out.save(dst, optimize=True)
    print(f"wrote {dst}: {out.width}x{out.height}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    frame(sys.argv[1], sys.argv[2])
