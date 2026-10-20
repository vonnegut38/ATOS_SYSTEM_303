/* TOS window colour client test (T2 colour milestone).
 *
 * A plain 0x601A TOS program that switches the screen to 320x200 with 16
 * colours (XBIOS Setscreen, rez 0, four bitplanes) and paints a pattern that
 * makes every failure mode visible:
 *
 *   - the 16 vertical bars are palette entries 0..15, in order, so a wrong
 *     palette or a swapped plane order shows immediately;
 *   - a white border and a black diagonal cross prove the plane expansion and
 *     the clipping.
 *
 * It writes into the STANDARD ST screen layout: bitplanes interleaved at 16-bit
 * words (one word per plane per 16-pixel group, big-endian, MSB = leftmost),
 * stride = (320/8)*4 = 160 bytes, 32000 bytes total.  That is the exact
 * contract kgfx_blit_planar() reads, so a mismatch here would have looked like
 * "the converter is broken" when the source data was simply in the wrong shape.
 *
 * XBIOS ABI (EmuTOS): arguments pushed right-to-left, the function number WORD
 * on top of the user stack.  Setscreen(log, phys, rez) therefore exposes
 * log=a[0..1], phys=a[2..3], rez=a[4].
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;

/* Setscreen(log, phys, rez): push rez (WORD), phys (LONG), log (LONG), fn WORD. */
static int32_t setscreen(int32_t log, int32_t phys, int16_t rez)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.w %1,-(%%sp)\n\t"
                         "move.l %2,-(%%sp)\n\t"
                         "move.l %3,-(%%sp)\n\t"
                         "move.w %4,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "add.l #12,%%sp"
                         : "=r"(r)
                         : "d"((int32_t)rez), "r"(phys), "r"(log), "r"((int32_t)5)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}
/* One 32-bit argument (Setpalette pointer etc.): arg1 at usp+2. */
static int32_t xbios1(int16_t fn, int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r) : "r"(a), "r"((int32_t)fn)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}

/* Standard ST screen: 320x200x4.  stride = (320/8)*4 = 160 B; one 16-pixel
   group = 4 words (plane 0..3), total 160*200 = 32000 bytes. */
#define SCREEN_STRIDE 160u
#define SCREEN_BYTES 32000u

/* Milestone markers at 0x0800 (free TOS scratch): report how far the program
   got, so a black window can be told apart from "never reached the draw". */
static void mark(unsigned at, uint8_t value)
{
    uint32_t address = 0x0800u + at;
    __asm__ __volatile__("move.b %1,%0@" : : "a"(address), "d"((uint32_t)value) : "memory");
}

/* One pixel: plane words interleaved (group*4 + plane), bit 15-(x&15). */
static void set_pixel(uint8_t *screen, int x, int y, uint8_t colour)
{
    unsigned group = (unsigned)(x >> 4), bit = 15u - ((unsigned)x & 15u);
    uint16_t mask = (uint16_t)(0x8000u >> bit);
    int plane;
    for (plane = 0; plane < 4; ++plane) {
        volatile uint8_t *w = screen + (uint32_t)y * SCREEN_STRIDE +
                              (uint32_t)(group * 4u + (unsigned)plane) * 2u;
        uint16_t word = (uint16_t)(((uint16_t)w[0] << 8) | w[1]);
        if (colour & (1u << plane)) word = (uint16_t)(word | mask);
        else word = (uint16_t)(word & (uint16_t)~mask);
        w[0] = (uint8_t)(word >> 8); w[1] = (uint8_t)word;
    }
}
static void fill(uint8_t *screen, uint8_t colour)
{
    int y, g, p;
    for (y = 0; y < 200; ++y)
        for (g = 0; g < 20; ++g)
            for (p = 0; p < 4; ++p) {
                volatile uint8_t *w = screen + (uint32_t)y * SCREEN_STRIDE +
                                      (uint32_t)(g * 4 + p) * 2u;
                uint8_t v = (colour & (1u << p)) ? 0xff : 0x00;
                w[0] = v; w[1] = v;
            }
}
static void bar(uint8_t *screen, int x, uint8_t colour)
{
    int y, i;
    for (y = 0; y < 200; ++y)
        for (i = 0; i < 16; ++i) set_pixel(screen, x + i, y, colour);
}
static void border(uint8_t *screen, uint8_t colour)
{
    int i;
    for (i = 0; i < 320; ++i) { set_pixel(screen, i, 0, colour); set_pixel(screen, i, 199, colour); }
    for (i = 0; i < 200; ++i) { set_pixel(screen, 0, i, colour); set_pixel(screen, 319, i, colour); }
    for (i = 0; i < 200; ++i) { set_pixel(screen, i, i, colour); set_pixel(screen, 319 - i, i, colour); }
}

