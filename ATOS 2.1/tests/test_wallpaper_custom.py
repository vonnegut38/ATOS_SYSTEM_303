"""Tapeta CUSTOM (wzor 4) - cala sciezka: parser WALL= -> jadro -> rysowanie.

Trzy miejsca, w ktorych CUSTOM ginal, kazde sprawdzone osobnym przypadkiem:

1. desktop_wm.c parse_wall(): stary kod bral JEDNA cyfre i obcinal wszystko
   powyzej 3 do zera, wiec zapisane przez edytor kafelka "WALL=4" wczytywalo
   sie jako SOLID (0) i tapeta po restarcie byla jednolita, mimo poprawnego
   kafelka w jadrze.
2. kernel_theme.c kgfx_theme_set_wall(): `n>3u?0u:n` zamienialo 4 na 0.
3. kgfx_pattern_fill(): dla wzoru 4 musi czytac WLASNY KAFELEK (kgfx_tile_get),
   a nie malowac samego tla.

Test wycina te funkcje ze zrodel jadra/pulpitu i kompiluje je na hoście ze
stubami - dokladnie ten sam kod, ktory idzie do KERNEL.SYS i DESKTOP.APP.
"""
from pathlib import Path
import subprocess, tempfile, unittest

ROOT = Path(__file__).resolve().parents[1]

STUBS = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* --- stuby powierzchni i clipu (to, co kgfx_pattern_fill wola z jadra) --- */
#define FB_W 64
#define FB_H 64
static uint8_t fb[FB_W * FB_H];
static int dirty_calls, last_x, last_y, last_w, last_h;

typedef struct { uint8_t *words; unsigned line_words; int chunky; } surface_t;

static int clip_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    (void)x; (void)y;
    if (*w <= 0 || *h <= 0) return 0;
    return 1;
}
static surface_t active_surface(void)
{
    surface_t s; s.words = fb; s.line_words = FB_W / 2u; s.chunky = 1; return s;
}
static void mark_dirty(int32_t x, int32_t y, int32_t w, int32_t h)
{
    ++dirty_calls; last_x = (int)x; last_y = (int)y; last_w = (int)w; last_h = (int)h;
}
static int kgfx_set_palette(uint8_t index, uint16_t rgb)
{
    (void)index; (void)rgb; return 1;
}
static void kgfx_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color)
{
    int32_t i, j;
    for (j = 0; j < h; ++j) for (i = 0; i < w; ++i)
        fb[(y + j) * FB_W + (x + i)] = color;
}
'''

# kernel_theme.c setter wola te dwa - licza, czy pulpit dostal sygnal
STUBS_THEME = r'''
#include <assert.h>
#include <stdint.h>
static int theme_damage_all, theme_repaint;
void kernel_wm_damage_all(void) { ++theme_damage_all; }
void kernel_wm_desktop_repaint(void) { ++theme_repaint; }
'''

def slice_between(text, start, end):
    a = text.index(start)
    b = text.index(end, a)
    return text[a:b]

def build(code, tmp):
    src = tmp / 'test.c'
    src.write_text(code)
    exe = tmp / 'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(ROOT / 'include'),
                    str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


class WallpaperCustom(unittest.TestCase):
    def test_pattern_four_draws_custom_tile(self):
        gfx = (ROOT / 'system/kernel_graphics.c').read_text()
        body = slice_between(gfx, 'static uint8_t custom_tile[32];', 'void kgfx_fill_rect')
        code = STUBS + body + r'''
static uint8_t px(int x, int y) { return fb[y * FB_W + x]; }
/* SZYBKA sciezka (x=0, szerokosc %16==0) zapisuje wiersz osmiona slowami
   16-bitowymi w kolejnosci BIG-endian (t[0] w gornym bajcie).  Na m68k bajt
   t[0] trafia na offset 0; na malo-endianowym hoscie para jest zamieniona -
   to artefakt testu, nie kod, wiec sprawdzamy dokladnie pare. */
