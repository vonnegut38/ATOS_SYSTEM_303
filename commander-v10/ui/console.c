#include "console.h"
#include "font.h"
#include "hardware.h"
#include "system.h"
static uint16_t cursor_x, cursor_y;
static uint8_t inverse;
static uint16_t text_cell_height(void);

static void console_next_row(void)
{
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
    uint16_t y, x, longs=(uint16_t)(shifter_line_bytes()/4u);
    volatile uint32_t *screen=(volatile uint32_t *)(uintptr_t)SCREEN_BASE;
    uint16_t height=text_cell_height();
    for (y=0; y<height; ++y) {
        volatile uint32_t *line=screen+(uint32_t)(row*height+y)*longs;
        for (x=0; x<longs; ++x) line[x]=0;
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
    if (cursor_x >= shifter_text_columns()) { cursor_x=0; console_next_row(); }
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