/* GEMDOS call with one LONG argument (Cconis/Crawcin take none): the key check
   below cuts the demonstration hold short so the desktop is not blocked for the
   whole window - any key returns control immediately. */
static int32_t gdos1(int16_t fn, int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(a), "r"((int32_t)fn)
                         : "d1", "d2", "a0", "a1", "a2", "memory");
    return r;
}

int32_t _tos_main(void)
{
    static uint16_t colours[16] = {
        0x000, 0xfff, 0xf00, 0x0f0, 0x00f, 0xff0, 0x0ff, 0xf0f,
        0x700, 0x070, 0x007, 0x777, 0xaaa, 0x333, 0xfb0, 0xb0f
    };
    uint8_t *screen;
    int saved_mode, i;

    mark(0u, 1u);                          /* entered _tos_main */
    mark(1u, 0xAAu);                       /* marker stores work */
    saved_mode = (int)xbios1(4, 0);        /* Getrez */
    mark(4u, (uint8_t)(saved_mode & 0xffu)); /* XBIOS entry works */

    mark(5u, 0xC0u);                       /* before Setscreen */
    (void)setscreen(-1, -1, 0);            /* keep screen, rez 0 = 320x200x16 */
    mark(6u, 0xCDu);                       /* Setscreen returned */

    screen = (uint8_t *)xbios1(2, 0);      /* Physbase */
    mark(7u, 0xDDu);                       /* about to Setpalette */
    {
        volatile uint32_t palette_address = (uint32_t)colours;
        (void)xbios1(6, (int32_t)palette_address);   /* Setpalette */
    }
    mark(8u, 0xEEu);                       /* Setpalette returned */

    screen = (uint8_t *)xbios1(3, 0);      /* Logbase (private buffer) */
    if (!screen) { return 1; }

    fill(screen, 0x0);
    for (i = 0; i < 16; ++i) bar(screen, 4 + i * 16, (uint8_t)i);
    for (i = 0; i < 16; ++i) {
        set_pixel(screen, 4 + i * 16 + 2, 4 + i, 0x0);
        set_pixel(screen, 4 + i * 16 + 3, 4 + i, 0x0);
    }
    border(screen, 0xf);
    mark(9u, 0x55u);                       /* drawing finished */

    /* Pump the compositor: each Vsync is a trap #14 whose dispatcher runs
       tos_composite_work, so the bars reach the WM surface (past the /5
       divider) BEFORE the hold. */
    { int vi; for (vi = 0; vi < 12; ++vi) (void)xbios1(37, 0); }

    /* Bounded hold so the window stays visible long enough to capture. */
    { volatile uint32_t spin;
      for (spin = 0; spin < 60000000u; ++spin) {
          /* Any key ends the hold (and is consumed), so the user is never
             locked out of the desktop while a TOS program is on screen. */
          if((spin & 0xFFFFu)==0u && gdos1(0x0Bu, 0) > 0) { (void)gdos1(0x07u, 0); break; }
          if((spin & 0x3FFFFu)==0u) (void)xbios1(37, 0);   /* keep the compositor + pointer alive */
      } }

    (void)setscreen(-1, -1, (int16_t)saved_mode);   /* restore */
    return 0;
}
