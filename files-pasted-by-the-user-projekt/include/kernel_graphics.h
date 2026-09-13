#ifndef ASF_KERNEL_GRAPHICS_H
#define ASF_KERNEL_GRAPHICS_H
#include <stdint.h>
#include "gui_draw.h"
void kgfx_draw_batch(const gui_draw_t *,uint32_t);
/* Haczyk "pompy" kursora: wolany w trakcie rysowania, zeby wskaznik nie
 * zastygal na czas dlugiego renderu. Ustawiany przez jadro. */
extern void (*kgfx_cursor_pump)(void);

#define KGFX_DIRTY_MAX 16u
#define KGFX_SERVICE_MAGIC 0x4b474658UL
#define KGFX_SERVICE_VERSION 2u
#define KGFX_CAP_LINE             0x0001u
#define KGFX_CAP_PALETTE          0x0002u
#define KGFX_CAP_BLITTER_PRESENT  0x0004u
#define KGFX_CAP_BLITTER_ENABLED  0x0008u
#define KGFX_CAP_DESKTOP_BUFFER   0x0010u
#define KGFX_CAP_MONO_WORD_COPY   0x0020u
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
void kgfx_emutos_background(int32_t,int32_t,int32_t,int32_t);
void kgfx_emutos_controls(int32_t,int32_t,int32_t,int32_t,uint32_t);
void kgfx_emutos_window(int32_t,int32_t,int32_t,int32_t);
void kgfx_clear(uint8_t color);
void kgfx_pixel(int32_t x,int32_t y,uint8_t color);
void kgfx_fill_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color);
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
