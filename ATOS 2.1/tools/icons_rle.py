#!/usr/bin/env python3
"""Skompresuj ICONS.DAT (v1, surowy) do wersji 4 (LZSS).
    v2 = RLE par (count,value)          ~129 KB
    v3 = PackBits (0x80|n + literaly)   ~107 KB
    v4 = LZSS, okno 4095, 8 tokenow pod bajtem flag, dopasowanie 2 B
         (12 bitow offsetu + 4 bity dlugosci, dlugosc=pole+3)  ~67 KB  <- domyslna
    Dekoder v4 jest w kgfx_icons_load (kernel_graphics.c).  Stare pliki v2/v3
    nadal sie wczytuja.

Format v2: naglowek i wpisy BEZ ZMIAN, a CALY obszar danych (od najnizszego
offsetu maski/pikseli do konca pliku) jest zastapiony strumieniem RLE:
    uint32 BE  raw_size        - rozmiar danych po dekompresji
    potem pary (count, value)  - count 1..255 bajtow wartosci value
Dekoder w jadrze (kgfx_icons_load) rozwija to do bufora, a offsety wpisow
dzialaja dalej, bo uklad po dekompresji jest IDENTYCZNY z v1.
Wersja 1 jest nadal czytana (zgodnosc wstecz).
"""
import struct, sys, pathlib

def packbits(data):
    """v3: sterowanie 0x80|n = n+1 literalow (n<=127), n = n+1 powtorzen.
    Lepiej od RLE par (v2), bo nie marnuje 2 B na kazdy 1-bajtowy przebieg
    (ikony sa ditheringowane, wiec krotkie przebiegi sa czeste)."""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        j = i + 1
        while j < n and j - i < 128 and data[j] == data[i]:
            j += 1
        run = j - i
        if run >= 2:
            out.append(run - 1)
            out.append(data[i])
            i = j
        else:
            k = i
            lit = bytearray()
            while k < n and len(lit) < 128:
                e = k + 1
                while e < n and e - k < 128 and data[e] == data[k]:
                    e += 1
                if e - k >= 2:
                    break
                lit.append(data[k])
                k += 1
            out.append(0x80 | (len(lit) - 1))
            out += lit
            i = k
    return bytes(out)

def rle(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        v = data[i]
        j = i + 1
        while j < n and j - i < 255 and data[j] == v:
            j += 1
        out.append(j - i)
        out.append(v)
        i = j
    return bytes(out)

def lzss(data, window=4095, minlen=3, maxlen=18):
    # UWAGA: offset ma 12 bitow, wiec okno MUSI byc <= 4095.  Przy 4096 offset
    # 4096 obcinal sie do 0 (&0xff) i strumien sie rozjezdzal (ikony nie wstawaly).
    """v4: LZSS - 8 tokenow pod jednym bajtem flag, dopasowanie 12 bitow
    offsetu + 4 bity dlugosci (dlugosc = pole+3, czyli 3..18), okno 4096.
    Na ICONS.RAW daje ~66 KB zamiast 107 KB (PackBits) przy dekoderze
    wielkosci ~40 linii C - patrz kgfx_icons_load (wersja 4)."""
    out = bytearray()
    i = 0
    n = len(data)
    head = {}                     # 2-bajtowy prefiks -> lista pozycji
    while i < n:
        flags = 0
        body = bytearray()
        bit = 0
        while bit < 8 and i < n:
            best_len = 0
            best_off = 0
            if i + minlen <= n:
                key = data[i:i+2]
                cand = head.get(key, ())
                for j in reversed(cand[-64:]):
                    if i - j > window:
                        break
                    l = 0
                    while l < maxlen and i + l < n and data[j+l] == data[i+l]:
                        l += 1
                    if l > best_len:
                        best_len = l
                        best_off = i - j
                        if l == maxlen:
                            break
            if best_len >= minlen:
                flags |= (1 << bit)
                body.append((best_off >> 4) & 0xff)
                body.append(((best_off & 0x0f) << 4) | (best_len - minlen))
                for k in range(best_len):
                    p = i + k
                    if p + 1 < n:
                        head.setdefault(data[p:p+2], []).append(p)
                i += best_len
            else:
                body.append(data[i])
                if i + 1 < n:
                    head.setdefault(data[i:i+2], []).append(i)
                i += 1
            bit += 1
        out.append(flags)
        out += body
    return bytes(out)

def main():
    src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "build/system/ICONS.DAT")
    dst = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else str(src) + ".rle")
    d = src.read_bytes()
    magic, ver, count, pal_off, ent_off = struct.unpack(">IIIII", d[:20])
    assert magic == 0x49434F4E, "to nie ICONS.DAT"
    assert ver == 1, "juz skompresowany?"
    # najnizszy offset danych = min(mask_off, pixels_off)
    first = None
    for k in range(count):
        m, p = struct.unpack(">II", d[ent_off + k * 8: ent_off + k * 8 + 8])
        for o in (m, p):
            if first is None or o < first:
                first = o
    raw = d[first:]
    comp = lzss(raw)                    # v4 (LZSS, okno 4 KB)
    out = bytearray(d[:first])
    out[4:8] = struct.pack(">I", 4)                     # version = 4 (LZSS)
    out += struct.pack(">I", len(raw)) + comp
    dst.write_bytes(out)
    print(f"{src.name}: {len(d)} B -> {len(out)} B "
          f"({100*len(out)//len(d)}%), danych {len(raw)} -> {len(comp)} B")

if __name__ == "__main__":
    main()
