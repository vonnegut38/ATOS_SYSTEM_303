#ifndef ASF_KERNEL_GRAPHICS_H
#define ASF_KERNEL_GRAPHICS_H
#include <stdint.h>
#include "gui_draw.h"
#include "text_terminal.h"
void kgfx_terminal_invalidate(void);
int kgfx_terminal_present(const term_cell_t *);
int kgfx_terminal_submit(unsigned,const term_cell_t *);
int kgfx_console_present(term_display_t *,const term_cell_t *);
/* The next console render repaints the full grid (window re-shown). */
void kgfx_console_force_redraw(void);
void kgfx_session_view(int);
void kgfx_overlay_hide(void);
void kgfx_overlay_show(const term_cell_t *,unsigned);
void kgfx_draw_batch(const gui_draw_t *,uint32_t);
/* Bounding box (half-open, target-relative) of the last successful batch: the
   window manager uses it to damage exactly what an application repainted. */
int kgfx_batch_bounds(int32_t *,int32_t *,int32_t *,int32_t *);
/* Requests-per-frame measurement: one bump per serviced GFX request. */
void kgfx_note_request(void);
uint32_t kgfx_request_count(void);

#define KGFX_DIRTY_MAX 16u
#define KGFX_SERVICE_MAGIC 0x4b474658UL
#define KGFX_SERVICE_VERSION 2u
#define KGFX_CAP_LINE             0x0001u
#define KGFX_CAP_PALETTE          0x0002u
#define KGFX_CAP_BLITTER_PRESENT  0x0004u
#define KGFX_CAP_BLITTER_ENABLED  0x0008u
#define KGFX_CAP_DESKTOP_BUFFER   0x0010u
#define KGFX_CAP_MONO_WORD_COPY   0x0020u
#define KGFX_CAP_BLITTER_COPY     0x0040u   /* hardware planar copy self-tested */
typedef struct {int16_t x,y;uint16_t width,height;} kgfx_rect_t;
typedef struct {
    uint16_t width,height,line_bytes;
    uint8_t planes,mode;
    uint16_t capabilities;
    volatile uint8_t *framebuffer;
} kgfx_info_t;

int kgfx_init(uint8_t mode,uint8_t blitter_present,uint8_t blitter_enabled);
int kgfx_self_test(void);
const kgfx_info_t *kgfx_get_info(void);
/* Compositor access: the desktop framebuffer, its bytes-per-line and bpp. */
uint8_t *kgfx_framebuffer(uint16_t *pitch,uint8_t *bpp);
/* Render a console cell grid into a caller buffer (640x400 mono). */
/* The surface the compositor PRESENTS (the scanned-out desktop frame).  Draw
   content that must survive compositor flushes - a TOS window client - here,
   not into kgfx_set_target()'s application buffer. */
uint8_t *kgfx_present_target(uint16_t *pitch,uint8_t *bpp);
/* Compositor: point drawing at a window surface; returns the previous target. */
uint8_t *kgfx_set_target(uint8_t *target);
int kgfx_console_render(const void *cells,uint8_t *target,
                        uint16_t *dx,uint16_t *dy,uint16_t *dw,uint16_t *dh);
void kgfx_emutos_background(int32_t,int32_t,int32_t,int32_t);
void kgfx_icon(int32_t x,int32_t y,uint8_t icon,uint8_t ink,uint8_t paper,const char *label,int invert);
void kgfx_icon_color(int32_t x,int32_t y,uint8_t id,const char *label);
void kgfx_icon_palette(void);
/* Wczytanie ikon pulpitu z bloba w TT-RAM (plik \\SYSTEM\\ICONS.DAT). */
int kgfx_icons_load(const void *blob,uint32_t size);
int kgfx_icons_loaded(void);
void kgfx_icons_unload(void);
/* Zyczenie rozmiaru (32/48/64) na nastepne kgfx_icons_load(); 0 = domyslne 48. */
void kgfx_icons_request(uint16_t px);
/* Biezacy rozmiar ikon - tyle pikseli ma bok rysowanej ikony. */
uint16_t kgfx_icons_size(void);
/* Tryb siatki pulpitu: 0 COMPACT, 1 NORMAL, 2 WIDE. */
void kgfx_icon_grid_set(uint8_t mode);
uint8_t kgfx_icon_grid(void);
void kgfx_icon_register(uint8_t slot,const uint8_t *mask,const uint8_t *pixels);
int kgfx_icon_from_png(uint8_t slot,const uint8_t *png,uint32_t png_size);
void kgfx_emutos_controls(int32_t,int32_t,int32_t,int32_t,uint32_t);
void kgfx_emutos_window(int32_t,int32_t,int32_t,int32_t);
void kgfx_clear(uint8_t color);
void kgfx_pixel(int32_t x,int32_t y,uint8_t color);
void kgfx_fill_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color);
void kgfx_pattern_fill(int32_t x,int32_t y,int32_t width,int32_t height,unsigned pattern);
void kgfx_tile_clear(void);
void kgfx_tile_set(int x,int y,int on);
int  kgfx_tile_get(int x,int y);
const uint8_t *kgfx_tile_bytes(void);
void kgfx_tile_from_bytes(const uint8_t *src);
void kgfx_tile_put_byte(unsigned i,unsigned v);
void kgfx_tile_grid(int32_t x,int32_t y,int32_t cw,int32_t ch);
void kgfx_tile_set_fg(unsigned rgb12);
unsigned kgfx_tile_fg(void);
/* Blit a packed 1-bpp mono bitmap (MSB first) into the chunky framebuffer. */
void kgfx_blit_mono(const uint8_t *source,int32_t dx,int32_t dy,int32_t width,int32_t height,
                    int32_t stride,uint8_t ink,uint8_t paper);