static void pair(int x, int y, uint8_t a, uint8_t b)
{
    uint8_t lo = px(x, y), hi = px(x + 1, y);
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    assert(lo == b && hi == a);
#else
    assert(lo == a && hi == b);
#endif
}
int main(void)
{
    /* kafelek: zapalone (0,0) i (15,15), reszta tla */
    kgfx_tile_clear();
    kgfx_tile_set(0, 0, 1);
    kgfx_tile_set(15, 15, 1);
    assert(kgfx_tile_get(0, 0) == 1);
    assert(kgfx_tile_get(1, 0) == 0);

    /* (A) SCIEZKA BAJTOWA (szerokosc nie jest wielokrotnoscia 16) - co do bajtu */
    memset(fb, 2, sizeof fb);
    dirty_calls = 0;
    kgfx_pattern_fill(0, 0, 31, 17, 4u);
    assert(px(0, 0) == 3);          /* [0] */
    assert(px(1, 0) == 2);
    assert(px(0, 1) == 2);          /* wiersz 1 pusty */
    assert(px(15, 15) == 3);
    assert(px(16, 0) == 3);         /* kafelek powtarza sie co 16 px */
    assert(dirty_calls == 1);       /* jedno uszkodzenie na cale tlo */
    assert(last_x == 0 && last_y == 0 && last_w == 31 && last_h == 17);

    /* (B) SZYBKA SCIEZKA (x=0, szerokosc %16==0) - pary 16-bitowe */
    memset(fb, 2, sizeof fb);
    kgfx_pattern_fill(0, 0, 32, 32, 4u);
    pair(0, 0, 3, 2);               /* zapalony piksel kafelka */
    pair(2, 0, 2, 2);
    pair(0, 1, 2, 2);
    pair(16, 0, 3, 2);              /* druga kopia kafelka */
    pair(30, 15, 2, 3);             /* wiersz 15: t[14]=tlo, t[15]=zapalony piksel */
    pair(30, 31, 2, 3);             /* ta sama para w drugim kafelku (y=31) */

    /* wzory proceduralne nadal dzialaja (1 = dither 50/50) */
    memset(fb, 0, sizeof fb);
    kgfx_pattern_fill(0, 0, 16, 16, 1u);
    {unsigned on = 0, x, y;
     for (y = 0; y < 16u; ++y) for (x = 0; x < 16u; ++x) on += (px((int)x, (int)y) == 3);
     assert(on == 128u);}
    /* 0 = jednolite tlo */
    memset(fb, 9, sizeof fb);
    kgfx_pattern_fill(0, 0, 16, 16, 0u);
    assert(px(3, 3) == 2);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            build(code, Path(d))

    def test_kernel_accepts_pattern_four(self):
        theme = (ROOT / 'system/kernel_theme.c').read_text()
        body = slice_between(theme, 'static unsigned wall_pattern;',
                             'static unsigned flags_of(unsigned i)')
        code = STUBS_THEME + body + r'''
int main(void)
{
    assert(kgfx_theme_wall() == 0u);          /* domyslnie SOLID */
    assert(kgfx_theme_set_wall(4u) == 0);
    assert(kgfx_theme_wall() == 4u);          /* CUSTOM przechodzi do jadra */
    assert(theme_damage_all == 1 && theme_repaint == 1);
    assert(kgfx_theme_set_wall(3u) == 0 && kgfx_theme_wall() == 3u);
    assert(kgfx_theme_set_wall(5u) == 0 && kgfx_theme_wall() == 0u);  /* smiec -> SOLID */
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            build(code, Path(d))

    def test_desktop_config_parser_keeps_four(self):
        wm = (ROOT / 'system/desktop_wm.c').read_text()
        body = slice_between(wm, '#define WALL_PATTERN_MAX 4u',
                             '/* Restore the config saved by desktop_save()')
        code = r'''
#include <assert.h>
#include <stdint.h>
''' + body + r'''
int main(void)
{
    assert(parse_wall("0") == 0u);
    assert(parse_wall("1") == 1u);
    assert(parse_wall("3") == 3u);
    assert(parse_wall("4") == 4u);          /* WALL=4 -> CUSTOM (bylo 0!) */
    assert(parse_wall("4\r") == 4u);        /* plik z CRLF */
    assert(parse_wall("5") == 0u);          /* poza zakresem -> SOLID */
    assert(parse_wall("40") == 0u);
    assert(parse_wall("") == 0u);
    assert(parse_wall("A") == 0u);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            build(code, Path(d))


if __name__ == '__main__':
    unittest.main()
