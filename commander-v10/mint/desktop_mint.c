/* ATOS DESKTOP.PRG - gemlib: white bar + black clock cell with white text. */
#include <gem.h>

static int tgettime(void)
{
    int t;
    __asm__ __volatile__("move.w #0x2c,%%d0\n\ttrap #1\n\tmove.w %%d0,%0"
                         : "=d"(t) : : "memory");
    return t & 0xFFFF;
}

static void draw_clock(short handle, int t)
{
    char clk[9];
    short pxy[4];
    unsigned s2 = (unsigned)(t & 0x1F);
    unsigned m  = (unsigned)((t >> 5) & 0x3F);
    unsigned h  = (unsigned)((t >> 11) & 0x1F);
    unsigned s  = s2 * 2;
    if (s > 59) s = 59;

    clk[0] = (char)('0' + h / 10); clk[1] = (char)('0' + h % 10); clk[2] = ':';
    clk[3] = (char)('0' + m / 10); clk[4] = (char)('0' + m % 10); clk[5] = ':';
    clk[6] = (char)('0' + s / 10); clk[7] = (char)('0' + s % 10); clk[8] = 0;

    vsf_interior(handle, 1);
    vsf_color(handle, 0);                        /* BLACK cell */
    pxy[0] = 520; pxy[1] = 1; pxy[2] = 637; pxy[3] = 14;
    v_bar(handle, pxy);
    vst_color(handle, 1);                        /* WHITE text */
    v_gtext(handle, 540, 12, clk);
}

int main(void)
{
    short handle = 1;
    short work_in[11], work_out[57], pxy[4];
    int last = -1, i, t;

    appl_init();
    for (i = 0; i < 10; i++) work_in[i] = 1;
    work_in[10] = 2;
    v_opnvwk(work_in, &handle, work_out);
    if (handle <= 0) handle = 1;

    vsf_interior(handle, 1);
    vsf_color(handle, 1);                        /* WHITE bar */
    pxy[0] = 0; pxy[1] = 0; pxy[2] = 639; pxy[3] = 15;
    v_bar(handle, pxy);

    vsf_color(handle, 0);                        /* colour test: BLACK patch */
    pxy[0] = 300; pxy[1] = 4; pxy[2] = 340; pxy[3] = 12;
    v_bar(handle, pxy);

    t = tgettime();
    draw_clock(handle, t);
    last = t;

    for (;;) {
        t = tgettime();
        if (t != last) { last = t; draw_clock(handle, t); }
    }
    return 0;
}