/* Blit a planar Atari screen into the chunky framebuffer, one index byte per
   pixel.  The source uses the STANDARD ST layout: bitplanes interleaved at
   16-bit word granularity (one word per plane per 16-pixel group, big-endian,
   MSB = leftmost pixel); stride = (width/8)*planes bytes per row.  Lets a TOS
   program's own ST screen appear inside a desktop window. */
void kgfx_blit_planar(const uint8_t *source,uint16_t source_width,uint16_t source_height,
                      uint8_t source_bpp,uint16_t source_stride,uint32_t source_capacity,
                      int32_t dx,int32_t dy,int32_t clip_x,int32_t clip_y,
                      int32_t clip_width,int32_t clip_height,uint8_t palette_base);
void kgfx_frame_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color);
void kgfx_line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint8_t color);
int kgfx_set_palette(uint8_t index,uint16_t color);
void kgfx_copy_rect(int32_t source_x,int32_t source_y,int32_t width,int32_t height,
                    int32_t destination_x,int32_t destination_y);
void kgfx_text(int32_t x,int32_t y,const char *text,uint8_t foreground,
               uint8_t background,int opaque);
void kgfx_cursor_show(int32_t x,int32_t y);
void kgfx_cursor_move(int32_t x,int32_t y);
void kgfx_cursor_hide(void);
int kgfx_cursor_visible(void);
int kgfx_enter_desktop(void);
void kgfx_restore_console(void);
int kgfx_desktop_active(void);
/* Adopt the active video geometry and depth (1 = planar, 8 = chunky). */
void kgfx_set_depth(uint16_t width,uint16_t height,uint8_t bpp);
/* (Re)program the 16 system colours, including the card RAMDAC when active. */
void kgfx_palette_defaults(void);
/* GFX opcode 30: switch the adapter + compositor to a new mode (0 = ok). */
int kgfx_video_reconfigure(uint16_t width,uint16_t height);
/* Live mode switch with an explicit depth (8 = chunky 256 colours). */
int kgfx_video_reconfigure_format(uint16_t width,uint16_t height,uint8_t bpp);
/* 8 bpp chunky -> 16 bpp 5:6:5 z tabela palety systemowej (tryb 800x600x16). */
int kgfx_present_packed16_pal(uint8_t *dst,uint16_t dst_pitch,const uint8_t *src,
                              uint16_t src_pitch,uint16_t width,uint16_t height);
void kgfx_dirty_reset(void);
uint8_t kgfx_dirty_count(void);
int kgfx_dirty_get(uint8_t index,kgfx_rect_t *rectangle);

typedef struct {
    uint32_t magic;
    uint16_t version,size;
    const kgfx_info_t *(*info)(void);
    void (*clear)(uint8_t color);
    void (*pixel)(int32_t x,int32_t y,uint8_t color);
    void (*fill_rect)(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color);
    void (*frame_rect)(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color);
    void (*line)(int32_t,int32_t,int32_t,int32_t,uint8_t);
    int (*set_palette)(uint8_t,uint16_t);
    void (*copy_rect)(int32_t,int32_t,int32_t,int32_t,int32_t,int32_t);
    void (*text)(int32_t,int32_t,const char *,uint8_t,uint8_t,int);
    void (*dirty_reset)(void);
    uint8_t (*dirty_count)(void);
    int (*dirty_get)(uint8_t index,kgfx_rect_t *rectangle);
} kgfx_service_t;
const kgfx_service_t *kgfx_get_service(void);

#endif
