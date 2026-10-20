#!/usr/bin/env python3
"""Raster paths of the TOS VDI: span lines and glyph-clipped text.

Extracts the real vdi_line/vdi_glyph/tos_vdi_text_clip code from
system/tos_vdi.c and runs it against stubs, so the numbers are the audit's:
a 200x120 rectangle outline used to cost 644 vdi_fill calls (one per pixel),
and a solid 8x16 glyph with a 1x1 clip used to ask for 128 pixels in 16 spans
that all left the clip.
"""
import pathlib, subprocess, tempfile, sys

root = pathlib.Path(__file__).resolve().parents[1]
if len(sys.argv) > 1:
    _arg = pathlib.Path(sys.argv[1])
    if _arg.is_dir() and (_arg / 'system/tos_vdi.c').exists():
        root = _arg
src = (root / 'system/tos_vdi.c').read_text()

clip = src[src.index('/* Optional pixel clip for glyph spans.'):src.index('static void vdi_glyph(')]
glyph = src[src.index('static void vdi_glyph('):src.index('static int vdi_line_style_on(')]
style = src[src.index('static int vdi_line_style_on('):src.index('static void vdi_line(')]
line = src[src.index('static void vdi_line('):src.index('/* Scanline fill of a polygon')]

part1 = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fill_calls, fill_pixels, outside_pixels;
static int last_x0, last_y0, last_x1, last_y1;

typedef struct { int line_width, line_type, wrt_mode; } ws_t;
static ws_t ws = { 1, 1, 1 };
static ws_t *vdi_ws(void) { return &ws; }

/* A real 640x400 mono surface for the word-mask glyph path. */
static uint8_t surface[640 * 400 / 8];
typedef struct { uint8_t *data; uint16_t pitch, w, h, bpp; } wm_fb_t;
static wm_fb_t vdi_fb(void)
{
    wm_fb_t d; d.data = surface; d.pitch = 80; d.w = 640; d.h = 400; d.bpp = 1; return d;
}
static int vdi_clip_on; static int16_t vdi_clip[4];
static void dummy_hook(void) { }
static void (*vdi_row_dirty_hook)(void) = 0;
static int dirty_calls;
static void tos_note_dirty_rect(int x0, int y0, int x1, int y1)
{ (void)x0; (void)y0; (void)x1; (void)y1; ++dirty_calls; }
static uint32_t tos_vdi_glyph_fast;
static int mono_calls, mono_height;
static void mono_glyph_h(volatile uint16_t *w, uint16_t stride, uint16_t x, uint16_t y,
                         const uint8_t g[16], uint16_t height, uint8_t fg, uint8_t bg, int opaque)
{ (void)w; (void)stride; (void)x; (void)y; (void)g; (void)fg; (void)bg; (void)opaque;
  ++mono_calls; mono_height = (int)height; }
static void mono_glyph(volatile uint16_t *w, uint16_t stride, uint16_t x, uint16_t y,
                       const uint8_t g[16], uint8_t fg, uint8_t bg, int opaque)
{ mono_glyph_h(w, stride, x, y, g, 16u, fg, bg, opaque); }

/* Solid 8x16 glyph: every pixel of the cell is set, so any span that leaves
   the clip shows up in the counters. */
static const uint8_t solid[16] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
static const uint8_t *font_glyph(char ch, uint8_t tall) { (void)ch; (void)tall; return solid; }
'''

part2 = r'''
static void vdi_fill(int x0, int y0, int x1, int y1, int color)
{
    (void)color;
    ++fill_calls;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    last_x0 = x0; last_y0 = y0; last_x1 = x1; last_y1 = y1;
    fill_pixels += (x1 - x0 + 1) * (y1 - y0 + 1);
    if (vdi_text_clip_on) {
        int ix0 = x0 < vdi_text_clip[0] ? vdi_text_clip[0] : x0;
        int iy0 = y0 < vdi_text_clip[1] ? vdi_text_clip[1] : y0;
        int ix1 = x1 > vdi_text_clip[2] ? vdi_text_clip[2] : x1;
        int iy1 = y1 > vdi_text_clip[3] ? vdi_text_clip[3] : y1;
        int inside = (ix1 >= ix0 && iy1 >= iy0) ? (ix1 - ix0 + 1) * (iy1 - iy0 + 1) : 0;
        outside_pixels += (x1 - x0 + 1) * (y1 - y0 + 1) - inside;
    } else {
        outside_pixels += (x1 - x0 + 1) * (y1 - y0 + 1);
    }
}
'''

part3 = r'''
static void rect_outline(int x, int y, int w, int h)
{
    vdi_line(x, y, x + w - 1, y, 1);
    vdi_line(x + w - 1, y, x + w - 1, y + h - 1, 1);
    vdi_line(x + w - 1, y + h - 1, x, y + h - 1, 1);
    vdi_line(x, y + h - 1, x, y, 1);
}

