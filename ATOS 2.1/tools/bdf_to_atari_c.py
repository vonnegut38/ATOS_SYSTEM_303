#!/usr/bin/env python3
"""Convert printable ASCII from an 8x16 BDF to BIOS 8x16 and 8x8 tables."""

import pathlib
import sys


def read_bdf(path):
    glyphs = {}
    encoding = None
    bitmap = None
    rows = []
    for raw in pathlib.Path(path).read_text(encoding="ascii").splitlines():
        if raw.startswith("ENCODING "):
            encoding = int(raw.split()[1])
        elif raw == "BITMAP":
            bitmap = True
            rows = []
        elif raw == "ENDCHAR":
            if encoding is not None and bitmap:
                glyphs[encoding] = rows
            encoding = None
            bitmap = None
        elif bitmap:
            rows.append(int(raw, 16))
    return glyphs


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: bdf_to_atari_c.py INPUT.bdf OUTPUT.c")
    glyphs = read_bdf(sys.argv[1])
    missing = [code for code in range(32, 127) if code not in glyphs]
    if missing:
        raise SystemExit(f"missing ASCII glyphs: {missing}")

    out = [
        '#include "font.h"',
        '',
        '/* Atari ST 8x16 system-style font from ntwk/atarist-font (BSD-3-Clause).',
        ' * The 8x8 color-monitor table is derived by combining adjacent scanlines. */',
        'static const uint8_t font_8x16[95][16] = {'
    ]
    for code in range(32, 127):
        rows = (glyphs[code] + [0] * 16)[:16]
        out.append('    {' + ','.join(f'0x{x:02x}' for x in rows) + f'}}, /* {code} */')
    out += ['};', '', 'static const uint8_t font_8x8[95][8] = {']
    for code in range(32, 127):
        rows = (glyphs[code] + [0] * 16)[:16]
        compact = [rows[i] | rows[i + 1] for i in range(0, 16, 2)]
        out.append('    {' + ','.join(f'0x{x:02x}' for x in compact) + f'}}, /* {code} */')
    out += [
        '};', '',
        'const uint8_t *font_glyph(char c, uint8_t tall)',
        '{',
        '    unsigned int code=(unsigned char)c;',
        "    if(code<32u || code>126u) code=(unsigned int)'?';",
        '    return tall ? font_8x16[code-32u] : font_8x8[code-32u];',
        '}', ''
    ]
    pathlib.Path(sys.argv[2]).write_text('\n'.join(out), encoding='ascii')


if __name__ == '__main__':
    main()
