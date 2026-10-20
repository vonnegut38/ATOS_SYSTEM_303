"""Dekodery ikon z jadra - test hostowy (wycina KOD JADRA ze zrodla).

Po co: format ICONS.DAT v5 trzyma kazdy rozmiar jako osobny strumien LZSS, a
te same dekodery (RLE v2, PackBits v3, LZSS v4/v5) siedza w
system/kernel_graphics.c.  Test bierze TE funkcje z jadra i sprawdza je na:
  * trzech sekcjach PRAWDZIWEGO build/system/ICONS.DAT (32/48/64),
  * wektorach zrobionych packerami projektu (tools/icons_rle.py).
Porownanie przez FNV-1a 32 bity liczone niezaleznie w Pythonie i w C - jesli
dekoder zgubi albo przesunie choc jeden bajt, sumy sie rozjada.

UWAGA: loadera kgfx_icons_load() NIE da sie przetestowac na hoscie - czyta
pola bloba WPROST jako uint32 (big-endian = natywne dla 68000), wiec na
malo-endianowym hoscie naglowek jest obrocony.  Ten test pokrywa wiec
DEKODERY (przenosne), a cala sciezke loadera sprawdza emulator (zrzuty w
display-demo/) i tests/test_icons_dat_v5.py (format pliku).
"""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
GFX = ROOT / "system" / "kernel_graphics.c"
DAT = ROOT / "build" / "system" / "ICONS.DAT"


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def lzss_decode(comp, raw):
    out = bytearray()
    j = 0
    while j < len(comp) and len(out) < raw:
        flags = comp[j]
        j += 1
        for bit in range(8):
            if j >= len(comp) or len(out) >= raw:
                break
            if flags & (1 << bit):
                lo, hi = comp[j], comp[j + 1]
                j += 2
                off = (lo << 4) | (hi >> 4)
                n = (hi & 15) + 3
                st = len(out) - off
                for k in range(n):
                    if len(out) >= raw:
                        break
                    out.append(out[st + k])
            else:
                out.append(comp[j])
                j += 1
    return bytes(out)


def packers():
    src = (ROOT / "tools" / "icons_rle.py").read_text()
    ns = {}
    exec(src[:src.index("def main():")], ns)
    return ns


def c_array(name, data):
    body = ",".join("0x%02x" % b for b in data)
    return "static const uint8_t %s[] = {%s};\n" % (name, body if body else "0x00")


class IconDecoders(unittest.TestCase):
    def test_decoders_match_python(self):
        if not DAT.exists():
            self.skipTest("brak build/system/ICONS.DAT")

        ns = packers()
        raw_file = DAT.read_bytes()
        _, _, nsizes, _, tab = struct.unpack(">IIIII", raw_file[:20])
        vectors = []          # (kind, comp, raw_size, expected_fnv, label)
        for i in range(nsizes):
            px, cnt, mb, pb, raw, off, ln = struct.unpack(
                ">IIIIIII", raw_file[tab + i * 28: tab + i * 28 + 28])
            comp = raw_file[off:off + ln]
            out = lzss_decode(comp, raw)
            self.assertEqual(len(out), raw, "sekcja %d px" % px)
            vectors.append((4, comp, raw, fnv1a(out), "sekcja %d px" % px))

        # wektory z packerow projektu: RLE (v2), PackBits (v3), LZSS (v4)
        rng = (bytes((i * 37 + (i >> 3) * 11) & 0xFF for i in range(4096))
               + b"\x00" * 600 + bytes(range(256)) * 4)
        for kind, enc, name in ((2, ns["rle"](rng), "RLE"),
                                (3, ns["packbits"](rng), "PackBits"),
                                (4, ns["lzss"](rng), "LZSS")):
            vectors.append((kind, enc, len(rng), fnv1a(rng), "wektor " + name))

        src = GFX.read_text()
        a = src.index("static int icons_rle(")
        b = src.index("void kgfx_icons_unload(void)")
        decoders = src[a:b]

        code = "#include <stdio.h>\n#include <stdint.h>\n#include <string.h>\n\n" + decoders + "\n"
        for idx, (_kind, comp, _raw, _expect, _label) in enumerate(vectors):
            code += c_array("comp_%d" % idx, comp)
        code += """
static uint32_t fnv1a(const uint8_t *p, uint32_t n)
{
    uint32_t h = 0x811C9DC5u, i;
    for (i = 0; i < n; ++i) { h ^= (uint32_t)p[i]; h *= 0x01000193u; }
    return h;
}
static uint8_t buf[1u << 21];
int main(void)
{
    int failures = 0;
    uint32_t got;
    int ok;
"""
        for idx, (kind, _comp, raw, expect, label) in enumerate(vectors):
            fn = "icons_rle" if kind == 2 else ("icons_packbits" if kind == 3 else "icons_lzss")
            extra = "" if kind in (2, 3) else ",0u"
            exp = "0x%08xu" % expect
            code += "\n    memset(buf, 0xAA, sizeof buf);\n"
            code += ("    ok = %s(comp_%d, (uint32_t)sizeof(comp_%d), buf, %du%s);\n"
                     % (fn, idx, idx, raw, extra))
            code += "    got = fnv1a(buf, %du);\n" % raw
            code += ('    if (!ok) { printf("%s: dekoder zwrocil BLAD\\n", "' + label + '"); ++failures; }\n')
            code += ('    else if (got != ' + exp + ') { printf("%s: suma 0x%08x != ' + exp[2:] + '\\n", "' + label + '", got); ++failures; }\n')
        code += """
    printf(failures ? "FAILURES=%d\\n" : "dekodery ikon: OK (sekcje ICONS.DAT + RLE/PackBits/LZSS)\\n", failures);
    return failures ? 1 : 0;
}
"""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            (p / "t.c").write_text(code)
            exe = p / "t"
            r = subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                                "-I" + str(ROOT / "include"), "-I" + str(ROOT / "system"),
                                str(p / "t.c"), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr[-3000:] + "\n--- kod ---\n" + code[-3000:])
            out = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout + out.stderr)


if __name__ == "__main__":
    unittest.main()
