"""Frame an 800x480 preview .ppm the way docs/dashboard_blocks.png is framed.

    python3 tools/frame_shot.py preview_blocks.ppm docs/dashboard_blocks.png

The README shots are 852x532: the scene inside a dark rounded bezel on the
site's off-white. Nothing else in the repo makes that frame, so this does.
"""
import sys
from PIL import Image, ImageDraw

MARGIN = 26           # bezel + margin each side: (852 - 800) / 2
BEZEL = 8             # dark rim width
RADIUS = 14
PAPER = (240, 239, 233)
INK = (30, 30, 30)

src = Image.open(sys.argv[1]).convert("RGB")
w, h = src.size
out = Image.new("RGB", (w + 2 * MARGIN, h + 2 * MARGIN), PAPER)
d = ImageDraw.Draw(out)
d.rounded_rectangle([MARGIN - BEZEL, MARGIN - BEZEL, w + MARGIN + BEZEL - 1, h + MARGIN + BEZEL - 1],
                    radius=RADIUS, fill=INK)
out.paste(src, (MARGIN, MARGIN))
out.save(sys.argv[2], optimize=True)
print(out.size, sys.argv[2])
