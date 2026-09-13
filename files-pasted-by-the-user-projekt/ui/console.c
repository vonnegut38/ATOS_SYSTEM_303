#include "console.h"
#include "damage.h"
#include "font.h"
#include "hardware.h"
#include "system.h"
static uint16_t cursor_x, cursor_y;
static uint8_t inverse;
static uint16_t text_cell_height(void);

/* Tryb VGA (VOFA ET4000): VRAM karty 0x00C00000, 1 bpp, 80 bajtow na linie
 * - uklad identyczny z ekranem ST High, wiec obraz idzie 1:1 bez konwersji. */
#define CONSOLE_VGA_MEM   0x00c00000UL
#define CONSOLE_VGA_BYTES 80u
static uint8_t vga_console;

void console_set_vga(uint8_t enabled) { vga_console = enabled; }
static uint16_t text_columns(void) { return vga_console ? 80u : shifter_text_columns(); }

/* Shift the ET4000 framebuffer up by one text row.
 *
 * GCC 16 on m68k contracted the byte loop into
 *      moveb %a0@+,%a0@(-1280)
 * using the SAME address register as post-increment source and displacement
 * destination.  That is a documented 68000 hazard: the destination address is
 * taken after the increment, so every scroll shifted the whole picture one
 * byte to the right (the "screen breaks / shifts right" symptom).  Copy through
 * two distinct address registers instead. */
static void vga_scroll_up(uint32_t count,uint32_t row_bytes)
{
    volatile uint8_t *dst=(volatile uint8_t *)(uintptr_t)CONSOLE_VGA_MEM;
    volatile uint8_t *src=dst+row_bytes;
    __asm__ volatile(
        "1:\n\t"
        "moveb (%1)+,(%0)+\n\t"
        "subq.l #1,%2\n\t"
        "bne.s 1b\n\t"
        : "+a"(dst), "+a"(src), "+d"(count)
        :
        : "cc", "memory");
}

static void console_next_row(void)
{
    if (vga_console) {
        /* 80x25 text on the ET4000 (classic AWARD geometry).  The card is
         * 640x480, but the emulated ST-high raster is 640x400, so rows 26..30
         * used to be cropped in Hatari.  25 rows fit both the emulator and a
         * real VGA monitor. */
        cursor_y++;
        if (cursor_y < 25u) return;
        {
            volatile uint8_t *screen=(volatile uint8_t *)(uintptr_t)CONSOLE_VGA_MEM;
            uint32_t row_bytes=CONSOLE_VGA_BYTES*16u, i2;
            vga_scroll_up(CONSOLE_VGA_BYTES*(400u-16u),row_bytes);
            for (i2=CONSOLE_VGA_BYTES*(400u-16u);i2<CONSOLE_VGA_BYTES*400u;++i2) screen[i2]=0u;
            cursor_y=24u;
        }
        return;
    }
    uint32_t i,row_bytes=(uint32_t)text_cell_height()*shifter_line_bytes();
    volatile uint32_t *screen=(volatile uint32_t *)(uintptr_t)SCREEN_BASE;
    if (cursor_y<24u) { ++cursor_y; return; }
    for(i=0;i<(SCREEN_BYTES-row_bytes)/4u;++i)
        screen[i]=screen[i+row_bytes/4u];
    for(;i<SCREEN_BYTES/4u;++i)screen[i]=0;
}

static uint16_t text_cell_height(void)
{
    return g_system.video_mode == VIDEO_ST_MONO ? 16u : 8u;
}

