#!/usr/bin/env python3
"""Generator ikon pulpitu: system/ICONS/*.png -> system/icons_ids.h + build/system/ICONS.DAT

Co robi:
  1. czyta 98 ikon-masterow 48x48 (RGBA) i buduje WSPOLNA palete 216 kolorow
     (RAMDAC 20..235) - dokladnie tak jak dotychczas, wiec sekcja 48 jest
     bit w bit ta sama, co w starym icons_color.h,
  2. dla rozmiarow 32/48/64 liczy maske 1 bpp (prog ALPHA_MIN) + indeksy palety
     8 bpp; 48 to natywny master, 32 i 64 powstaja przez resampling LANCZOS
     z tego samego mastera (nowej grafiki nie rysujemy - patrz PRZEKAZANIE 10v),
  3. zapisuje \\SYSTEM\\ICONS.DAT w formacie v5: naglowek, tabela sekcji, wspolna
     paleta, a potem KAZDA sekcja jako samodzielny blob v1 spakowany LZSS,
  4. zapisuje maly system/icons_ids.h (stale, enum ID, rozmiary) - duze tablice
     danych NIE trafiaja juz do naglowka kompilowanego przez aplikacje.

Dlaczego LANCZOS, a nie skalowanie w jadrze: dane to INDEKSY palety, wiec
interpolacja w runtime wymagalaby szukania najblizszego z 216 kolorow na kazdy
piksel; w praktyce zostaje nearest-neighbor, a 48->64 = 4/3 daje nierowne
piksele.  Resampling w buildzie kosztujezero CPU i dysk ~112 KB na 64 px.
"""
import os, glob, re, struct, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "system", "ICONS")
IDS = os.path.join(ROOT, "system", "icons_ids.h")
SHIM = os.path.join(ROOT, "system", "icons_color.h")
DAT = os.path.join(ROOT, "build", "system", "ICONS.DAT")

SIZES = [32, 48, 64]          # kolejnosc sekcji w ICONS.DAT
DEFAULT = 48                  # rozmiar domyslny (zrzut startowy bez zmian)
PAL_COLORS = 216
ALPHA_MIN = 128

# --- LZSS z tools/icons_rle.py (ten sam, ktory czyta jadro) -----------------
_src = open(os.path.join(HERE, "icons_rle.py")).read()
_ns = {}
exec(_src[:_src.index("def main():")], _ns)
lzss = _ns["lzss"]


def rgb12(r, g, b):
    return ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)


def mask_bytes(size):
    return (size * size + 7) // 8


def ordered_files():
    """Kolejnosc = stabilne ID.  Zrodlem jest ENUM w istniejacym naglowku:
    nowe nazwy dopisujemy na koncu, zeby dodanie PNG nie przesunelo ID."""
    files = sorted(glob.glob(os.path.join(SRC, "*.png")))
    baseline = []
    for cand in (IDS, SHIM):
        if not os.path.exists(cand):
            continue
        try:
            head = open(cand).read()
        except Exception:
            continue
        m = re.search(r"enum \{(.*?)\};", head, re.S)
        if not m:
            continue
        for line in m.group(1).split("\n"):
            tok = line.strip().rstrip(",")
            if not tok.startswith("ICON_"):
                continue
            nm = tok[5:].lower()
            if nm.endswith("_c"):
                nm = nm[:-2]
            if nm and nm not in baseline:
                baseline.append(nm)
        if baseline:
            break
    out = []
    for b in baseline:
        for ext in ("", "_C"):
            cand = os.path.join(SRC, b + ext + ".png")
            if os.path.exists(cand) and cand not in out:
                out.append(cand)
    appended = [os.path.basename(f)[:-4] for f in files if f not in out]
    for f in files:
        if f not in out:
            out.append(f)
    if appended:
        print("new icons appended (existing IDs unchanged):", appended)
    return out


def load_masters(files):
    icons = []
    for f in files:
        im = Image.open(f).convert("RGBA")
        if im.size != (DEFAULT, DEFAULT):
            im = im.resize((DEFAULT, DEFAULT), Image.LANCZOS)
        icons.append((os.path.basename(f)[:-4], im))
    return icons


