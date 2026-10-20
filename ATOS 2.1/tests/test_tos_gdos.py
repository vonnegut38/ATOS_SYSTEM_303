#!/usr/bin/env python3
"""GDOS layer: ASSIGN.SYS and the bitmap .FNT header, on the real files.

Extracts the pure parsers from system/tos_gdos.c and runs them against the
ASSIGN.SYS shipped on the LDW Power disk (tests/data/ldw-assign.sys) plus
synthetic .FNT headers built from the layout observed on that disk:
  byte 0 = id (0 bitmap / 1 outline), byte 2 = point, bytes 4..35 = name.
"""
import pathlib, subprocess, tempfile, sys

root = pathlib.Path(__file__).resolve().parents[1]
if len(sys.argv) > 1:
    _arg = pathlib.Path(sys.argv[1])
    if _arg.is_dir() and (_arg / 'system/tos_gdos.c').exists():
        root = _arg
src = (root / 'system/tos_gdos.c').read_text()
a = src.index('static int ci_eq(')
b = src.index('/* ---- boot-time loading')
parsers = src[a:b]

assign_text = (root / 'tests/data/ldw-assign.sys').read_text()
# C string literal: escape backslashes, quotes and newlines (raw strings are C++)
_lines = assign_text.replace('\\', '\\\\').replace('"', '\\"').split('\n')
c_assign = '\n'.join('    "%s\\n"' % ln for ln in _lines if ln is not None)

code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint16_t WORD;
typedef uint8_t  UBYTE;
#define TOS_GDOS_MAX_FONTS 64
#define TOS_GDOS_NAME_LEN  32
''' + '''
typedef struct {
    char     name[TOS_GDOS_NAME_LEN + 1];
    char     file[16];
    uint16_t point;
    uint16_t dev;
    uint16_t font_id;
    uint16_t first_ade,last_ade;
    uint16_t top,ascent,descent,bottom;
    uint16_t max_char_width,max_cell_width;
    uint16_t form_width,form_height;
    uint16_t flags;
    uint8_t  big_endian;
    uint8_t  bitmap;
    uint8_t  loaded;
} tos_gdos_font_t;
typedef struct {
    tos_gdos_font_t font[TOS_GDOS_MAX_FONTS];
    uint16_t count;
    uint16_t screen_dev;
    uint16_t printer_dev;
    char     path[64];
    uint8_t  parsed;
    uint8_t  state;
    uint16_t loaded_count;
} tos_gdos_t;
''' + parsers + r'''

static const char assign[] =
''' + c_assign + r''';

static uint16_t tos_gdos_device_count(const tos_gdos_t *g, uint16_t dev)
{
    unsigned i; uint16_t n = 0;
    for (i = 0; i < g->count; ++i) if (g->font[i].dev == dev) ++n;
    return n;
}

