#!/usr/bin/env python3
"""Generate an Adafruit GFX font header from a TTF, for a limited char range.
Replicates Adafruit fontconvert.c packing: glyph bitmaps bit-packed MSB-first,
rows concatenated with no per-row padding. yOffset = 1 - bitmap_top.
"""
import sys
import freetype

FONT_PATH = "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
PIXEL_SIZE = 74           # EM pixel size (cap height ~0.70em -> ~84 px digits)
FIRST, LAST = 0x2D, 0x3A   # '0'..'9' and ':'
NAME = "PoppinsBoldTemp"

face = freetype.Face(FONT_PATH)
face.set_pixel_sizes(0, PIXEL_SIZE)

bitstream = []   # list of 0/1
glyphs = []      # (offset_bytes? no: offset in bytes into packed array), per GFX it's byte offset

bit_off = 0
for code in range(FIRST, LAST + 1):
    face.load_char(chr(code), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
    g = face.glyph
    bmp = g.bitmap
    w, h, pitch, buf = bmp.width, bmp.rows, bmp.pitch, bmp.buffer
    offset_bytes = len(bitstream) // 8
    assert len(bitstream) % 8 == 0 or True  # offsets are byte offsets; pad below per glyph
    # GFX packs each glyph starting at a byte boundary? fontconvert does NOT pad:
    # it packs continuously but records bitmapOffset in BYTES only when aligned.
    # Actually fontconvert starts each glyph at the current byte, padding the
    # previous glyph's final byte. Replicate that: pad to byte boundary first.
    while len(bitstream) % 8:
        bitstream.append(0)
    offset_bytes = len(bitstream) // 8
    for r in range(h):
        for c in range(w):
            byte = buf[r * pitch + (c >> 3)]
            bit = (byte >> (7 - (c & 7))) & 1
            bitstream.append(bit)
    glyphs.append({
        "code": code, "offset": offset_bytes, "w": w, "h": h,
        "adv": g.advance.x >> 6, "xo": g.bitmap_left, "yo": 1 - g.bitmap_top,
    })

while len(bitstream) % 8:
    bitstream.append(0)
data = bytearray()
for i in range(0, len(bitstream), 8):
    b = 0
    for j in range(8):
        b = (b << 1) | bitstream[i + j]
    data.append(b)

y_advance = face.size.height >> 6

hdr = []
hdr.append(f"// {NAME}: generated from Poppins-Bold.ttf at {PIXEL_SIZE}px EM,")
hdr.append(f"// chars 0x{FIRST:02X}-0x{LAST:02X} ('0'-'9' and ':'). Adafruit GFX format.")
hdr.append("#pragma once")
hdr.append("#include <Adafruit_GFX.h>")
hdr.append("")
hdr.append(f"const uint8_t {NAME}_Bitmaps[] PROGMEM = {{")
for i in range(0, len(data), 12):
    hdr.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i+12]) + ",")
hdr.append("};")
hdr.append("")
hdr.append(f"const GFXglyph {NAME}_Glyphs[] PROGMEM = {{")
for g in glyphs:
    ch = chr(g["code"]).replace("\\", "\\\\")
    hdr.append(f'  {{ {g["offset"]:5d}, {g["w"]:3d}, {g["h"]:3d}, {g["adv"]:3d}, {g["xo"]:4d}, {g["yo"]:5d} }},   // \'{ch}\'')
hdr.append("};")
hdr.append("")
hdr.append(f"const GFXfont {NAME} PROGMEM = {{")
hdr.append(f"  (uint8_t  *){NAME}_Bitmaps,")
hdr.append(f"  (GFXglyph *){NAME}_Glyphs,")
hdr.append(f"  0x{FIRST:02X}, 0x{LAST:02X}, {y_advance} }};")
hdr.append("")

out = sys.argv[1] if len(sys.argv) > 1 else "ClockFont.h"
with open(out, "w") as f:
    f.write("\n".join(hdr))

# ---- report + self-check ----
digits_w = {chr(g["code"]): g["adv"] for g in glyphs}
sample = lambda s: sum(digits_w[c] for c in s)
print(f"wrote {out}: {len(data)} bytes bitmap, yAdvance={y_advance}")
print(f'widths: "12:34"={sample("12:34")}px  "20:00"={sample("20:00")}px  "8:08"={sample("8:08")}px')
hmax = max(g["h"] for g in glyphs)
print(f"max glyph height={hmax}px, cap top yOffset={min(g['yo'] for g in glyphs)}")

# decode glyph '8' from the packed data and render as ASCII art to verify packing
g8 = next(g for g in glyphs if g["code"] == ord("8"))
art = []
boff = g8["offset"] * 8
for r in range(g8["h"]):
    line = ""
    for c in range(g8["w"]):
        idx = boff + r * g8["w"] + c
        bit = (data[idx >> 3] >> (7 - (idx & 7))) & 1
        line += "#" if bit else "."
    art.append(line)
# print a downsampled ascii art (every 3rd row/col) for eyeballing
for r in range(0, len(art), 4):
    print(art[r][::3])
