#ifndef ASF_VIDEO_DRIVER_H
#define ASF_VIDEO_DRIVER_H
#include <stdint.h>
/* Phase 7/11 (v10): the ONLY interface VDI/AES/Desktop may use for output.
 * No code outside a backend may test for ET4000/Shifter.  16bpp is out of
 * scope for v10; start with PLANAR and CHUNKY_8BPP. */
typedef enum {
    VIDEO_FMT_PLANAR_1BPP = 1,
    VIDEO_FMT_PLANAR_2BPP = 2,
    VIDEO_FMT_PLANAR_4BPP = 4,
    VIDEO_FMT_CHUNKY_8BPP = 8
} video_format_t;

typedef struct {
    uint16_t width,height,pitch;   /* pitch = bytes per line */
    uint8_t  format;               /* video_format_t */
    uint8_t  *pixels;
} video_surface_t;

typedef struct video_driver {
    const char *name;
    int  (*init)(void);
    void (*shutdown)(void);
    int  (*set_mode)(uint16_t width,uint16_t height,uint8_t format);
    const video_surface_t *(*get_surface)(void);
    void (*set_palette)(const uint16_t *rgb,uint16_t first,uint16_t count);
    void (*fill_rect)(int x,int y,int w,int h,uint8_t color);
    void (*draw_line)(int x0,int y0,int x1,int y1,uint8_t color);
    void (*blit)(int dx,int dy,const video_surface_t *src,int sx,int sy,int w,int h);
    void (*show_cursor)(int x,int y);
    void (*hide_cursor)(void);
    void (*vsync)(void);
} video_driver_t;

void video_register(video_driver_t *driver);
video_driver_t *video_select(const char *name);
video_driver_t *video_active(void);

/* Software backend used for the desktop/compositor and for host tests. */
void video_sw_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch,uint8_t format);
video_driver_t *video_sw_driver(void);

/* v10: the compositor publishes its framebuffer here and reads it back through
 * this abstraction instead of calling kgfx_* directly. */
void video_screen_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch,uint8_t format);
const video_surface_t *video_screen_surface(void);

/* One pixel helper shared by backends/tests; honours the surface format. */
void video_put_pixel(video_surface_t *s,int x,int y,uint8_t color);
#endif
