#ifndef WM_BLIT_H
#define WM_BLIT_H
/* Surface -> framebuffer blits used by the compositor. Kept free of kernel
   dependencies so the pixel maths can be unit tested on the host. */
#include <stdint.h>
#include "wm_core.h"

typedef struct {
    uint8_t *data;
    uint16_t pitch;     /* bytes per line */
    uint16_t w,h;
    uint8_t bpp;        /* 1 = ST planar mono, 8 = chunky (ET4000) */
} wm_fb_t;

void wm_fill_rect(wm_fb_t *fb,const wm_rect_t *r,uint8_t level);
/* Win98-style 2px bevel: paper on the lit edges, ink on the shaded ones. */
void wm_bevel_rect(wm_fb_t *fb,const wm_rect_t *r,uint8_t ink,uint8_t paper,uint8_t sunken);
void wm_copy_rect(wm_fb_t *dst,const wm_fb_t *src,const wm_rect_t *d,uint16_t sx,uint16_t sy);

#endif
