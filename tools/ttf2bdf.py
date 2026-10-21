#!/usr/bin/env python3
# Renders a TTF into a 1-bit BDF for U8g2's bdfconv (needs freetype-py).
# How src/features/nagger/fonts/nsansB13.c was made (Liberation Sans Bold 2.1.5):
#   python tools/ttf2bdf.py LiberationSans-Bold.ttf 13 nsansB13.bdf
#   bdfconv -f 1 -b 0 -m '32-383,183' -n u8g2_font_nsansB13_te -o nsansB13.c nsansB13.bdf
# Keep Reserved Font Names (Liberation, Arimo, Tinos, Cousine) out of the font name.
import sys
import freetype

path, px, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
first, last = (int(x) for x in (sys.argv[4] if len(sys.argv) > 4 else "32-383").split("-"))
face = freetype.Face(path)
face.set_pixel_sizes(0, px)
ascent = face.size.ascender >> 6
descent = -(face.size.descender >> 6)

glyphs = []
for cp in list(range(first, last + 1)) + [0x00B7]:
    if face.get_char_index(cp) == 0:
        continue
    face.load_char(cp, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO)
    g = face.glyph
    bm = g.bitmap
    w, h = bm.width, bm.rows
    rows = []
    rowbytes = (w + 7) // 8
    for y in range(h):
        rows.append(bytes(bm.buffer[y * bm.pitch: y * bm.pitch + rowbytes]).hex().upper())
    if w == 0 or h == 0:
        w, h, rows = 1, 1, ["00"]
        xoff, yoff = 0, 0
    else:
        xoff, yoff = g.bitmap_left, g.bitmap_top - h
    glyphs.append((cp, g.advance.x >> 6, w, h, xoff, yoff, rows))

seen = set()
unique = [gl for gl in glyphs if not (gl[0] in seen or seen.add(gl[0]))]
maxw = max(gl[2] for gl in unique)
with open(out, "w") as f:
    f.write("STARTFONT 2.1\n")
    f.write(f"FONT -nagger-sans-bold-r-normal--{px}-{px*10}-75-75-p-0-iso10646-1\n")
    f.write(f"SIZE {px} 75 75\n")
    f.write(f"FONTBOUNDINGBOX {maxw} {ascent + descent} 0 {-descent}\n")
    f.write("STARTPROPERTIES 3\n")
    f.write('COPYRIGHT "Digitized data (c) 2010 Google Corporation; (c) 2012 Red Hat, Inc.; bitmap version for Nagger, SIL OFL 1.1"\n')
    f.write(f"FONT_ASCENT {ascent}\nFONT_DESCENT {descent}\nENDPROPERTIES\n")
    f.write(f"CHARS {len(unique)}\n")
    for cp, adv, w, h, xoff, yoff, rows in unique:
        f.write(f"STARTCHAR uni{cp:04X}\nENCODING {cp}\nSWIDTH {adv * 1000 // px} 0\nDWIDTH {adv} 0\n")
        f.write(f"BBX {w} {h} {xoff} {yoff}\nBITMAP\n")
        f.write("".join(r + "\n" for r in rows))
        f.write("ENDCHAR\n")
    f.write("ENDFONT\n")
print(f"{len(unique)} glyphs, ascent {ascent}, descent {descent}")
