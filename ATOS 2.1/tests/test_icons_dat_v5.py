"""ICONS.DAT format v5 (sekcje rozmiarow 32/48/64) - test hostowy.

Pilnuje czterech rzeczy, ktore latwo zepsuc po cichu:
  1. naglowek v5: magic, wersja, liczba sekcji, offset tabeli sekcji,
  2. sekcje: px/mask_bytes/pixel_bytes/count/raw_size sa spojne (raw_size =
     naglowek v1 + wpisy + paleta + dane), offsety nie wychodza za plik,
  3. kazda sekcja ROZPAKOWUJE SIE do ukladu v1 o dokladnie raw_size bajtow,
     a wpisy wskazuja w granicach bufora,
  4. dane sa PRAWDZIWE: maska ikony z sekcji 64 odpowiada progowej alfie PNG
     (ALPHA_MIN=128) przeskalowanego do 64 px - czyli resampling naprawde
     trafia do pliku, a nie tylko "jest wiekszy".

Uruchamiane z `make check-graphics`. Wymaga build/system/ICONS.DAT (generator
tools/mk_icons_color.py).
"""
from pathlib import Path
import glob
import os
import struct
import unittest

ROOT = Path(__file__).resolve().parents[1]
DAT = ROOT / "build" / "system" / "ICONS.DAT"
SRC = ROOT / "system" / "ICONS"
ALPHA_MIN = 128


def lzss_decode(comp, raw):
    out = bytearray()
    i = 0
    n = len(comp)
    while i < n and len(out) < raw:
        flags = comp[i]
        i += 1
        for bit in range(8):
            if i >= n or len(out) >= raw:
                break
            if flags & (1 << bit):
                if i + 1 >= n:
                    return None
                lo, hi = comp[i], comp[i + 1]
                i += 2
                off = (lo << 4) | (hi >> 4)
                ln = (hi & 0x0F) + 3
                if off == 0 or off > len(out):
                    return None
                start = len(out) - off
                for k in range(ln):
                    if len(out) >= raw:
                        break
                    out.append(out[start + k])
            else:
                out.append(comp[i])
                i += 1
    return bytes(out) if len(out) == raw else None


class IconsDatV5(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not DAT.exists():
            raise unittest.SkipTest("brak build/system/ICONS.DAT (make najpierw)")
        cls.raw = DAT.read_bytes()

    def header(self):
        magic, ver, nsizes, pal_off, tab_off = struct.unpack(">IIIII", self.raw[:20])
        return magic, ver, nsizes, pal_off, tab_off

    def sections(self):
        magic, ver, nsizes, pal_off, tab_off = self.header()
        out = []
        for i in range(nsizes):
            f = struct.unpack(">IIIIIII", self.raw[tab_off + i * 28: tab_off + i * 28 + 28])
            out.append(dict(zip(("px", "count", "mask_bytes", "pixel_bytes",
                                 "raw_size", "blob_off", "blob_size"), f)))
        return out

    def test_header(self):
        magic, ver, nsizes, pal_off, tab_off = self.header()
        self.assertEqual(magic, 0x49434F4E, "magic 'ICON'")
        self.assertEqual(ver, 5, "format v5 (sekcje rozmiarow)")
        self.assertEqual(nsizes, 4, "sekcje 16/32/48/64")
        self.assertEqual(pal_off, 20, "paleta zaraz za naglowkiem")
        self.assertEqual(tab_off, 20 + 216 * 2, "tabela sekcji za paleta")
        self.assertLessEqual(tab_off + nsizes * 28, len(self.raw))

    def test_sections_consistent(self):
        secs = self.sections()
        self.assertEqual([s["px"] for s in secs], [16, 32, 48, 64])
        counts = {s["count"] for s in secs}
        self.assertEqual(len(counts), 1, "wszystkie sekcje maja tyle samo ikon")
        for s in secs:
            px = s["px"]
            self.assertEqual(s["mask_bytes"], (px * px + 7) // 8, "maska px*px/8")
            self.assertEqual(s["pixel_bytes"], px * px, "piksele px*px")
            # uklad v1: naglowek(20) + wpisy(8*count) + paleta(216*2) + dane
            expect = 20 + 8 * s["count"] + 216 * 2 + s["count"] * (s["mask_bytes"] + s["pixel_bytes"])
            self.assertEqual(s["raw_size"], expect, "raw_size sekcji %d" % px)
            self.assertGreater(s["blob_size"], 0)
            self.assertLessEqual(s["blob_off"] + s["blob_size"], len(self.raw),
                                 "strumien sekcji %d w granicach pliku" % px)

    def test_sections_decode_to_v1(self):
        for s in self.sections():
            comp = self.raw[s["blob_off"]: s["blob_off"] + s["blob_size"]]
            blob = lzss_decode(comp, s["raw_size"])
            self.assertIsNotNone(blob, "sekcja %d sie nie rozpakowuje" % s["px"])
            magic, ver, count, pal_off, ent_off = struct.unpack(">IIIII", blob[:20])
            self.assertEqual(magic, 0x49434F4E)
            self.assertEqual(ver, 1, "wewnatrz sekcji uklad v1")
            self.assertEqual(count, s["count"])
            self.assertEqual(pal_off, 20 + 8 * count)
            self.assertEqual(ent_off, 20)
            last_end = 0
            for i in range(count):
                mb, pb = struct.unpack(">II", blob[ent_off + i * 8: ent_off + i * 8 + 8])
                self.assertEqual(mb + s["mask_bytes"] <= len(blob), True)
                self.assertEqual(pb + s["pixel_bytes"] <= len(blob), True)
                last_end = max(last_end, mb + s["mask_bytes"], pb + s["pixel_bytes"])
            self.assertEqual(last_end, len(blob), "dane wypelniaja blob dokladnie")

    def test_sixtyfour_mask_matches_png(self):
        """Maska ikony #0 z sekcji 64 = progowa alfa PNG przeskalowanego do 64."""
        from PIL import Image
        secs = {s["px"]: s for s in self.sections()}
        s = secs[64]
        # kolejnosc ID: ta sama co pliki PNG w generatorze (baseline z icons_ids.h)
        ids = (ROOT / "system" / "icons_ids.h").read_text()
        enum = ids[ids.index("enum {"): ids.index("};", ids.index("enum {"))]
        names = [t.strip().rstrip(",")[5:].lower()
                 for t in enum.split("\n") if t.strip().startswith("ICON_")]
        self.assertEqual(len(names), s["count"])
        first = names[0]
        cand = [p for p in (ROOT / "system" / "ICONS").glob("*.png")
                if p.name.lower() == first + ".png"]
        if not cand:
            self.skipTest("brak PNG dla pierwszej ikony (%s)" % first)
        blob = lzss_decode(self.raw[s["blob_off"]: s["blob_off"] + s["blob_size"]], s["raw_size"])
        mb, pb = struct.unpack(">II", blob[20:28])
        mask = blob[mb: mb + s["mask_bytes"]]
        im = Image.open(cand[0]).convert("RGBA").resize((64, 64), Image.LANCZOS)
        px = im.load()
        bad = 0
        for y in range(64):
            for x in range(64):
                a = px[x, y][3]
                on = bool(mask[(y * 64 + x) >> 3] & (0x80 >> (x & 7)))
                if on != (a >= ALPHA_MIN):
                    bad += 1
        self.assertEqual(bad, 0, "maska 64 px rozni sie od alfy PNG w %d px" % bad)


if __name__ == "__main__":
    unittest.main()