def shared_palette(icons):
    """Wspolna paleta z WIDOCZNYCH pikseli 48x48 - ta sama metoda co dotychczas,
    zeby paleta (i caly zrzut startowy) pozostala bit w bit."""
    visible = []
    for _, im in icons:
        px = im.load()
        for y in range(DEFAULT):
            for x in range(DEFAULT):
                r, g, b, a = px[x, y]
                if a >= ALPHA_MIN:
                    visible.append((r, g, b))
    if not visible:
        visible = [(0, 0, 0)]
    tmp = Image.new("RGB", (max(len(visible), 1), 1))
    tmp.putdata(visible[:max(len(visible), 1)])
    q = tmp.quantize(colors=min(PAL_COLORS, len(set(visible))),
                     method=Image.Quantize.MAXCOVERAGE, dither=Image.Dither.NONE)
    pal = q.getpalette()
    return [(pal[3 * i], pal[3 * i + 1], pal[3 * i + 2]) for i in range(min(PAL_COLORS, len(pal) // 3))]


def nearest_fn(palette):
    """Najblizszy kolor palety z pamiecia - dla 32/64 resampling tworzy tysiace
    nowych kolorow, a liniowe szukanie 216 pozycji na piksel byloby zbyt wolne."""
    cache = {}

    def nearest(rgb):
        got = cache.get(rgb)
        if got is not None:
            return got
        best, bi = 1 << 30, 0
        for i, (pr, pg, pb) in enumerate(palette):
            d = (rgb[0] - pr) ** 2 + (rgb[1] - pg) ** 2 + (rgb[2] - pb) ** 2
            if d < best:
                best, bi = d, i
        cache[rgb] = bi
        return bi
    return nearest


def size_data(icons, size, palette):
    """(maska, piksele) dla kazdej ikony w danym rozmiarze."""
    mb, pb = mask_bytes(size), size * size
    nearest = nearest_fn(palette)
    out = []
    for _, im in icons:
        src = im if size == DEFAULT else im.resize((size, size), Image.LANCZOS)
        px = src.load()
        mask = bytearray(mb)
        pix = bytearray(pb)
        for y in range(size):
            for x in range(size):
                r, g, b, a = px[x, y]
                if a >= ALPHA_MIN:
                    mask[(y * size + x) >> 3] |= (0x80 >> (x & 7))
                    pix[y * size + x] = nearest((r, g, b))
        out.append((bytes(mask), bytes(pix)))
    return out


def v1_blob(names, data, size, palette, count):
    """Samodzielny blob v1 (naglowek + wpisy + paleta + maski/piksele).
    Uklad zgodny z include/icons_blob.h, wiec jadro czyta go tym samym kodem."""
    mb = mask_bytes(size)
    hdr = 20 + count * 8 + len(palette) * 2
    off = hdr
    entries = []
    for _ in range(count):
        entries.append((off, off + mb))
        off += mb + size * size
    blob = bytearray()
    for v in (0x49434F4E, 1, count, 20 + count * 8, 20):
        blob += struct.pack(">I", v)
    for m, p in entries:
        blob += struct.pack(">II", m, p)
    for r, g, b in palette:
        blob += struct.pack(">H", rgb12(r, g, b))
    for mask, pix in data:
        blob += mask + pix
    return bytes(blob)


def write_ids(names, palette):
    parts = []
    parts.append("/* Generated by tools/mk_icons_color.py - DO NOT EDIT.")
    parts.append("   Stale i ID ikon; DANE (maski/piksele) sa w \\SYSTEM\\ICONS.DAT")
    parts.append("   w formacie v5 (sekcje rozmiarow 32/48/64), zeby nie kompilowac")
    parts.append("   megabajtow tablic w kazdej aplikacji. */")
    parts.append("#ifndef ICONS_IDS_H")
    parts.append("#define ICONS_IDS_H")
    parts.append("#include <stdint.h>")
    parts.append("/* Nominalny rozmiar ikony (kontrakt rysowania w trybie mono). */")
    parts.append("#define ICON_SIZE %du" % DEFAULT)
    parts.append("#define ICON_SIZE_COUNT %du" % len(SIZES))
    parts.append("#define ICON_MAX_SIZE %du" % max(SIZES))
    parts.append("#define ICON_MAX_PIXEL_BYTES (ICON_MAX_SIZE*ICON_MAX_SIZE)")
    parts.append("#define ICON_MAX_MASK_BYTES ((ICON_MAX_SIZE*ICON_MAX_SIZE+7u)/8u)")
    parts.append("#define ICON_MASK_BYTES %du      /* 48x48, zgodnosc wstecz */" % mask_bytes(DEFAULT))
    parts.append("#define ICON_PIXEL_BYTES %du" % (DEFAULT * DEFAULT))
    parts.append("#define ICON_PAL_COLORS %du" % len(palette))
    parts.append("#define ICON_PAL_BASE 20u")
    parts.append("typedef struct {const uint8_t *mask;const uint8_t *pixels;} icon_color_t;")
    parts.append("")
    parts.append("/* Rozmiary dostepne w ICONS.DAT (kolejnosc sekcji w pliku). */")
    parts.append("static const uint16_t icon_sizes[ICON_SIZE_COUNT] = { %s };"
                 % ", ".join("%du" % s for s in SIZES))
    parts.append("static const uint8_t icon_size_index_default = %du;" % SIZES.index(DEFAULT))
    parts.append("")
    parts.append("/* Stabilne ID, w tej samej kolejnosci co wpisy w ICONS.DAT. */")
    parts.append("enum {")
    for i, (name, _) in enumerate(names):
        parts.append("    ICON_%s%s" % (name.upper(), "" if i == len(names) - 1 else ","))
    parts.append("};")
    parts.append("#define ICON_COLOR_COUNT %du" % len(names))
    parts.append("")
    parts.append("#endif")
    with open(IDS, "w") as f:
        f.write("\n".join(parts) + "\n")
    print("wrote", IDS, "icons=%d sizes=%s" % (len(names), SIZES))


def write_shim():
    """Stary include nadal dziala - kieruje do malego naglowka ze stalymi."""
    with open(SHIM, "w") as f:
        f.write("/* Generated by tools/mk_icons_color.py - DO NOT EDIT.\n"
                "   Dane ikon sa w \\SYSTEM\\ICONS.DAT; tu zostaly tylko stale i ID. */\n"
                "#ifndef ICONS_COLOR_H\n#define ICONS_COLOR_H\n#include \"icons_ids.h\"\n#endif\n")


def write_dat(names, palette, per_size):
    count = len(names)
    sections = []
    for size in SIZES:
        mini = v1_blob(names, per_size[size], size, palette, count)
        comp = lzss(mini)
        print("  sekcja %2d px: raw %7d B -> LZSS %7d B" % (size, len(mini), len(comp)))
        sections.append((size, count, mask_bytes(size), size * size, len(mini), comp))

    sizes_n = len(sections)
    pal_off = 20
    table_off = pal_off + len(palette) * 2
    blob_off = table_off + sizes_n * 28
    out = bytearray()
    out += struct.pack(">IIIII", 0x49434F4E, 5, sizes_n, pal_off, table_off)
    for r, g, b in palette:
        out += struct.pack(">H", rgb12(r, g, b))
    off = blob_off
    for size, cnt, mb, pb, raw, comp in sections:
        out += struct.pack(">IIIIIII", size, cnt, mb, pb, raw, off, len(comp))
        off += len(comp)
    for *_rest, comp in sections:
        out += comp
    os.makedirs(os.path.dirname(DAT), exist_ok=True)
    with open(DAT, "wb") as f:
        f.write(out)
    print("wrote", DAT, "%d B (sekcje: %s)" % (len(out), ", ".join("%d px = %d B" % (s, len(c)) for s, *_, c in sections)))


def main():
    files = ordered_files()
    icons = load_masters(files)
    palette = shared_palette(icons)
    print("palette=%d colors, icons=%d" % (len(palette), len(icons)))
    per_size = {}
    for size in SIZES:
        per_size[size] = size_data(icons, size, palette)
        print("  %2d px: maska %d B + piksele %d B na ikone" % (size, mask_bytes(size), size * size))
    write_ids(icons, palette)
    write_shim()
    write_dat(icons, palette, per_size)


if __name__ == "__main__":
    main()
