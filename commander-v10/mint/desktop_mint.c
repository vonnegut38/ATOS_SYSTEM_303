/* ATOS DESKTOP.PRG - gemlib: bar + clock + desktop icons (icons.h). */
#include <gem.h>
#include "icons.h"

static int tgettime(void)
{
    int t;
    __asm__ __volatile__("move.w #0x2c,%%d0\n\ttrap #1\n\tmove.w %%d0,%0"
                         : "=d"(t) : : "memory");
    return t & 0xFFFF;
}

static void blit(short h, const unsigned long ic[32], int x, int y)
{
    short pxy[4];
    int r, k;
    vsf_interior(h, 1);
    vsf_color(h, 1);                 /* dark pixels on light background */
    for (r = 0; r < 32; r++) {
        unsigned long row = ic[r];
        for (k = 0; k < 32; k++) {
            if (row & (0x80000000UL >> k)) {
                pxy[0] = (short)(x + k); pxy[1] = (short)(y + r);
                pxy[2] = (short)(x + k); pxy[3] = (short)(y + r);
                v_bar(h, pxy);
            }
        }
    }
}

static void draw_clock(short h, int t)
{
    char clk[9];
    short pxy[4];
    unsigned s2 = (unsigned)(t & 0x1F);
    unsigned m  = (unsigned)((t >> 5) & 0x3F);
    unsigned hh = (unsigned)((t >> 11) & 0x1F);
    unsigned s  = s2 * 2; if (s > 59) s = 59;

    clk[0] = (char)('0' + hh / 10); clk[1] = (char)('0' + hh % 10); clk[2] = ':';
    clk[3] = (char)('0' + m / 10);  clk[4] = (char)('0' + m % 10);  clk[5] = ':';
    clk[6] = (char)('0' + s / 10);  clk[7] = (char)('0' + s % 10);  clk[8] = 0;

    vsf_interior(h, 1); vsf_color(h, 1);
    pxy[0] = 520; pxy[1] = 1; pxy[2] = 637; pxy[3] = 14; v_bar(h, pxy);
    vst_color(h, 0);
    v_gtext(h, 540, 12, clk);
}

static void icon(short h, const unsigned long ic[32], int x, int y, const char *label)
{
    int len = 0; const char *p = label;
    blit(h, ic, x, y);
    while (p[len]) len++;
    vst_color(h, 1);
    v_gtext(h, x + 16 - len * 4, y + 42, label);
}

int main(void)
{
    short handle = 1;
    short work_in[11], work_out[57], pxy[4];
    int i, t, last = -1;

    appl_init();
    for (i = 0; i < 10; i++) work_in[i] = 1;
    work_in[10] = 2;
    v_opnvwk(work_in, &handle, work_out);
    if (handle <= 0) handle = 1;

    vsf_interior(handle, 1); vsf_color(handle, 0);       /* light desktop (0=white in mono) */
    pxy[0] = 0; pxy[1] = 0; pxy[2] = 639; pxy[3] = 399; v_bar(handle, pxy);

    vsf_color(handle, 1);                                /* dark top bar */
    pxy[0] = 0; pxy[1] = 0; pxy[2] = 639; pxy[3] = 15; v_bar(handle, pxy);

    icon(handle, ic_DYSK,  48, 60, "DYSK");
    icon(handle, ic_KOSZ,  48,150, "KOSZ");
    icon(handle, ic_FOLDER,140, 60, "FOLDER");
    icon(handle, ic_TERM, 140,150, "TERM");
    icon(handle, ic_GEAR, 232, 60, "USTAWIENIA");

    t = tgettime(); draw_clock(handle, t); last = t;
    for (;;) { t = tgettime(); if (t != last) { last = t; draw_clock(handle, t); } }
    return 0;
}
