#!/usr/bin/env python3
"""Generate an 8x8 font header from a BDF (Atari ST normal font).
The ST 80-column font is exactly 8x8, which is what lets 80x25 characters
fit a window on a 640x400 screen."""
import re,sys

src=open(sys.argv[1],encoding="latin-1").read().splitlines()
glyphs={}
enc=None
in_bitmap=False
rows=[]
for line in src:
    t=line.strip()
    if t.startswith("ENCODING"):
        enc=int(t.split()[1])
    elif t.startswith("BITMAP"):
        in_bitmap=True; rows=[]
    elif t.startswith("ENDCHAR"):
        if in_bitmap and enc is not None and 0<=enc<256:
            # The source BDF is 8x16. Downscale to 8x8 by OR-ing row pairs:
            # truncating to the first 8 rows decapitates lowercase letters,
            # whose pixels live in the bottom half of the 16 row cell.
            r=list(rows)
            while len(r)<16: r.append(0)
            if len(r)>=16:
                glyphs[enc]=[r[2*i]|r[2*i+1] for i in range(8)]
            else:
                glyphs[enc]=r[:8]+[0]*(8-len(r))
        in_bitmap=False; enc=None
    elif in_bitmap and re.fullmatch(r"[0-9A-Fa-f]{2}", t):
        rows.append(int(t,16))

out=[]
out.append("#ifndef ASF_FONT8_H")
out.append("#define ASF_FONT8_H")
out.append("#include <stdint.h>")
out.append("/* 8x8 Atari ST font: 80x25 cells occupy 640x200 pixels. */")
out.append("static const uint8_t emutos_font8[256][8]={")
for c in range(256):
    g=glyphs.get(c,[0,0,0,0,0,0,0,0])
    out.append("{"+",".join("0x%02x"%b for b in g)+"},")
out.append("};")
out.append("#endif")
hdr="\n".join(out)+"\n"

path=sys.argv[2]
with open(path,"w") as f: f.write(hdr)
print("glyphs=%d bytes=%d -> %s"%(len(glyphs),len(hdr),path))
print("sample A rows:", " ".join("%02x"%b for b in glyphs.get(65,[0]*8)))
print("sample space:", " ".join("%02x"%b for b in glyphs.get(32,[0]*8)))
