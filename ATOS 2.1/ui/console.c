#include "console.h"
#include "font.h"
#include "hardware.h"
#include "system.h"
#include "kernel_blitter.h"
static uint16_t cursor_x, cursor_y;
static uint8_t inverse;
/* Scanout base: the Shifter framebuffer, or the adapter's VRAM once the ROM
   has brought the card up (same 8x16 mono byte layout). */
/* Initialised data: the ROM startup copies .data from ROM to RAM (see
   boot/reset.S), so a normal initialiser is safe again. */
static volatile uint8_t *console_screen=(volatile uint8_t *)(uintptr_t)SCREEN_BASE;
static volatile uint8_t *console_target(void){return console_screen;}
/* The POST screen paints its logo as rectangles, so it needs the framebuffer
   the console is writing to (the generic video driver is not linked into the
   ROM).  Read-only for everyone else. */
volatile uint8_t *console_screen_ptr(void){return console_screen;}
volatile uint8_t *console_screen_ptr(void);
static uint16_t text_cell_height(void);
void console_set_screen(volatile uint8_t *screen)
{if(screen)console_screen=screen;}
/* Character mirror of the visible screen so BOOT.SYS and the kernel can keep
   one continuous log instead of clearing and redrawing it. */
#define CONSOLE_TEXT_COLS 80u
#define CONSOLE_TEXT_ROWS 25u
static char console_text[CONSOLE_TEXT_COLS*CONSOLE_TEXT_ROWS];
static void console_text_blank_rows(uint16_t first,uint16_t last)
{
    uint16_t r,c;
    for(r=first;r<=last&&r<CONSOLE_TEXT_ROWS;++r)
        for(c=0;c<CONSOLE_TEXT_COLS;++c)console_text[(uint32_t)r*CONSOLE_TEXT_COLS+c]=' ';
}

static void console_next_row(void)
{
    uint32_t i,frame=shifter_frame_bytes(),row_bytes=(uint32_t)text_cell_height()*shifter_line_bytes();
    volatile uint32_t *screen=(volatile uint32_t *)console_target();
    if (cursor_y<24u) { ++cursor_y; return; }
    for(i=0;i<(frame-row_bytes)/4u;++i)
        screen[i]=screen[i+row_bytes/4u];
    for(;i<frame/4u;++i)screen[i]=0;
    for(i=0;i<(CONSOLE_TEXT_ROWS-1u)*CONSOLE_TEXT_COLS;++i)
        console_text[i]=console_text[i+CONSOLE_TEXT_COLS];
    console_text_blank_rows((uint16_t)(CONSOLE_TEXT_ROWS-1u),(uint16_t)(CONSOLE_TEXT_ROWS-1u));
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
    volatile uint32_t *screen=(volatile uint32_t *)console_target();
    /* UI redraw must never reprogram the Shifter.  Reinitializing video while
     * it is scanning the framebuffer can produce unstable frames and was also
     * making menu navigation dependent on the live monitor-detect signal. */
    /* On the Shifter framebuffer in mono, a present+enabled BLiTTER clears the
       whole 640x400 screen in one op.  Plenty of STs have no blitter, and the
       VOFA card VRAM is not on the blitter bus, so every other case uses the
       plain CPU loop. */
    if (console_target()==(volatile uint8_t *)(uintptr_t)SCREEN_BASE &&
        g_system.video_mode==VIDEO_ST_MONO && kblit_present() && kblit_enabled() &&
        kblit_fill_planar((volatile uint16_t *)screen,40u,1u,0u,0u,640u,400u,0u)) {
        /* blitter cleared the framebuffer */
    } else {
        uint32_t longs=shifter_frame_bytes()/4u;
        for (i=0; i<longs; ++i) screen[i]=0;
    }
    console_text_blank_rows(0u,(uint16_t)(CONSOLE_TEXT_ROWS-1u));
    cursor_x=cursor_y=0;
}

void console_clear_text_row(uint16_t row)
{
    uint16_t y, x, longs=(uint16_t)(shifter_line_bytes()/4u);
    volatile uint32_t *screen=(volatile uint32_t *)console_target();
    uint16_t height=text_cell_height();
    for (y=0; y<height; ++y) {
        volatile uint32_t *line=screen+(uint32_t)(row*height+y)*longs;
        for (x=0; x<longs; ++x) line[x]=0;
    }
    console_text_blank_rows(row,row);
}

