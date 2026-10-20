/* TOS window MONO client test: 640x400x1 in a fixed 640x400 window.
 *
 * Switches the screen to rez 2 (640x400, mono) and paints an 8x8 checkerboard
 * plus a full border.  The 1:1 client (no 2:1 subsample) is the contract this
 * test verifies: an 8-pixel block must stay exactly 8 pixels wide on screen.
 *
 * Screen layout: standard ST mono, one bitplane, 80 bytes per row (640/8),
 * 80*400 = 32000 bytes total, MSB = leftmost pixel.
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
    uint32_t address = 0x0840u + at;
    __asm__ __volatile__("move.b %1,%0@" : : "a"(address), "d"((uint32_t)value) : "memory");
}

#define STRIDE 80u

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

    mark(0u, 0x01u);                       /* entered _tos_main */
    (void)setscreen(-1, -1, 2);            /* rez 2 = 640x400 mono */
    mark(1u, 0x02u);                       /* Setscreen returned */

    screen = (uint8_t *)xbios1(3, 0);      /* Logbase (private buffer) */
    if (!screen) return 1;
    mark(2u, 0x03u);

    /* 8x8 checkerboard: whole bytes, so the fill is 80 stores per row. */
    for (y = 0; y < 400; ++y) {
        uint8_t *row = screen + (uint32_t)y * STRIDE;
        for (x = 0; x < 80; ++x)
            row[x] = (uint8_t)(((x ^ (y >> 3)) & 1) ? 0xFF : 0x00);
    }
    /* Full border. */
    for (x = 0; x < 80; ++x) { screen[x] = 0xFF; screen[399u * STRIDE + (uint32_t)x] = 0xFF; }
    for (y = 0; y < 400; ++y) { screen[(uint32_t)y * STRIDE] |= 0x80u; screen[(uint32_t)y * STRIDE + 79u] |= 0x01u; }
    mark(3u, 0x55u);                       /* drawing finished */

    /* Pump the compositor (Vsync -> trap #14 -> tos_composite_work). */
    { int vi; for (vi = 0; vi < 12; ++vi) (void)xbios1(37, 0); }

    /* Bounded hold so the window stays visible long enough to capture. */
    { volatile uint32_t spin;
      for (spin = 0; spin < 60000000u; ++spin) {
          /* Any key ends the hold (and is consumed), so the user is never
             locked out of the desktop while a TOS program is on screen. */
          if((spin & 0xFFFFu)==0u && gdos1(0x0Bu, 0) > 0) { (void)gdos1(0x07u, 0); break; }
          if((spin & 0x3FFFFu)==0u) (void)xbios1(37, 0);   /* keep the compositor + pointer alive */
      } }

    return 0;
}