void console_set_cursor(uint16_t x, uint16_t y) { cursor_x=x; cursor_y=y; }
void console_set_inverse(uint8_t enabled) { inverse=enabled; }
void console_clear(void)
{
    if (vga_console) {
        volatile uint8_t *screen=(volatile uint8_t *)(uintptr_t)CONSOLE_VGA_MEM;
        uint32_t i;
        for (i=0u;i<CONSOLE_VGA_BYTES*480u;++i) screen[i]=0u;
        damage_add(0,0,640,480);
        cursor_x=0; cursor_y=0;
        return;
    }
    uint32_t i;
    volatile uint32_t *screen=(volatile uint32_t *)(uintptr_t)SCREEN_BASE;
    /* UI redraw must never reprogram the Shifter.  Reinitializing video while
     * it is scanning the framebuffer can produce unstable frames and was also
     * making menu navigation dependent on the live monitor-detect signal. */
    for (i=0; i<SCREEN_BYTES/4u; ++i) screen[i]=0;
    cursor_x=cursor_y=0;
}

void console_clear_text_row(uint16_t row)
{
    uint16_t y, x, longs;
    if (vga_console) {
        /* Clear the row in the ET4000 framebuffer, not in Shifter RAM. */
        volatile uint8_t *vram=(volatile uint8_t *)(uintptr_t)CONSOLE_VGA_MEM;
        for (y=0;y<16u;++y)
            for (x=0;x<CONSOLE_VGA_BYTES;++x)
                vram[(uint32_t)(row*16u+y)*CONSOLE_VGA_BYTES+x]=0u;
        damage_add(0,row*16,640,16);
        return;
    }
    longs=(uint16_t)(shifter_line_bytes()/4u);
    {
    volatile uint32_t *screen=(volatile uint32_t *)(uintptr_t)SCREEN_BASE;
    uint16_t height=text_cell_height();
    for (y=0; y<height; ++y) {
        volatile uint32_t *line=screen+(uint32_t)(row*height+y)*longs;
        for (x=0; x<longs; ++x) line[x]=0;
    }
    }
}

void console_putc(char c)
{
    const uint8_t *glyph;
    uint16_t y, value, keep_mask, shift;
    volatile uint16_t *p;
    if (c == '\b') {
        if (cursor_x) { --cursor_x; console_putc(' '); --cursor_x; }
        return;
    }
    if (c == '\n') { cursor_x=0; console_next_row(); return; }
    if (cursor_x >= text_columns()) { cursor_x=0; console_next_row(); }
    if (vga_console) {
        uint16_t y2;
        glyph = font_glyph(c,1u);
        for (y2=0; y2<16u; ++y2) {
            uint8_t bits = glyph[y2];
            if (inverse) bits=(uint8_t)~bits;
            *(volatile uint8_t *)(uintptr_t)(CONSOLE_VGA_MEM
                +(uint32_t)(cursor_y*16u+y2)*CONSOLE_VGA_BYTES
                +(uint32_t)cursor_x) = bits;
        }
        damage_add(cursor_x*8,cursor_y*16,8,16);
        ++cursor_x;
        return;
    }
    {
        uint16_t height=text_cell_height();
        glyph = font_glyph(c,(uint8_t)(height==16u));
        /* Each 8-pixel character occupies half of the plane-0 word. */
        shift=(cursor_x&1u) ? 0u : 8u;
        keep_mask=(cursor_x&1u) ? 0xff00u : 0x00ffu;
        for (y=0; y<height; ++y) {
            value=glyph[y];
            if (inverse) value=(uint16_t)(~value)&0x00ffu;
            p=(volatile uint16_t *)(uintptr_t)(SCREEN_BASE
                +(cursor_y*height+y)*shifter_line_bytes()
                +(cursor_x>>1)*shifter_group_bytes());
            *p=(uint16_t)((*p&keep_mask)|(value<<shift));
        }
    }
    ++cursor_x;
}
void console_puts(const char *s) { while (*s) console_putc(*s++); }
void console_put_u32(uint32_t n)
{
    char b[10]; unsigned int i=0;
    if (!n) { console_putc('0'); return; }
    while (n && i<sizeof b) { b[i++]=(char)('0'+n%10u); n/=10u; }
    while (i) console_putc(b[--i]);
}