int main(void)
{
    (void)mono_glyph;   /* kept for compatibility; the VDI calls mono_glyph_h */
    /* --- horizontal and vertical lines are spans, not pixels -------------- */
    fill_calls = 0; fill_pixels = 0; outside_pixels = 0;
    vdi_line(10, 20, 209, 20, 1);                 /* 200 px horizontal */
    assert(fill_calls == 1 && fill_pixels == 200);
    fill_calls = 0; fill_pixels = 0;
    vdi_line(10, 20, 10, 139, 1);                 /* 120 px vertical */
    assert(fill_calls == 1 && fill_pixels == 120);
    fill_calls = 0; fill_pixels = 0;
    vdi_line(209, 20, 10, 20, 1);                 /* backwards: still one span */
    assert(fill_calls == 1 && fill_pixels == 200 && last_x0 == 10 && last_x1 == 209);

    /* the audit's rectangle: dx=200, dy=120 -> four spans instead of 644 fills */
    fill_calls = 0; fill_pixels = 0;
    rect_outline(0, 0, 201, 121);
    assert(fill_calls == 4);
    /* The audit measured 644 vdi_fill calls (one per pixel).  The four spans
       carry the same 644 pixels, so the picture is unchanged and the call count
       drops 161x.  Corner pixels are covered by two spans, which is why the
       total is 2*201 + 2*121 and not the union area. */
    assert(fill_pixels == 644);

    /* wide lines: one span per width step */
    fill_calls = 0; fill_pixels = 0;
    ws.line_width = 3;
    vdi_line(0, 0, 199, 0, 1);
    assert(fill_calls == 3 && fill_pixels == 600);
    ws.line_width = 1;

    /* dashed styles keep the per-step path (a span cannot express them) */
    fill_calls = 0;
    ws.line_type = 3;                             /* dot */
    vdi_line(0, 0, 99, 0, 1);
    assert(fill_calls > 1);
    ws.line_type = 1;

    /* non-replace write modes keep the per-step path (overlay semantics) */
    fill_calls = 0;
    ws.wrt_mode = 3;                              /* XOR */
    vdi_line(0, 0, 99, 0, 1);
    assert(fill_calls > 1);
    ws.wrt_mode = 1;

    /* diagonals are not spans */
    fill_calls = 0;
    vdi_line(0, 0, 50, 50, 1);
    assert(fill_calls == 51);

    /* --- glyph text obeys the pixel clip ---------------------------------- */
    vdi_row_dirty_hook = dummy_hook;              /* the kernel installs one too */
    fill_calls = 0; fill_pixels = 0; outside_pixels = 0;
    tos_vdi_text_clip(0, 0, 0, 0);                /* 1x1 cell */
    vdi_glyph(0, 16, 16, 'X', 1, 1);              /* solid 8x16 glyph */
    assert(fill_pixels == 1);                     /* was 128 in the audit */
    assert(outside_pixels == 0);                  /* every span inside */
    tos_vdi_text_clip_off();

    /* A complete, unclipped 8x16 cell takes the word-mask path: one mono_glyph
       call and one damage rectangle, no vdi_fill at all. */
    fill_calls = 0; fill_pixels = 0; mono_calls = 0; dirty_calls = 0;
    vdi_glyph(0, 16, 16, 'X', 1, 1);
    assert(mono_calls == 1 && fill_calls == 0 && dirty_calls == 1);
    assert(tos_vdi_glyph_fast == 1);
    /* The cell must be fully inside the surface, and mono_glyph never clips the
       buffer itself - one pixel past the right edge stays on the span path. */
    fill_calls = 0; mono_calls = 0;
    vdi_glyph(636, 16, 16, 'X', 1, 1);            /* 636+8 > 640 */
    assert(mono_calls == 0 && fill_calls > 0);
    /* Non-replace write modes keep the span path (overlay semantics). */
    fill_calls = 0; mono_calls = 0; ws.wrt_mode = 3;
    vdi_glyph(0, 16, 16, 'X', 1, 1);
    assert(mono_calls == 0 && fill_calls == 16);
    ws.wrt_mode = 1;
    /* The short font (8 rows, vst_height 8 - what SysInfo actually uses) takes
       the same word-mask path, with the height passed through. */
    fill_calls = 0; mono_calls = 0; mono_height = 0;
    vdi_glyph(0, 8, 8, 'X', 0, 1);
    assert(mono_calls == 1 && mono_height == 8 && fill_calls == 0);

    /* half-clipped glyph: four of the eight columns -> 4 pixels per row */
    fill_calls = 0; fill_pixels = 0; outside_pixels = 0;
    tos_vdi_text_clip(0, 0, 3, 15);
    vdi_glyph(0, 16, 16, 'X', 1, 1);
    assert(fill_pixels == 64 && outside_pixels == 0);   /* 4 x 16 */
    tos_vdi_text_clip_off();

    /* a clip that does not touch the cell paints nothing at all (columns
       100..107 while the glyph occupies 0..7) */
    fill_calls = 0; fill_pixels = 0;
    tos_vdi_text_clip(100, 0, 107, 15);
    vdi_glyph(0, 16, 16, 'X', 1, 1);
    assert(fill_pixels == 0 && fill_calls == 0);
    tos_vdi_text_clip_off();

    puts("VDI raster: span lines (200x120 outline = 4 fills), glyph pixel clip "
         "(1x1 clip = 1 pixel, 0 outside) and word-mask glyphs: PASS");
    return 0;
}
'''

code = part1 + clip + part2 + glyph + style + line + part3

with tempfile.TemporaryDirectory(prefix='atos-vdi-test-') as d:
    p = pathlib.Path(d)
    (p / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