/* Pre-expanded 8 bpp glyph rows (index = the glyph row bits, value = the eight
   pixel bytes).  Writing them byte-by-byte into the card's VRAM bound the
   colour boot to 128 stores per character; two 32-bit stores per row fix that.
   The pitch is a multiple of four in every chunky mode we program. */
static uint32_t chunky_glyph_rows[256][2];
static uint8_t chunky_glyph_ready;
static void chunky_glyph_prepare(void)
{
    unsigned v,i;
    if(chunky_glyph_ready)return;
    for(v=0;v<256u;++v)
        for(i=0;i<8u;++i)
            ((uint8_t *)chunky_glyph_rows[v])[i]=(uint8_t)((v>>(7u-i))&1u);
    chunky_glyph_ready=1u;
}
static void putc_core(char c,uint8_t thin)
{
    const uint8_t *glyph;
    uint16_t y, value, keep_mask, shift, height, line_bytes;
    volatile uint8_t *p;
    if (c == '\b') {
        if (cursor_x) { --cursor_x; putc_core(' ',thin); --cursor_x; }
        return;
    }
    if (c == '\n') { cursor_x=0; console_next_row(); return; }
    if (cursor_x >= shifter_text_columns()) { cursor_x=0; console_next_row(); }
    if (cursor_y < CONSOLE_TEXT_ROWS && cursor_x < CONSOLE_TEXT_COLS)
        console_text[(uint32_t)cursor_y*CONSOLE_TEXT_COLS+cursor_x]=c;
    /* Hoist the target and the two geometry calls out of the scanline loop
       and advance a byte pointer instead of recomputing the address.  The
       old code called console_target()/shifter_line_bytes()/
       shifter_group_bytes() once per scanline (48 calls per character), which
       dominated the visible redraw on a 68000. */
    height=text_cell_height();
    line_bytes=shifter_line_bytes();
    glyph = font_glyph(c,(uint8_t)(height==16u));
    if (shifter_chunky()) {
        /* 8 bpp: one BYTE per pixel, so each glyph bit becomes one byte
           (1 = white, 0 = black - the two system palette entries). */
        volatile uint8_t *q=(volatile uint8_t *)(uintptr_t)(console_target()
            +(uint32_t)cursor_y*height*line_bytes+(uint32_t)cursor_x*8u);
        chunky_glyph_prepare();
        for (y=0; y<height; ++y) {
            uint32_t lo,hi;
            value=(uint16_t)glyph[y];
            if (thin&&!value) { q+=line_bytes; continue; }
            lo=chunky_glyph_rows[value&0xffu][0];
            hi=chunky_glyph_rows[value&0xffu][1];
            if (inverse) {lo^=0xffffffffu;hi^=0xffffffffu;}
            *(volatile uint32_t *)q=lo;
            *(volatile uint32_t *)(q+4)=hi;
            q+=line_bytes;
        }
        ++cursor_x;
        return;
    }
    /* Each 8-pixel character occupies half of the plane-0 word. */
    shift=(cursor_x&1u) ? 0u : 8u;
    keep_mask=(cursor_x&1u) ? 0xff00u : 0x00ffu;
    p=(volatile uint8_t *)(uintptr_t)(console_target()
        +(uint32_t)cursor_y*height*line_bytes
        +(uint32_t)(cursor_x>>1)*shifter_group_bytes());
    for (y=0; y<height; ++y) {
        volatile uint16_t *w=(volatile uint16_t *)p;
        value=glyph[y];
        if (inverse) value=(uint16_t)(~value)&0x00ffu;
        if (thin&&!value) { p+=line_bytes; continue; }   /* blank scanline */
        *w=(uint16_t)((*w&keep_mask)|(value<<shift));
        p+=line_bytes;
    }
    ++cursor_x;
}
void console_putc(char c){putc_core(c,0u);}
void console_putc_thin(char c){putc_core(c,1u);}
void console_puts_thin(const char *s){while(*s)putc_core(*s++,1u);}
void console_puts(const char *s) { while (*s) console_putc(*s++); }
void console_text_copy(char *destination,uint16_t *column,uint16_t *row)
{
    uint32_t i;
    if(destination)
        for(i=0;i<CONSOLE_TEXT_COLS*CONSOLE_TEXT_ROWS;++i)destination[i]=console_text[i];
    if(column)*column=cursor_x;
    if(row)*row=cursor_y;
}
void console_put_u32(uint32_t n)
{
    char b[10]; unsigned int i=0;
    if (!n) { console_putc('0'); return; }
    while (n && i<sizeof b) { b[i++]=(char)('0'+n%10u); n/=10u; }
    while (i) console_putc(b[--i]);
}