int main(void)
{
    tos_gdos_t g;

    /* --- the real ASSIGN.SYS from the LDW Power disk ---------------------- */
    memset(&g, 0, sizeof g);
    assert(tos_gdos_assign_parse(assign, (unsigned)strlen(assign), &g) == 8);
    assert(strcmp(g.path, "a:\\gdos.fnt") == 0);
    assert(g.screen_dev == 1);            /* 01p screen.sys */
    assert(g.printer_dev == 21);          /* 21 fx80.sys */
    assert(g.count == 8);                 /* 4 under the printer, 4 under MF */
    assert(strcmp(g.font[0].file, "attr10ep.fnt") == 0);
    assert(g.font[0].dev == 21);
    assert(strcmp(g.font[4].file, "attr10mf.fnt") == 0);
    assert(g.font[4].dev == 31);
    assert(g.parsed == 1);
    /* Device split: the SCREEN has no extra fonts (1..4), four sizes belong to
       the printer (21) and four to the metafile device (31).  The old code
       mixed them into one global list and reported eight screen fonts. */
    assert(tos_gdos_device_count(&g, 1) == 0);
    assert(tos_gdos_device_count(&g, 21) == 4);
    assert(tos_gdos_device_count(&g, 31) == 4);

    /* comments, CRLF and blank lines must not create fonts */
    {const char *t = "; comment\r\n\r\npath = c:\\\r\n01p screen.sys ; x\r\nNOTAFONT.TXT\r\n";
     tos_gdos_t h; memset(&h, 0, sizeof h);
     assert(tos_gdos_assign_parse(t, (unsigned)strlen(t), &h) == 0);
     assert(strcmp(h.path, "c:\\") == 0);
     assert(h.screen_dev == 1);}

    /* --- Fonthead (EmuTOS fonthdr.h), both byte orders -------------------- */
    {uint8_t fnt[96]; tos_gdos_font_t fh;
     /* LITTLE-endian header, as on the LDW Power disk: 0e 00 = ID 14,
        18 00 = 24 pt.  Word order must be DECIDED, not assumed. */
     memset(fnt, 0, sizeof fnt);
     fnt[0] = 0x0e; fnt[1] = 0x00;
     fnt[2] = 24;   fnt[3] = 0x00;
     memcpy(fnt + 4, "Dutch", 5);
     fnt[36] = 0x20; fnt[37] = 0x00;             /* first_ade = 32, little endian */
     fnt[38] = 0x7e; fnt[39] = 0x00;             /* last_ade  = 126 */
     fnt[48] = 0x10; fnt[49] = 0x00;             /* bottom = 16 */
     fnt[80] = 0x10; fnt[81] = 0x00;             /* form_width  = 16 */
     fnt[82] = 0x18; fnt[83] = 0x00;             /* form_height = 24 */
     memset(&fh, 0, sizeof fh);
     assert(tos_gdos_fnt_header(fnt, sizeof fnt, &fh) == 1);
     assert(fh.point == 24);
     assert(fh.font_id == 14);
     assert(strcmp(fh.name, "Dutch") == 0);
     assert(fh.big_endian == 0);                 /* little endian */
     assert(fh.bitmap == 1);
     assert(fh.form_height == 24 && fh.first_ade == 32 && fh.last_ade == 126);
     /* BIG-endian (Motorola) header must validate too: 00 18 = 24 pt */
     memset(fnt, 0, sizeof fnt);
     fnt[0] = 0x00; fnt[1] = 0x01;               /* font_id = 1 (a bitmap!) */
     fnt[2] = 0x00; fnt[3] = 24;
     memcpy(fnt + 4, "Motorola", 8);
     fnt[36] = 0x00; fnt[37] = 0x20;
     fnt[38] = 0x00; fnt[39] = 0x7e;
     fnt[48] = 0x00; fnt[49] = 0x10;
     fnt[66] = 0x00; fnt[67] = 0x04;             /* F_STDFORM */
     fnt[80] = 0x00; fnt[81] = 0x10;
     fnt[82] = 0x00; fnt[83] = 0x18;
     memset(&fh, 0, sizeof fh);
     assert(tos_gdos_fnt_header(fnt, sizeof fnt, &fh) == 1);
     assert(fh.point == 24);
     assert(fh.font_id == 1);
     assert(fh.big_endian == 1);
     /* a non-ASCII name is cut, not copied through */
     memcpy(fnt + 4, "AB\x01" "CD", 5);
     memset(&fh, 0, sizeof fh);
     assert(tos_gdos_fnt_header(fnt, sizeof fnt, &fh) == 1);
     assert(strcmp(fh.name, "AB") == 0);
     /* too short, and an implausible point size, must both be rejected */
     assert(tos_gdos_fnt_header(fnt, 40, &fh) == 0);
     memset(fnt, 0, sizeof fnt);
     fnt[2] = 0xff; fnt[3] = 0xff;
     assert(tos_gdos_fnt_header(fnt, sizeof fnt, &fh) == 0);}

    puts("GDOS parsers: ASSIGN.SYS (8 fonts, path, devices) and .FNT header: PASS");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='atos-gdos-test-') as d:
    p = pathlib.Path(d)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
