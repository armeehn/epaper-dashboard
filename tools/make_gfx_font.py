#!/usr/bin/env python3
"""Generate an Adafruit GFX font header from a TTF, for a limited char range.
Replicates Adafruit fontconvert.c packing: glyph bitmaps bit-packed MSB-first,
rows concatenated with no per-row padding. yOffset = 1 - bitmap_top.

Importable: gfx_font() returns the header text for one font, so a script that
bundles several faces into one header (tools/make_riposte_fonts.py) shares the
packing with the digit fonts generated here.
"""
import sys
import freetype

FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
PIXEL_SIZE = 105           # EM pixel size (cap height ~0.70em -> ~84 px digits)
FIRST, LAST = 0x30, 0x3A   # '0'..'9' and ':'
NAME = "DashClockFont"
GFX_DPI = 141              # fontconvert's DPI: 9pt there is a 17.6px EM here


def pack(face, first, last):
    """Bit-pack every glyph in [first, last] the way fontconvert does."""
    bitstream = []   # list of 0/1
    glyphs = []

    for code in range(first, last + 1):
        face.load_char(chr(code), freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
        g = face.glyph
        bmp = g.bitmap
        w, h, pitch, buf = bmp.width, bmp.rows, bmp.pitch, bmp.buffer
        # fontconvert starts each glyph at the current byte, padding the
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

    return data, glyphs


def gfx_font(font_path, name, first, last, px=None, pt=None):
    """Header text (no #pragma once / include) for one face at one size.

    px sets the EM in pixels, as the digit fonts do; pt sets a point size at
    fontconvert's 141 dpi, so a 9pt here matches FreeSans9pt7b's metrics.
    Returns (text, data, glyphs, y_advance).
    """
    face = freetype.Face(font_path)
    if pt is not None:
        face.set_char_size(int(pt * 64), 0, GFX_DPI, 0)
        size_note = f"{pt}pt @ {GFX_DPI}dpi"
    else:
        face.set_pixel_sizes(0, px)
        size_note = f"{px}px EM"

    data, glyphs = pack(face, first, last)
    y_advance = face.size.height >> 6
    base = font_path.rsplit("/", 1)[-1]

    hdr = []
    hdr.append(f"// {name}: generated from {base} at {size_note},")
    hdr.append(f"// chars 0x{first:02X}-0x{last:02X}. Adafruit GFX format.")
    hdr.append(f"const uint8_t {name}_Bitmaps[] PROGMEM = {{")
    for i in range(0, len(data), 12):
        hdr.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i+12]) + ",")
    hdr.append("};")
    hdr.append("")
    hdr.append(f"const GFXglyph {name}_Glyphs[] PROGMEM = {{")
    for g in glyphs:
        ch = chr(g["code"]).replace("\\", "\\\\")
        hdr.append(f'  {{ {g["offset"]:5d}, {g["w"]:3d}, {g["h"]:3d}, {g["adv"]:3d}, {g["xo"]:4d}, {g["yo"]:5d} }},   // \'{ch}\'')
    hdr.append("};")
    hdr.append("")
    hdr.append(f"const GFXfont {name} PROGMEM = {{")
    hdr.append(f"  (uint8_t  *){name}_Bitmaps,")
    hdr.append(f"  (GFXglyph *){name}_Glyphs,")
    hdr.append(f"  0x{first:02X}, 0x{last:02X}, {y_advance} }};")
    hdr.append("")

    return "\n".join(hdr), data, glyphs, y_advance


def main():
    text, data, glyphs, y_advance = gfx_font(FONT_PATH, NAME, FIRST, LAST, px=PIXEL_SIZE)
    hdr = ["#pragma once", "#include <Adafruit_GFX.h>", "", text]

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


if __name__ == "__main__":
    main()
