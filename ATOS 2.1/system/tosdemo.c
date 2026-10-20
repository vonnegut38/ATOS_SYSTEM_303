/* TOS window demo: cycles rez 0 -> 1 -> 2 in one program so all three client
 * resolutions can be watched live.
 *
 * Every stage writes FULL words (set and clear bits), so no stale plane content
 * survives a mode switch and no pre-clear pass is needed.  Marker 0x0880 holds
 * the current stage: 0 = 320x200x16, 1 = 640x200x4, 2 = 640x400 mono,
 * 0xFF = done.
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
    uint32_t address = 0x0880u + at;
    __asm__ __volatile__("move.b %1,%0@" : : "a"(address), "d"((uint32_t)value) : "memory");
}
/* One 16-pixel group of one plane: MSB = leftmost pixel. */
static void put_group(uint8_t *screen, uint32_t offset, uint16_t word)
{
    screen[offset] = (uint8_t)(word >> 8);
    screen[offset + 1u] = (uint8_t)word;
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

static void hold(void)
{
    int i;
    for (i = 0; i < 12; ++i) (void)xbios1(37, 0);   /* pump the compositor */
    { volatile uint32_t spin;
      for (spin = 0; spin < 12000000u; ++spin) {
          if((spin & 0xFFFFu)==0u && gdos1(0x0Bu, 0) > 0) { (void)gdos1(0x07u, 0); break; }
          /* Keep the compositor alive during the hold.  While a TOS program
             runs the kernel main loop is blocked, so nothing would present a
             frame or move the pointer unless the program keeps trapping: the
             Vsync below is that trap (XBIOS 37 -> band present + cursor). */
          if((spin & 0x3FFFFu)==0u) (void)xbios1(37, 0);
      } }
}

int32_t _tos_main(void)
{
    static uint16_t colours[16] = {
        0x000, 0xfff, 0xf00, 0x0f0, 0x00f, 0xff0, 0x0ff, 0xf0f,
        0x700, 0x070, 0x007, 0x777, 0xaaa, 0x333, 0xfb0, 0xb0f
    };
    uint8_t *screen;
    int y, g, p;

    /* rez 0: 320x200x16 -> 16 vertical bars (one 16-pixel group per bar) */
    mark(0u, 0x00u);
    (void)setscreen(-1, -1, 0);
    { volatile uint32_t pa = (uint32_t)colours; (void)xbios1(6, (int32_t)pa); }
    screen = (uint8_t *)xbios1(3, 0);
    if (!screen) return 1;
    for (y = 0; y < 200; ++y)
        for (g = 0; g < 20; ++g) {
            uint8_t col = (uint8_t)(g & 15);
            for (p = 0; p < 4; ++p)
                put_group(screen, (uint32_t)y * 160u + (uint32_t)(g * 4 + p) * 2u,
                          (uint16_t)((col & (1u << p)) ? 0xffffu : 0x0000u));
        }
    hold();

    /* rez 1: 640x200x4 -> 4 bars of 160 px (10 groups each) */
    mark(0u, 0x01u);
    (void)setscreen(-1, -1, 1);
    { volatile uint32_t pa = (uint32_t)colours; (void)xbios1(6, (int32_t)pa); }
    screen = (uint8_t *)xbios1(3, 0);
    if (!screen) return 1;
    for (y = 0; y < 200; ++y)
        for (g = 0; g < 40; ++g) {
            uint8_t col = (uint8_t)(g < 10 ? 0 : g < 20 ? 1 : g < 30 ? 2 : 3);
            for (p = 0; p < 2; ++p)
                put_group(screen, (uint32_t)y * 160u + (uint32_t)(g * 2 + p) * 2u,
                          (uint16_t)((col & (1u << p)) ? 0xffffu : 0x0000u));
        }
    hold();

    /* rez 2: 640x400 mono -> 8x8 checkerboard (one byte = 8 px = one block) */
    mark(0u, 0x02u);
    (void)setscreen(-1, -1, 2);
    screen = (uint8_t *)xbios1(3, 0);
    if (!screen) return 1;
    for (y = 0; y < 400; ++y) {
        uint8_t *row = screen + (uint32_t)y * 80u;
        for (g = 0; g < 80; ++g)
            row[g] = (uint8_t)(((g ^ (y >> 3)) & 1) ? 0xFF : 0x00);
    }
    hold();

    mark(0u, 0xFFu);
    return 0;
}
