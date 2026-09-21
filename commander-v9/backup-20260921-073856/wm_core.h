#ifndef WM_CORE_H
#define WM_CORE_H
/* Compositor core: pure geometry + ordering, no kernel dependencies, so it
   can be unit tested on the host. The kernel integration (blitting into the
   framebuffer) passes a callback. */
#include <stdint.h>

#define WM_MAX 8u
#define WM_CLIP_MAX 8u
#define WM_DAMAGE_MAX 8u

typedef struct {int16_t x,y;uint16_t w,h;} wm_rect_t;
typedef struct {
    wm_rect_t frame;      /* full window incl. frame/border */
    wm_rect_t client;     /* drawable area (what the app owns) */
    uint32_t pid;         /* owning process, 0 = system */
    uint8_t flags;        /* 1 = visible */
    int16_t src_x,src_y;  /* top-left of the visible part inside the surface */
} wm_window_t;
typedef struct {
    wm_window_t win[WM_MAX];
    unsigned count;                 /* index 0 = bottom of the Z order */
    wm_rect_t clip[WM_MAX][WM_CLIP_MAX];
    unsigned clip_count[WM_MAX];
    wm_rect_t damage[WM_DAMAGE_MAX];
    unsigned damage_count;
    int16_t screen_w,screen_h;
} wm_state_t;

int  wm_rect_valid(const wm_rect_t *r);
int  wm_rect_intersect(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out);
int  wm_rect_union(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out);
void wm_init(wm_state_t *s,int16_t w,int16_t h);
void wm_damage_add(wm_state_t *s,wm_rect_t r);
void wm_damage_clear(wm_state_t *s);
void wm_rebuild_clip(wm_state_t *s);
/* Move a window (frame and client follow) and damage old + new: the classic
   "drag with content" step. The owning app is never asked to redraw. */
void wm_move(wm_state_t *s,unsigned index,int16_t nx,int16_t ny);
/* Resize a window: frame takes the new size, client is clamped inside it,
   and damage covers old + new.  The owning app is never asked to redraw. */
void wm_resize(wm_state_t *s,unsigned index,uint16_t w,uint16_t h);
/* Hit testing for the mouse. Returns the topmost window under (x,y) or
   WM_NO_WINDOW; wm_hit_part says whether the title bar or the client was hit. */
#define WM_NO_WINDOW 0xffffu
#define WM_PART_TITLE 1
#define WM_PART_CLIENT 2
#define WM_PART_FRAME 3
unsigned wm_hit(const wm_state_t *s,int16_t x,int16_t y);
int wm_hit_part(const wm_state_t *s,unsigned index,int16_t x,int16_t y);
/* Mouse drag state machine: begin on press, update on move, end on release.
   Grabbing the title moves the window; grabbing elsewhere resizes it. */
typedef struct {
    uint8_t active,part;
    unsigned window;
    int16_t grab_x,grab_y,x0,y0;
    uint16_t w0,h0;
} wm_drag_t;
void wm_drag_begin(wm_drag_t *g,const wm_state_t *s,int16_t x,int16_t y);
/* Bring a window to the top of the stacking order (click-to-focus). */
void wm_raise(wm_state_t *s,unsigned index);
void wm_drag_update(wm_drag_t *g,wm_state_t *s,int16_t x,int16_t y);
void wm_drag_end(wm_drag_t *g);
/* blit(ctx, pid (0=background), destination rect, source x, source y) */
typedef void (*wm_blit_fn)(void *ctx,uint32_t pid,const wm_rect_t *dst,uint16_t sx,uint16_t sy);
void wm_recompose(wm_state_t *s,wm_blit_fn blit,void *ctx);

#endif
