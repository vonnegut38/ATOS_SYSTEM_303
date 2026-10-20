/* TOS window MEDIUM-res colour client test: 640x200x4 (rez 1).
 *
 * Switches to rez 1 (640x200, 2 bitplanes) and paints four vertical bars of
 * colours 0..3 plus a border, so the 2-plane expansion and the 640-wide client
 * can be verified.  Layout is the standard ST planar interleave: one word per
 * plane per 16-pixel group, so stride = (640/16)*2*2 = 160 bytes.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;

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
static void mark(unsigned at, uint8_t value)
{
    uint32_t address = 0x0860u + at;
    __asm__ __volatile__("move.b %1,%0@" : : "a"(address), "d"((uint32_t)value) : "memory");
}

#define STRIDE 160u

static void set_pixel(uint8_t *screen, int x, int y, uint8_t colour)
{
    uint16_t *g = (uint16_t *)(void *)(screen + (uint32_t)y * STRIDE + (uint32_t)(x >> 4) * 4u);
    uint16_t bit = (uint16_t)(0x8000u >> (x & 15));
    if (colour & 1u) g[0] |= bit;
    if (colour & 2u) g[1] |= bit;
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
    uint8_t *screen;
    int x, y;

    static uint16_t colours[16] = {
        0xfff, 0x000, 0xf00, 0x0f0, 0x00f, 0xff0, 0x0ff, 0xf0f,
        0x700, 0x070, 0x007, 0x777, 0xaaa, 0x333, 0xfb0, 0xb0f
    };
    mark(0u, 0x01u);
    (void)setscreen(-1, -1, 1);            /* rez 1 = 640x200, 4 colours */
    mark(1u, 0x02u);
    { volatile uint32_t pa = (uint32_t)colours;
      (void)xbios1(6, (int32_t)pa); }      /* Setpalette: white/black/red/green */
    mark(2u, 0x03u);

    screen = (uint8_t *)xbios1(3, 0);
    if (!screen) return 1;

    for (y = 0; y < 200; ++y)
        for (x = 0; x < 640; ++x)
            set_pixel(screen, x, y,
                      (uint8_t)(x >= 480 ? 3 : x >= 320 ? 2 : x >= 160 ? 1 : 0));  /* 4 bars */
    for (x = 0; x < 640; ++x) { set_pixel(screen, x, 0, 1); set_pixel(screen, x, 199, 1); }
    for (y = 0; y < 200; ++y) { set_pixel(screen, 0, y, 1); set_pixel(screen, 639, y, 1); }
    mark(3u, 0x55u);

    { int vi; for (vi = 0; vi < 12; ++vi) (void)xbios1(37, 0); }
    { volatile uint32_t spin;
      for (spin = 0; spin < 60000000u; ++spin) {
          /* Any key ends the hold (and is consumed), so the user is never
             locked out of the desktop while a TOS program is on screen. */
          if((spin & 0xFFFFu)==0u && gdos1(0x0Bu, 0) > 0) { (void)gdos1(0x07u, 0); break; }
          if((spin & 0x3FFFFu)==0u) (void)xbios1(37, 0);   /* keep the compositor + pointer alive */
      } }

    return 0;
}
