#include "wm_reformat.h"
#include "kernel_fat.h"

/* Rozmiar okna konsoli zapamietany miedzy restartami (zapis w resize,
   odczyt raz przy starcie).  Deklaracje musza byc PRZED kodem resize. */
static uint16_t console_geom_w,console_geom_h;
static uint8_t  console_geom_valid,console_geom_tried;
#include "kernel_game.h"
#include "wm_clamp.h"
#include "chunky_raster.h"
#include "kernel_wm.h"
#include "kernel_graphics.h"
#include "kernel_theme.h"
#include "icons_ids.h"
#include "tos_async.h"
extern unsigned kernel_user_pid_terminal(uint32_t pid);
extern int32_t kernel_spawn_user_async(const char *,const char *,const char *,uint32_t *);
extern void kblock_cache_clear(void);
extern uint32_t kernel_desktop_active_pid(void);
extern int kernel_user_pid_name(uint32_t pid,char name[13]);
extern uint32_t kernel_user_find_running_app(const char *path,unsigned *session);
#include "font.h"
#include "gui_draw.h"
#include "kernel_console.h"
#include "kernel_sessions.h"
#include "kernel_video.h"
#include "video_driver.h"
#include "desktop_module.h"
#include "wm_core.h"
#include "wm_chrome.h"
extern desktop_module_t desktop_mod_clock;
#include "wm_blit.h"
#include "kernel_memory.h"
#include "kernel_blitter.h"
#include "rtc.h"

#define WM_SURFACE_MAX 8u
/* Application windows the compositor can own at once: 6 app windows plus the
   console window and the desktop canvas fill WM_MAX (8).  Each app window needs
   its own 32 KB mono surface, allocated lazily, so 3/5/6 windows cost 96/160/
   192 KB of surfaces. */
#define WM_APP_WINDOWS 6u
/* EmuTOS mono theme: colour 1 is white paper, colour 0 is black ink.  The top
   menu bar is white, so every compositor-owned top-bar element draws in ink. */
#define WM_INK   0u
#define WM_PAPER 1u

static wm_state_t wm;
static uint8_t wm_ready;
static uint8_t *wm_framebuffer;
static uint8_t wm_compose_forced;   /* compose outside the desktop-session gate */
static uint16_t wm_pitch;
static uint8_t wm_bpp;
static uint8_t *wm_surface[WM_SURFACE_MAX];
static uint16_t wm_surface_pitch,wm_surface_w,wm_surface_h;
static uint8_t wm_background_canvas;   /* a full-screen app canvas paints the desktop */
/* The top-bar clock applet is DISABLED for now (requested): it is the only
   reason the strip has to be repainted outside damage, and its module path was
   a variable while chasing the top-bar erasure.  Set to 1 to bring it back. */
#define WM_CLOCK_APPLET 0
static void task_draw(void);
static int task_chrome_at(int32_t x,int32_t y);
static void deco_text(wm_fb_t *fb,int16_t x,int16_t y,uint8_t code,
                      uint8_t ink,uint8_t paper,uint8_t opaque);
static uint32_t console_surface_bytes;
static uint8_t *app_surface[WM_APP_WINDOWS];
static uint32_t app_surface_bytes[WM_APP_WINDOWS];
static uint32_t app_surface_pid[WM_APP_WINDOWS];
static uint16_t app_surface_handle[WM_APP_WINDOWS];
static uint8_t app_surface_used[WM_APP_WINDOWS];

/* Per (process, window handle) requested geometry and title (GFX opcodes
   20/21).  Handle 0 is the app's primary window/canvas; a higher handle is an
   additional window the app owns (the desktop uses one for its window while
   keeping the full-screen canvas as handle 0). */
static struct {uint32_t pid;uint16_t handle;int16_t x,y;uint16_t w,h;uint8_t set;
               char title[24];uint8_t title_set;
               uint16_t content_h;uint8_t content_set;} wm_window_req[WM_APP_WINDOWS];
/* The WM identifies a window by its backing-surface pointer, not by the client
   pid, so title lookup has to translate through the app-surface table. */
static int app_surface_find(uint32_t pid,uint16_t handle)
{
    unsigned i;
    for(i=0;i<WM_APP_WINDOWS;++i)
        if(app_surface_used[i]&&app_surface_pid[i]==pid&&app_surface_handle[i]==handle)return (int)i;
    return -1;
}
static unsigned wm_window_slot(uint32_t pid,uint16_t handle)
{unsigned i;for(i=0;i<WM_APP_WINDOWS;++i)
    if(wm_window_req[i].set&&wm_window_req[i].pid==pid&&wm_window_req[i].handle==handle)return i;
 return WM_APP_WINDOWS;}
/* Client handle of a WM window's surface (0 for the kernel console). */
static uint32_t window_owner_handle(unsigned i)
{
    unsigned j;
    if(i>=wm.count)return 0u;
    for(j=0;j<WM_APP_WINDOWS;++j)
        if(app_surface_used[j]&&(uint32_t)(uintptr_t)app_surface[j]==wm.win[i].pid)
            return app_surface_handle[j];
    return 0u;
}
/* Owner pid of a WM window, found from its backing-surface pointer. */
static uint32_t window_owner_pid(unsigned i)
{
    unsigned j;
    if(i>=wm.count)return 0u;
    for(j=0;j<WM_APP_WINDOWS;++j)
        if(app_surface_used[j]&&(uint32_t)(uintptr_t)app_surface[j]==wm.win[i].pid)
            return app_surface_pid[j];
    return 0u;
}
/* Client pid owning the topmost window under a screen point, or 0 when the
   point is on the desktop canvas / task bar / console.  The kernel input router
   uses this to deliver mouse events to the window under the pointer. */
uint32_t kernel_wm_pid_at(int32_t x,int32_t y)
{
    int hit;
    if(!wm_ready)return 0u;
    if(x<0||y<0||x>=(int32_t)wm.screen_w||y>=(int32_t)wm.screen_h)return 0u;
    hit=wm_hit(&wm,(int16_t)x,(int16_t)y);
    if(hit==WM_NO_WINDOW)return 0u;
    return window_owner_pid((unsigned)hit);
}

static void window_close(unsigned i);
static void window_zoom(unsigned i);
/* Close every window owned by a dying client.  Without this an application
   that exits (or crashes) without unregistering leaves a ghost window on the
   compositor and a stale keyboard focus behind it. */
uint32_t kdbg_wm_close_pid,kdbg_wm_close_window;   /* who drops windows, and how often */
void kernel_wm_close_pid(uint32_t pid)
{
    ++kdbg_wm_close_pid;
    int i;
    if(!pid||!wm_ready)return;
    for(i=(int)wm.count-1;i>=0;--i){
        if(wm.win[i].flags&4u)continue;                     /* desktop background */
        if(window_owner_pid((unsigned)i)==pid)window_close((unsigned)i);
    }
    /* Claude, 16.10: drop EVERY geometry request of the exiting process.
       window_close() releases a request only for handle != 0, but an
       application's main window is handle 0 (SYSINFO, COMMANDER...), so each
       run leaked one of the WM_APP_WINDOWS (6) slots.  With the table full,
       kernel_wm_set_window() returned silently and the next application window
       registered as a full-screen background canvas (flags 7): the desktop,
       its top bar and all window frames vanished. */
    {unsigned r;for(r=0;r<WM_APP_WINDOWS;++r)
        if(wm_window_req[r].set&&wm_window_req[r].pid==pid){
            wm_window_req[r].set=0u;wm_window_req[r].title_set=0u;
            wm_window_req[r].content_set=0u;}}
}

/* Close ONE window of a client.  The TOS personality registers its window under
   the pid of the application that launched the .PRG, so closing by pid would
   also destroy the launcher's own dialogs (file window, SETTINGS, ...).  0x0
   handle means "not a specific window". */
void kernel_wm_close_window(uint32_t pid,uint16_t handle)
{
    ++kdbg_wm_close_window;
    int i;
    if(!pid||!wm_ready)return;
    for(i=(int)wm.count-1;i>=0;--i){
        if(wm.win[i].flags&4u)continue;
        if(window_owner_pid((unsigned)i)==pid&&window_owner_handle((unsigned)i)==(uint32_t)handle){
            window_close((unsigned)i);return;}
    }
}

/* Notify the client that the WM committed a move/resize (WM_MOVED pattern). */
static void notify_geometry(unsigned i)
{
    uint32_t pid,handle;extern void kernel_user_notify_window(uint32_t,uint32_t,uint32_t,uint32_t);
    if(i>=wm.count)return;
    pid=window_owner_pid(i);
    if(!pid)return;
    handle=window_owner_handle(i);
    {unsigned slot=wm_window_slot(pid,(uint16_t)handle);
     if(slot<WM_APP_WINDOWS){wm_window_req[slot].x=wm.win[i].frame.x;wm_window_req[slot].y=wm.win[i].frame.y;
         wm_window_req[slot].w=wm.win[i].frame.w;wm_window_req[slot].h=wm.win[i].frame.h;}}
    kernel_user_notify_window(pid,
        ((uint32_t)(uint16_t)wm.win[i].frame.x<<16)|(uint16_t)wm.win[i].frame.y,
        ((uint32_t)wm.win[i].frame.w<<16)|wm.win[i].frame.h,
        handle);
}
/* Zmiana wygladu pulpitu (tlo/pasek/tapeta) zmienia PALETE albo wzor, ale
   powierzchnia pulpitu trzyma stare piksele.  Pulpit jest aplikacja - jadro
   musi go obudzic, inaczej zmiana jest widoczna tylko pod oknami.  Wysylamy
   mu WMOV z niezmieniona geometria: jego handler robi paint(). */
void kernel_wm_desktop_repaint(void)
{
    unsigned i;
    for(i=0;i<wm.count;++i)
        if(wm.win[i].pid&&wm.win[i].frame.w>=(uint16_t)wm.screen_w&&
           wm.win[i].frame.h>=(uint16_t)wm.screen_h){notify_geometry(i);return;}
}

static uint8_t read_pixel(const wm_fb_t *fb,int x,int y)
{
    const uint8_t *row;
    if(x<0||y<0||x>=(int)fb->w||y>=(int)fb->h)return 0;
    row=fb->data+(uint32_t)y*fb->pitch;
    if(fb->bpp==8)return row[x];
    return (uint8_t)((row[x>>3]>>(7-(x&7)))&1u);
}

/* --- TOS window display transform -------------------------------------------
   The SysInfo window is painted by the app into its own surface with the
   correct orientation (verified against the official program), but on the
   user's display it comes out mirrored.  The compositor can mirror the app
   window back per axis, while the kernel console (wm_surface[0]) is left
   untouched.  wm_app_flip: bit 0 = mirror X (about the vertical axis),
   bit 1 = mirror Y.  Runtime-settable so the orientation can be corrected
   without touching the drawing code. */
/* v10: the new render path uses ONE convention ((0,0)=top-left).  The old
   flip/invert workaround is kept for the LEGACY window manager only and is 0;
   the compositor glue below must not branch on it. */
int wm_app_flip = 0;
void kernel_wm_set_app_flip(int v){wm_app_flip=v;}

__attribute__((unused))
static void wm_copy_rect_flip(wm_fb_t *dst,const wm_fb_t *src,const wm_rect_t *d,
                              uint16_t sx,uint16_t sy,int fx,int fy,int inv)
{
    int row,col;
    if(!dst||!src||!d)return;
    for(row=0;row<(int)d->h;++row){
        int dy=d->y+row;
        int sy2=fy?((int)src->h-1-((int)sy+row)):((int)sy+row);
        if(sy2<0||sy2>=(int)src->h)continue;
        for(col=0;col<(int)d->w;++col){
            int dx=d->x+col;
            int sx2=fx?((int)src->w-1-((int)sx+col)):((int)sx+col);
            uint8_t bit,m,*p;
            if(sx2<0||sx2>=(int)src->w)continue;
            if(dx<0||dy<0||dx>=(int)dst->w||dy>=(int)dst->h)continue;
            bit=(uint8_t)((src->data[(uint32_t)sy2*src->pitch+(sx2>>3)]>>(7-(sx2&7)))&1u);
            if(inv)bit^=1u;
            p=dst->data+(uint32_t)dy*dst->pitch+(dx>>3);
            m=(uint8_t)(0x80u>>(dx&7));
            if(bit)*p|=m;else *p&=(uint8_t)~m;
        }
    }
}

/* v10: the compositor destination comes through the video abstraction.
   kgfx_framebuffer() remains the legacy producer; its result is published into
   video_screen_*, and the compositor reads it back from there. */
static uint8_t *wm_get_fb(uint16_t *pitch,uint8_t *bpp)
{
    uint8_t *fb=kgfx_framebuffer(pitch,bpp);
    if(fb){
        video_screen_attach(fb,(uint16_t)wm.screen_w,(uint16_t)wm.screen_h,*pitch,
                            (uint8_t)((*bpp==8u)?VIDEO_FMT_CHUNKY_8BPP:VIDEO_FMT_PLANAR_1BPP));
        {const video_surface_t *s=video_screen_surface();
         if(s&&s->pixels){*pitch=s->pitch;*bpp=(uint8_t)((s->format==VIDEO_FMT_CHUNKY_8BPP)?8u:1u);return s->pixels;}}
    }
    return fb;
}

/* Blit-path counters: how often the compositor used the hardware Blitter vs
   the CPU span copy, and the area threshold that decides it. */
#define WM_BLIT_MIN_PIXELS 960u
uint32_t kblit_calls,kblit_hw,kblit_cpu;

/* The core passes the window pid; the glue maps it to the surface pointer. */
uint32_t kdbg_blit_calls,kdbg_blit_px;
static void glue_blit(void *ctx,uint32_t pid,const wm_rect_t *dst,uint16_t sx,uint16_t sy)
{
    wm_fb_t d;wm_fb_t s;uint32_t i;
    (void)ctx;
    ++kdbg_blit_calls;
    kdbg_blit_px+=(uint32_t)dst->w*(uint32_t)dst->h;
    /* The compositor may be asked to paint into a buffer that is never
       displayed (hidden session); wm_get_fb() reports exactly that case. */
    {uint16_t lp=wm_pitch;uint8_t lb=wm_bpp;uint8_t *lfb=wm_get_fb(&lp,&lb);
     if(!lfb)return;
     d.data=lfb;d.pitch=lp;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=lb;}
    if(!pid){
        /* A full-screen application canvas (the desktop app) paints the whole
           desktop, so the WM dither underneath would only be overwritten.
           Skipping it removes the duplicate work AND the visible flicker. */
        if(wm_background_canvas)return;
        /* Desktop background: a 50% dither.  Written as one byte pattern per
           row (80 bytes) instead of a per-pixel loop: ~8x less work and no
           per-pixel bit fiddling. */
        int y;
        wm_fill_rect(&d,dst,0);
        for(y=dst->y;y<dst->y+(int)dst->h;++y){
            int bx0,bx1,b;uint8_t pat;uint8_t *row;
            if(y<0||y>=d.h)continue;
            bx0=dst->x>>3;bx1=(int)((dst->x+dst->w-1)>>3);
            if(bx0<0)bx0=0;
            if(bx1>(int)((d.w-1)>>3))bx1=(int)((d.w-1)>>3);
            if(bx1<bx0)continue;
            pat=(y&1)?0x55u:0xAAu;
            row=d.data+(uint32_t)y*d.pitch;
            if(d.bpp==8){for(b=bx0;b<=bx1;++b)row[b]=(uint8_t)(pat);}
            else{for(b=bx0;b<=bx1;++b)row[b]=pat;}
        }
        return;}
    for(i=0;i<WM_SURFACE_MAX;++i)
        if(wm_surface[i]&&(uint32_t)(uintptr_t)wm_surface[i]==pid){
            /* Source bpp must follow the LIVE mode (d.bpp), not the cached
               wm_bpp: a stale wm_bpp=1 made the compositor read an 8 bpp
               canvas as mono, collapsing every palette entry to 1 bit
               (colour icons painted white/black). */
            s.data=wm_surface[i];s.pitch=wm_surface_pitch;s.w=wm_surface_w;s.h=wm_surface_h;s.bpp=d.bpp;
            (void)i;
            /* v10: no wm_app_flip on the new path - single top-left convention. */
            /* Use the self-tested blitter for anything but tiny rects (its
               register setup would dominate there); without a blitter
               kblit_copy_planar_to() returns 0 and the fast CPU copy runs. */
            /* The Blitter wins on big rects but its per-operation setup makes
               thin drag strips slower than the CPU span copy.  A window move
               during a drag produces full-width strips only a few rows high, so
               keep the blitter for genuinely large areas. */
            ++kblit_calls;
            if(s.bpp==1u&&d.bpp==1u&&(uint32_t)dst->w*(uint32_t)dst->h>=WM_BLIT_MIN_PIXELS&&
               kblit_copy_planar_to((volatile uint16_t *)(uintptr_t)s.data,(uint16_t)(s.pitch>>1),
                                    (volatile uint16_t *)(uintptr_t)d.data,(uint16_t)(d.pitch>>1),
                                    1u,sx,sy,dst->w,dst->h,(uint16_t)dst->x,(uint16_t)dst->y)){
                ++kblit_hw;return;}
            ++kblit_cpu;
            wm_copy_rect(&d,&s,dst,sx,sy);
            return;}
}

static void console_raise(void);
/* Screen geometry comes from the video layer: the compositor must follow the
   active mode (Shifter 640x400, ET4000 800x600/1024x768) instead of assuming
   ST high.  wm_surface_* describe one compositor surface in the same mode. */
/* Surfaces are allocated for the widest mode the adapter can switch to, so a
   live resolution change only updates the pitch/window geometry - it never has
   to move or reallocate a backing buffer. */
#define WM_SURFACE_MONO_BYTES (98304UL)   /* 1024x768 mono = 128 B * 768 */
static uint32_t wm_surface_bytes(void)
{
    /* 8 bpp needs one byte per pixel: 800x600 = 480000 B, far past the mono
       maximum.  The buffers are reallocated when the depth changes. */
    if(wm_bpp==8u)return (uint32_t)wm_surface_w*(uint32_t)wm_surface_h;
    return WM_SURFACE_MONO_BYTES;
}
/* 1 = allocate the big (chunky) buffers from TT-RAM: 8 of them is 3.8 MB and
   ST-RAM is only 4 MB.  The blitter is off on an 030 anyway, so placement does
   not matter for hardware acceleration. */
static int wm_surface_prefer_tt(void){return wm_surface_bytes()>WM_SURFACE_MONO_BYTES;}
void kernel_wm_init(void)
{
    const kgfx_info_t *g=kgfx_get_info();
    int16_t w=(g&&g->width)?(int16_t)g->width:640;
    int16_t h=(g&&g->height)?(int16_t)g->height:400;
    wm_ready=0;wm_init(&wm,w,h);
}

/* Production bring-up for the opt-in compositor: no test rectangles, no pixel
   assertions.  The caller (kgfx_enter_desktop under CONFIG_WM) has already
   switched graphics.framebuffer to the live screen. */
int kernel_wm_enable(void)
{
    if(wm_ready)return 1;
    kernel_wm_init();
    wm_framebuffer=wm_get_fb(&wm_pitch,&wm_bpp);
    if(!wm_framebuffer||(wm_bpp!=1u&&wm_bpp!=8u))return 0;
    /* Every compositor surface is one full-screen bitmap in the ACTIVE mode.
       Only the console-registration path used to set this, which left
       application windows with pitch 0 in desktop mode (they composed as
       nothing). */
    {const kgfx_info_t *g=kgfx_get_info();
     wm_surface_w=(uint16_t)(g&&g->width?g->width:640u);
     wm_surface_h=(uint16_t)(g&&g->height?g->height:400u);
     wm_surface_pitch=(uint16_t)(g&&g->line_bytes?g->line_bytes:
                                 (uint16_t)((wm_surface_w*wm_bpp+7u)/8u));}
    wm_ready=1;
    kernel_wm_damage_all();
    return 1;
}

int kernel_wm_self_test(void)
{
    static uint8_t surf_a[64*8],surf_b[64*8];
    wm_fb_t d;unsigned i;uint8_t a_expect,b_expect;
    kernel_wm_init();
    wm_framebuffer=wm_get_fb(&wm_pitch,&wm_bpp);
    if(!wm_framebuffer||(wm_bpp!=1u&&wm_bpp!=8u))return 0;
    if(wm_bpp==1u){wm_surface_pitch=8;wm_surface_w=64;wm_surface_h=64;
        for(i=0;i<sizeof surf_a;++i){surf_a[i]=0xffu;surf_b[i]=0x00u;}}
    else{wm_surface_pitch=64;wm_surface_w=64;wm_surface_h=64;
        for(i=0;i<sizeof surf_a;++i){surf_a[i]=0x11u;surf_b[i]=0x22u;}}
    wm_surface[0]=surf_a;wm_surface[1]=surf_b;
    wm.win[0].frame.x=0;wm.win[0].frame.y=0;wm.win[0].frame.w=32;wm.win[0].frame.h=32;
    wm.win[0].client.x=0;wm.win[0].client.y=0;wm.win[0].client.w=32;wm.win[0].client.h=32;
    wm.win[0].pid=(uint32_t)(uintptr_t)surf_a;wm.win[0].flags=1;
    wm.win[1].frame.x=16;wm.win[1].frame.y=16;wm.win[1].frame.w=32;wm.win[1].frame.h=32;
    wm.win[1].client.x=16;wm.win[1].client.y=16;wm.win[1].client.w=32;wm.win[1].client.h=32;
    wm.win[1].pid=(uint32_t)(uintptr_t)surf_b;wm.win[1].flags=1;
    wm.count=2;
    wm_rebuild_clip(&wm);
    wm_damage_clear(&wm);
    wm_damage_add(&wm,(wm_rect_t){0,0,64,64});
    wm_ready=1;
    wm_recompose(&wm,glue_blit,0);
    d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
    a_expect=(uint8_t)(wm_bpp==1u?1u:0x11u);
    b_expect=(uint8_t)(wm_bpp==1u?0u:0x22u);
    if(read_pixel(&d,8,8)!=a_expect)return 0;      /* only A there */
    if(read_pixel(&d,24,24)!=b_expect)return 0;    /* B wins on top */
    if(read_pixel(&d,51,50)!=0u)return 0;          /* background (odd cell of the dither) */
    /* M5: drag with content — move B, pixels follow, old area is restored,
       and the owning app was never asked to redraw. */
    wm_move(&wm,1,32,32);
    wm_recompose(&wm,glue_blit,0);
    if(read_pixel(&d,40,40)!=b_expect)return 0;    /* content moved with the window */
    if(read_pixel(&d,24,24)!=a_expect)return 0;    /* old spot shows A underneath */
    if(read_pixel(&d,41,20)!=0u)return 0;          /* old spot outside A: background dither */
    wm_damage_clear(&wm);
    return 1;
}

static uint32_t wm_last_frame;
static uint32_t wm_frames,wm_damage_pixels,wm_console_cells;
/* Requests-per-frame: sampled once per composed frame from the broker counter,
   so the budget from ETAP 1 ("requests per frame") becomes a real number. */
static uint32_t wm_requests_seen,wm_requests_last,wm_requests_max,wm_requests_total;
void kernel_wm_stats(uint32_t *frames,uint32_t *pixels,uint32_t *cells)
{
    if(frames)*frames=wm_frames;
    if(pixels)*pixels=wm_damage_pixels;
    if(cells)*cells=wm_console_cells;
}
void kernel_wm_request_stats(uint32_t *total,uint32_t *last,uint32_t *max)
{
    if(total)*total=wm_requests_total;
    if(last)*last=wm_requests_last;
    if(max)*max=wm_requests_max;
}
static uint8_t console_registered;
/* Outline ("rubber band") window drag: the client is NOT moved while dragging,
   only a dashed frame follows the pointer; the window jumps once on release. */
static unsigned drag_win;
/* Win98 buttons: a gadget shows sunken while held; the action fires on release. */
static unsigned gpress_win;
static uint8_t gpress_part;
/* Outline resize: drag a border; a dashed frame follows, wm_resize on release. */
/* Diagnostics for the scripted window tests (read via m68k-elf-nm). */
uint32_t wm_dbg_press,wm_dbg_drag_start,wm_dbg_drag_move,wm_dbg_drag_end,wm_dbg_rsz_start;
uint32_t wm_dbg_dx,wm_dbg_dy,wm_dbg_nx,wm_dbg_ny,wm_dbg_mx,wm_dbg_my,wm_dbg_mvx,wm_dbg_mvy,wm_dbg_mvwin;
uint32_t wm_dbg_pmx,wm_dbg_pmy,wm_dbg_phit,wm_dbg_press_frame;
uint32_t kdbg_wm_calls,kdbg_wm_composes,kdbg_outline_draws,kdbg_deco_draws;
/* Test hook (GFX opcode 28): drive a synthetic fast drag from the kernel so the
   compositor's outline tracking can be measured without app IPC latency. */
uint32_t kdbg_autodrag;
/* Hires-cycle accumulators (12288 counts/s) for the compose sections. */
uint32_t kdbg_recompose_cycles,kdbg_deco_cycles,kdbg_cursor_cycles,kdbg_total_cycles,kdbg_outline_cycles;

uint32_t kdbg_loop_cycles,kdbg_loop_calls;
extern uint32_t kdbg_frames;
int32_t wm_dbg_rszw,wm_dbg_rszh;
static unsigned rsz_win;
static int32_t rsz_gx,rsz_gy,rsz_x0,rsz_y0,rsz_w0,rsz_h0,rsz_nw,rsz_nh;
static uint8_t rsz_active,rsz_edge,rsz_drawn_valid;
static wm_rect_t rsz_drawn;
/* Interactive WINDOW MOVE: same model as the EmuTOS desktop (graf_dragbox):
   only a dashed outline follows the pointer and the window is placed once on
   release.  Redrawing the window content every step is wrong in a compositor:
   the compositor repaints only the damaged rects, so the overlap of the old
   and the new frame keeps stale pixels and the content visibly smears. */
static uint8_t move_drawn_valid;
static wm_rect_t move_drawn;
/* Outline presentation rate limit (~20 ms at 12288 hires counts/s).  Input can
   arrive faster than the card can show, and every intermediate position used to
   cost an XOR plus its own kvideo_flush() before the main composition was even
   rate limited.  The newest position is what gets drawn; the release always
   commits from its own coordinates, so no button edge is delayed or merged. */
#define WM_OUTLINE_PERIOD 246u
static uint32_t outline_shown_tick;
static int32_t drag_dx,drag_dy,drag_nx,drag_ny;
static uint16_t drag_w,drag_h;
static uint8_t drag_active,wm_force_compose;
/* Przeciaganie z ZAWARTOSCIA (jak MagiC "online mover", look_flags bit 6):
   1 = okno jedzie z trescia, 0 = klasyczna gumka (XOR obrysu).
   KOSZT ZMIERZONY (800x600x8, okno 420x300, Hatari): krok z zawartoscia to
   DWA kompozyty (stara i nowa ramka) ~0.2-0.4 s czasu maszyny, a krok gumki
   ~2 ms (sam obrys).  Dlatego zawartosc jest domyslna tylko dla MALYCH okien
   (WM_LIVE_MAX_PIXELS = okna do ~420x300), a prawy przycisk zawsze wymusza gumke - dokladnie
   jak w MagiC, gdzie prawy/Ctrl wylacza tryb online.  Chcesz zawartosc
   wszedzie: podnies WM_LIVE_MAX_PIXELS. */
#define WM_LIVE_MAX_PIXELS 130000u
static uint8_t drag_live,drag_step_valid;
/* Desktop background painted once per mode entry, not per frame. */
static uint8_t wm_bg_done;

/* Vertical scrollbar (GEM W_VBAR/W_VSLIDE/W_VELEV): 16px column at the right
   edge of the client.  Scrolling shifts src_y into the window surface, so the
   client never redraws. */
#define WM_SB_W 16u
static int sb_drag_win=-1;
static int32_t sb_drag_grab;
static int32_t sb_range(unsigned w);
static int sb_visible(unsigned w);
static int32_t cxs_of(unsigned w);
static int32_t sb_thumb_size(unsigned w);
static int32_t sb_h(unsigned w);
/* Prostokat SUWAKA dla danego src_y (wspolny dla rysowania i uszkadzania). */
static void sb_thumb_rect(unsigned w,int32_t src_y,wm_rect_t *r)
{
    int32_t cys=(int32_t)wm.win[w].client.y,chs=sb_h(w);
    int32_t range=sb_range(w),track=chs-2*(int32_t)WM_SB_W;
    int32_t ths=sb_thumb_size(w),thp;
    thp=track?(range?(track*src_y/range):0):0;
    if(thp>track-ths)thp=track-ths;
    if(thp<0)thp=0;
    r->x=(int16_t)(cxs_of(w)+1);r->y=(int16_t)(cys+(int32_t)WM_SB_W+thp);
    r->w=(uint16_t)(WM_SB_W-2);r->h=(uint16_t)ths;
}
/* Przewiniecie zmienia polozenie TYLKO suwaka - MagiC odrysowuje sam obiekt
   slidera, my uszkadzamy stary i nowy prostokat suwaka zamiast calej kolumny
   16 x wysokosc okna (3-8x mniej pikseli na krok przewijania). */
static void sb_damage_from(unsigned w,int32_t old_src)
{
    wm_rect_t a,b;
    if(w>=wm.count||!sb_visible(w))return;
    if(old_src==(int32_t)wm.win[w].src_y){   /* offset sie nie zmienil */
        sb_thumb_rect(w,old_src,&a);
        wm_damage_add(&wm,a);
        return;
    }
    sb_thumb_rect(w,old_src,&a);
    sb_thumb_rect(w,(int32_t)wm.win[w].src_y,&b);
    wm_damage_add(&wm,a);
    wm_damage_add(&wm,b);
    /* Przewiniecie zmienia PIKSELE KLIENTA (blit z powierzchni od src_y), nie
       tylko suwak - bez uszkodzenia klienta lista zostawalaby na ekranie
       nieruchoma, mimo ze suwak juz sie przesunal. */
    wm_damage_add(&wm,wm.win[w].client);
}
static int32_t sb_range(unsigned w)
{
    int32_t r=(int32_t)wm.win[w].content_h-(int32_t)wm.win[w].client.h;
    return r>0?r:0;
}
static int sb_visible(unsigned w)
{
    return w<wm.count&&!(wm.win[w].flags&2u)&&(int32_t)wm.win[w].content_h>(int32_t)wm.win[w].client.h;
}
/* Height of the scrollbar column.  The client reaches the bottom of the frame,
   so a full-height bar put its down arrow exactly on the GEM sizer (the 16x16
   bottom-right corner): the sizer was painted over and a click there scrolled
   instead of resizing.  Like GEM (W_VBAR ends above W_SIZER), stop the bar
   above the sizer whenever the two overlap. */
static int32_t sb_h(unsigned w)
{
    int32_t h=(int32_t)wm.win[w].client.h;
    int32_t cb=(int32_t)wm.win[w].client.y+h;
    int32_t sz=(int32_t)wm.win[w].frame.y+(int32_t)wm.win[w].frame.h-16;
    int32_t cr=(int32_t)wm.win[w].client.x+(int32_t)wm.win[w].client.w;
    int32_t fr=(int32_t)wm.win[w].frame.x+(int32_t)wm.win[w].frame.w;
    if(cr>fr-16&&cb>sz)h-=cb-sz;
    return h>0?h:0;
}
static int32_t cxs_of(unsigned w)
{
    return (int32_t)wm.win[w].client.x+(int32_t)wm.win[w].client.w-(int32_t)WM_SB_W;
}
static int32_t sb_thumb_size(unsigned w)
{
    int32_t track=sb_h(w)-2*(int32_t)WM_SB_W;
    int32_t t=track*(int32_t)wm.win[w].client.h/(int32_t)wm.win[w].content_h;
    if(t<(int32_t)WM_SB_W)t=WM_SB_W;
    if(t>track)t=track;
    return t;
}
int kernel_wm_chrome_at(int32_t x,int32_t y)
{
    unsigned hit;
    if(!wm_ready||x<0||y<0||x>=wm.screen_w||y>=wm.screen_h)return 0;
    if(task_chrome_at(x,y))return 1;
    hit=wm_hit(&wm,(int16_t)x,(int16_t)y);
    if(hit==WM_NO_WINDOW)return 0;
    if(wm.win[hit].flags&2u)return 0; /* client draws its own decorations */
    if(wm_hit_part(&wm,hit,(int16_t)x,(int16_t)y)!=WM_PART_CLIENT)return 1;
    /* The scrollbar overlays the client surface, but belongs to the WM too. */
    return sb_visible(hit)&&sb_h(hit)>2*(int32_t)WM_SB_W&&
           x>=cxs_of(hit)&&x<cxs_of(hit)+(int32_t)WM_SB_W&&
           y<(int32_t)wm.win[hit].client.y+sb_h(hit);
}
/* Handle a click in window w's scrollbar.  Returns 1 when it was consumed. */
static int sb_click(unsigned w,int32_t mx,int32_t my)
{
    int32_t cxs,cys,chs,page,range,old_src;
    if(w>=wm.count||(wm.win[w].flags&2u))return 0;
    if(!sb_visible(w))return 0;
    old_src=(int32_t)wm.win[w].src_y;
    cxs=cxs_of(w);
    if(mx<cxs||mx>=cxs+(int32_t)WM_SB_W)return 0;
    cys=(int32_t)wm.win[w].client.y;chs=sb_h(w);page=(int32_t)wm.win[w].client.h;
    if(chs<=2*(int32_t)WM_SB_W||my<cys||my>=cys+chs)return 0;
    range=sb_range(w);
    if(my<cys+(int32_t)WM_SB_W){
        int32_t v=(int32_t)wm.win[w].src_y-16;wm.win[w].src_y=(int16_t)(v>0?v:0);
    }else if(my>=cys+chs-(int32_t)WM_SB_W){
        int32_t v=(int32_t)wm.win[w].src_y+16;wm.win[w].src_y=(int16_t)(v<range?v:range);
    }else{
        int32_t track=chs-2*(int32_t)WM_SB_W;
        int32_t ths=sb_thumb_size(w);
        int32_t thp=range?(track*(int32_t)wm.win[w].src_y/range):0;
        if(my<cys+(int32_t)WM_SB_W+thp){int32_t v=(int32_t)wm.win[w].src_y-page;wm.win[w].src_y=(int16_t)(v>0?v:0);}
        else if(my>=cys+(int32_t)WM_SB_W+thp+ths){int32_t v=(int32_t)wm.win[w].src_y+page;wm.win[w].src_y=(int16_t)(v<range?v:range);}
        else{sb_drag_win=(int)w;sb_drag_grab=my-(cys+(int32_t)WM_SB_W+thp);return 1;}
    }
    sb_damage_from(w,old_src);
    return 1;
}
/* Damage only the 1px border of an outline: four thin strips instead of the
   whole window-sized rectangle, so an outline drag re-composites about
   2*(w+h) pixels per tick rather than w*h. */
/* Both the move and the resize outline are XOR-ed straight into the frame
   buffer by the mouse handler (xor_outline/outline_video_damage below), so the
   compositor neither draws nor damages them: a resize step costs ~2*(w+h)
   pixels instead of a window-sized recompose. */
extern int kernel_user_mouse_event(uint32_t *buttons,int32_t *x,int32_t *y);
void kernel_wm_frame(void)
{
    if(!wm_ready||!wm.damage_count)return;
    wm_recompose(&wm,glue_blit,0);
}
/* Compose the WM immediately, regardless of the desktop-session gate, so the
   TOS window (whose composite runs on a trap, not the main loop) reaches the
   framebuffer.  This must run the FULL frame path - wm_recompose alone paints no
   title bars, so the window would have no close/minimise/zoom gadgets. */
void kernel_wm_compose_now(void)
{
    extern uint32_t kernel_user_clock(void);
    if(!wm_ready)return;
    /* kvideo_flush() copies kvideo_front(); the console path can have switched
       it back to the card VRAM, in which case the flush is a no-op. */
    kvideo_select(wm_framebuffer);
    wm_compose_forced=1u;
    kernel_wm_frame_tick(kernel_user_clock());
    wm_compose_forced=0u;
}

/* GUI_FRAME: tylko zamow kompozycje na najblizszy tick.  Wczesniej kazda
   klatka aplikacji wymuszala pelny tick WM (dock, kursor, flush), wiec kilka
   widocznych animacji mnozylo kompozycje. */
void kernel_wm_compose_request(void)
{
    if(wm_ready)wm_force_compose=1u;
}
static uint8_t console_hidden;   /* desktop mode: shell window not shown */
static uint8_t console_focused=1;/* keyboard focus: shell window vs desktop */
/* Desktop mouse pointer: drawn last, after every composite, so it is never
   overwritten by window content. */
static int32_t wm_mouse_x,wm_mouse_y;
static uint8_t wm_mouse_valid;
static void cursor_sync(void)
{
    /* The compositor owns the pointer.  Always place it at the session's
       absolute position so a composite that hid it restores it; the GUI wake
       path moves it too, and both must use the same coordinates. */
    wm_mouse_x=ksessions_mouse_x();wm_mouse_y=ksessions_mouse_y();
    wm_mouse_valid=1;
    if(kgfx_cursor_visible())kgfx_cursor_move(wm_mouse_x,wm_mouse_y);
    else kgfx_cursor_show(wm_mouse_x,wm_mouse_y);
}
static uint8_t wm_desktop_session=0xFFu;  /* session number that is a pure desktop (0xFF = none) */
/* ---- top-bar task manager: icon + window list ---------------------------- */
#define TASK_ICON_W 16
#define TASK_ICON_H 14
#define TASK_LIST_W 200
#define TASK_ROW_H 16
static uint8_t task_list_open;
static wm_rect_t task_list_rect;
/* Last drawn list rectangle + whether it is on screen, so closing the list can
   hand that area back to the compositor instead of leaving it stale. */
static wm_rect_t task_list_last;
static uint8_t task_list_drawn;
/* Set when the list closes: the strip must be redrawn once, even though the
   list rectangle lies below it, because the restoring composition is what
   erased the badges/icon. */
static uint8_t task_strip_repaint;
/* Diagnostics for the "top bar erased after closing the task list" report. */
uint32_t kdbg_task_draws,kdbg_task_closes,kdbg_task_opens;
static unsigned task_rows[WM_MAX],task_row_count;
/* The desktop background canvas is not a task: it must not be duplicated in
   the list (there is nothing to switch to). */
static void task_build_rows(void)
{
    unsigned i;task_row_count=0;
    for(i=0;i<wm.count&&task_row_count<WM_MAX;++i){
        if((wm.win[i].flags&4u)||!(wm.win[i].flags&8u)||wm.win[i].session!=ksessions_current())continue;
        task_rows[task_row_count++]=i;
    }
}

void kernel_wm_set_title(uint32_t pid,uint16_t handle,const char *title)
{
    unsigned i;
    if(!title)return;
    {unsigned s=wm_window_slot(pid,handle);
     if(s<WM_APP_WINDOWS){unsigned j;for(j=0;j<23u;++j){wm_window_req[s].title[j]=title[j];if(!title[j])break;}
              wm_window_req[s].title[23]=0;wm_window_req[s].title_set=1u;}}
    {int sl=app_surface_find(pid,handle);uint8_t *surf=sl<0?(uint8_t *)0:app_surface[sl];
     for(i=0;i<wm.count;++i)if(surf&&wm.win[i].pid==(uint32_t)(uintptr_t)surf){
        unsigned j;for(j=0;j<23u;++j){wm.win[i].title[j]=title[j];if(!title[j])break;}
        wm.win[i].title[23]=0;
        /* Repaint just the title band; the task strip is refreshed separately. */
        {wm_rect_t tb;tb.x=wm.win[i].frame.x;tb.y=wm.win[i].frame.y;
         tb.w=wm.win[i].frame.w;tb.h=16;wm_damage_add(&wm,tb);}
        task_draw();     /* refresh only the top-bar task strip, not the screen */
        return;}}
}
static void task_icon_rect(wm_rect_t *r)
{r->x=(int16_t)(wm.screen_w-66-TASK_ICON_W-2);r->y=1;r->w=TASK_ICON_W;r->h=TASK_ICON_H;}
static void task_list_geometry(void)
{
    task_list_rect.x=(int16_t)(wm.screen_w-TASK_LIST_W-2);
    task_list_rect.y=(int16_t)(TASK_ICON_H+2);
    task_list_rect.w=TASK_LIST_W;
    task_list_rect.h=(uint16_t)((task_row_count?task_row_count:1u)*TASK_ROW_H+2u);
}
/* The popup owns the whole dismissal gesture, including outside clicks. */
static int task_chrome_at(int32_t x,int32_t y)
{
    wm_rect_t r;task_icon_rect(&r);
    return task_list_open||(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h);
}
/* Frame a window was last created or re-declared.  A freshly opened window must
   not be closed by the same pointer gesture that opened it: some hosts (Mac
   mouse behind Hatari's --grab/--mousewarp) report a stray button edge while the
   pointer travels onto the new window, which used to hit the X gadget and make
   the window vanish the instant the user moved the mouse over it. */
extern volatile uint32_t kernel_timer_ticks;
static uint32_t wm_open_tick;
extern uint32_t kdbg_frames;
/* Defined below; needed by the minimise/focus paths. */
uint32_t kernel_gui_focus_pid(void);
void kernel_gui_focus_set(uint32_t pid);

static int console_fit(unsigned i);
/* Pelny redraw bez limitu listy damage: przy drag/resize lista bywa pelna,
   a wtedy dodanie calego ekranu bylo po cichu odrzucane - smieci zostawaly
   do nastepnego, przypadkowego odswiezenia (1-2 s). */
static void wm_redraw_rect(wm_rect_t r)
{
    if(!wm_ready)return;
    wm_damage_clear(&wm);
    wm_damage_add(&wm,r);
    wm_force_compose=1;
}
static void wm_redraw_old_new(wm_rect_t oldr,wm_rect_t newr)
{
    if(!wm_ready)return;
    wm_damage_clear(&wm);
    wm_damage_add(&wm,oldr);
    if(newr.x!=oldr.x||newr.y!=oldr.y||newr.w!=oldr.w||newr.h!=oldr.h)wm_damage_add(&wm,newr);
    wm_force_compose=1;
}
static void window_minimize(unsigned i)
{
    if(i>=wm.count)return;
    if(wm.win[i].flags&(4u|8u))return;         /* never minimize the background */
    wm.win[i].flags=(uint8_t)((wm.win[i].flags|8u)&(uint8_t)~1u);
    wm_rebuild_clip(&wm);
    /* Sama ramka nie zawsze pokrywa chrom okien pod nia; wyczysc liste
       damage (bywa pelna po drag) i odswiez ramke. */
    wm_redraw_rect(wm.win[i].frame);
    /* A hidden window must not keep the keyboard: keys would go nowhere. */
    if(window_owner_pid(i)==kernel_gui_focus_pid())kernel_gui_focus_set(0u);
    wm_force_compose=1;task_strip_repaint=1u;
}
static void window_restore(unsigned i)
{
    if(i>=wm.count||!(wm.win[i].flags&8u))return;
    wm.win[i].flags=(uint8_t)((wm.win[i].flags&(uint8_t)~8u)|1u);
    wm_rebuild_clip(&wm);
    wm_redraw_rect(wm.win[i].frame);
    kernel_gui_focus_set(window_owner_pid(i));
    {extern void kernel_user_notify_focus(uint32_t);kernel_user_notify_focus(window_owner_pid(i));}
    wm_raise(&wm,i);
    wm_force_compose=1;task_strip_repaint=1u;
}
/* Maximize below the desktop bar. Minimize keeps both the current frame
   and the pre-maximize restore rectangle, including across Z-order changes. */
static void window_zoom(unsigned i)
{
    if(i>=wm.count)return;
    if(wm.win[i].flags&4u)return;              /* never the desktop background */
    if(wm.win[i].flags&8u)return;              /* minimized: restore it first */
    uint16_t old_h=wm.win[i].client.h;
    wm_damage_add(&wm,wm.win[i].frame);
    if(wm.win[i].flags&16u){
        wm.win[i].flags=(uint8_t)(wm.win[i].flags&(uint8_t)~16u);
        wm.win[i].frame=wm.win[i].restore;
    }else{
        wm.win[i].restore=wm.win[i].frame;
        wm.win[i].frame.x=0;wm.win[i].frame.y=16;
        wm.win[i].frame.w=(uint16_t)wm.screen_w;
        wm.win[i].frame.h=(uint16_t)(wm.screen_h-16);
        wm.win[i].flags=(uint8_t)(wm.win[i].flags|16u);
    }
    wm.win[i].client.x=wm.win[i].frame.x;
    wm.win[i].client.y=(int16_t)(wm.win[i].frame.y+16);
    wm.win[i].client.w=wm.win[i].frame.w;
    wm.win[i].client.h=(uint16_t)(wm.win[i].frame.h>16u?wm.win[i].frame.h-16u:0u);
    if(console_fit(i)){
        /* the shell window: geometry fitted to its 80x25 grid */
    }else{
    if(wm.win[i].content_h<=old_h)wm.win[i].content_h=wm.win[i].client.h;
    {int32_t range=(int32_t)wm.win[i].content_h-wm.win[i].client.h;
     if(range<0)range=0;
     if(wm.win[i].src_y>range)wm.win[i].src_y=(int16_t)range;}
    }
    notify_geometry(i);
    kernel_gui_focus_set(window_owner_pid(i));
    {extern void kernel_user_notify_focus(uint32_t);kernel_user_notify_focus(window_owner_pid(i));}
    wm_force_compose=1;
    wm_damage_add(&wm,wm.win[i].frame);
    wm_rebuild_clip(&wm);
    wm_raise(&wm,i);
}
/* Queue geometry work outside the input parser. Ctrl+Alt+Enter toggles
   the focused decorated window; plain Escape remains application input. */
static uint32_t zoom_pending_surface;
static uint8_t zoom_key_down;
int kernel_wm_key(uint32_t type,uint32_t scan,uint32_t mods)
{
    int i;
    /* Ctrl+Alt+Esc = kill-switch programu TOS.  Klawisze okna NIE przechodza
       przez kernel_user_keyboard_event, wiec obsluga musi byc takze tutaj -
       inaczej kombinacja trafiala do programu jako ESC (SYSINFO: "Exit
       SysInfo ?") i nie dalo sie wrocic do pulpitu bez myszy. */
    {extern uint32_t kdbg_ksw_wm_esc,kdbg_ksw_wm_mods;
     if(type==1u&&scan==0x01u){++kdbg_ksw_wm_esc;kdbg_ksw_wm_mods=mods;}}
#if CONFIG_TOS
    if(type==1u&&scan==0x01u&&(mods&12u)==12u){
        extern int kernel_tos_running(void);
        extern void kernel_tos_request_exit(void);
        if(kernel_tos_running()){{extern uint32_t kdbg_ksw_wm_chord; ++kdbg_ksw_wm_chord;}
                                  kernel_tos_request_exit();return 1;}
    }
#endif /* CONFIG_TOS: kill-switch istnieje tylko z warstwa TOS */
    if(scan!=0x1cu)return 0;
    if(zoom_key_down){if(type!=1u)zoom_key_down=0;return 1;}
    if(type!=1u||(mods&15u)!=12u||!wm_ready||kgame_owns_screen())return 0;
    for(i=(int)wm.count-1;i>=0;--i){
        if(wm.win[i].session!=ksessions_current()||!(wm.win[i].flags&1u)||(wm.win[i].flags&14u))continue;
        if(window_owner_pid((unsigned)i)!=kernel_gui_focus_pid())continue;
        zoom_pending_surface=wm.win[i].pid;zoom_key_down=1;return 1;
    }
    return 0;
}
static void window_pending_zoom(void)
{
    unsigned i;uint32_t surface=zoom_pending_surface;zoom_pending_surface=0;
    if(!surface)return;
    for(i=0;i<wm.count;++i)if(wm.win[i].pid==surface&&wm.win[i].session==ksessions_current()){
        window_zoom(i);return;
    }
}
/* Freestanding kernel: a struct assignment would pull in memcpy().  Copy
   through volatile bytes so the compiler keeps the loop. */
static void wm_window_copy(wm_window_t *d,const wm_window_t *s)
{
    volatile uint8_t *dp=(volatile uint8_t *)d;
    const volatile uint8_t *sp=(const volatile uint8_t *)s;
    unsigned k;for(k=0;k<sizeof(wm_window_t);++k)dp[k]=sp[k];
}
static void window_close(unsigned i)
{
    wm_rect_t r;unsigned j;uint32_t owner,handle=0u;
    if(i>=wm.count)return;
    if(wm.win[i].flags&4u)return;         /* never close the desktop background */
    r=wm.win[i].frame;
    owner=window_owner_pid(i);            /* before the surface mapping goes away */
    {extern uint32_t kernel_gui_focus_pid(void);extern void kernel_gui_focus_set(uint32_t);
     if(owner&&kernel_gui_focus_pid()==owner)kernel_gui_focus_set(0u);}
    for(j=0;j<WM_APP_WINDOWS;++j)if(app_surface_used[j]&&
        (uint32_t)(uintptr_t)app_surface[j]==wm.win[i].pid){
        handle=app_surface_handle[j];
        app_surface_used[j]=0;app_surface_pid[j]=0;app_surface_handle[j]=0;wm_surface[1u+j]=0;}
    /* A TOS program's window is owned by the client that launched it, but the
       program itself keeps running and keeps blitting into THIS surface, which
       window_close() is about to release.  Closing the window must therefore
       also terminate the program: otherwise the user is left with a window-less
       running program (the "I cannot close TOSCOL" report) and the next blit
       lands in a surface the compositor no longer maps. */
#if CONFIG_TOS
    if(handle){extern int kernel_tos_window_is(uint32_t);extern void kernel_tos_window_closed(void);
        if(kernel_tos_window_is(handle))kernel_tos_window_closed();}
#endif
    /* Release the geometry request as well.  wm_window_req[] has only
       WM_APP_WINDOWS slots (6) and a leaked slot makes the next window of the
       same client fall back to a full-screen background canvas - the TOS window
       turned into a black full-screen surface after a few runs. */
    if(owner&&handle){unsigned r;for(r=0;r<WM_APP_WINDOWS;++r)
        if(wm_window_req[r].set&&wm_window_req[r].pid==owner&&
           wm_window_req[r].handle==handle){wm_window_req[r].set=0u;break;}}
    for(j=i;j+1u<wm.count;++j)wm_window_copy(&wm.win[j],&wm.win[j+1u]);
    --wm.count;
    wm_rebuild_clip(&wm);
    /* Po zamknieciu okna nad innymi oknami sam prostokat bywa za maly;
       wyczysc liste i odswiez stara ramke (bez pelnego ekranu - ten
       zbyt dlugo trzymal smieci na 1024x768). */
    wm_redraw_rect(r);
    /* Tell the owner that this window is gone: the desktop drops its cached
       file-window state instead of painting into a window that no longer
       exists (and reopens it on the next icon click). */
    if(owner){extern void kernel_user_notify_window_close(uint32_t,uint32_t);
        kernel_user_notify_window_close(owner,handle);}
}
static unsigned wm_cycle_pos;
/* Keyboard task switch: bring the next window to the front (restoring it if
   minimized).  The background canvas is skipped. */
void kernel_wm_cycle(void)
{
    unsigned i,n=wm.count;
    if(n<2u)return;
    for(i=1u;i<=n;++i){
        unsigned idx=(wm_cycle_pos+i)%n;
        if(wm.win[idx].flags&4u)continue;
        wm_cycle_pos=idx;
        if(wm.win[idx].flags&8u)window_restore(idx);
        else{wm_damage_add(&wm,wm.win[idx].frame);wm_raise(&wm,idx);}
        return;
    }
}
/* Switch the view before any hit test or composition uses cached clips. */
static void wm_sync_view(void)
{
    uint8_t next=(uint8_t)ksessions_current();
    if(!wm_ready||wm.view_session==next)return;
    wm.view_session=next;
    wm_rebuild_clip(&wm);
    wm_damage_add(&wm,(wm_rect_t){0,0,(uint16_t)wm.screen_w,(uint16_t)wm.screen_h});
    wm_force_compose=1;
}
void kernel_wm_refresh_bar(void){wm_sync_view();task_draw();}
/* Draw the icon (and the open list) into the live framebuffer. */
/* --- Desktop (session) switcher -------------------------------------------
   A two-monitor glyph the size of a font cell sits next to the task icon.  With
   one desktop it stands alone; with two or more the current desktop's number
   follows it.  Clicking it drops a list of the desktops plus "Add Desk" — the
   graphics twin of the textual tts menu, which stays for the text sessions. */
#define DESK_ICON_W 22   /* two monitors, as large as the 16px bar allows */
#define DESK_ICON_H 15
#define DESK_NUM_W   9
#define DESK_LIST_W 92
#define DESK_ROW_H  16
static uint8_t desk_list_open,desk_list_drawn;
static wm_rect_t desk_list_rect,desk_list_last;
static uint8_t desk_rows[SESSION_COUNT+1];
static unsigned desk_row_count;
uint32_t kdbg_desk_menu,kdbg_desk_list_draws;

static unsigned desk_count(void)
{
    unsigned i,n=0,m=ksessions_mask();
    for(i=0;i<SESSION_COUNT;++i)if(m&(1u<<i))++n;
    return n;
}
static void desk_build_rows(void)
{
    unsigned i,m=ksessions_mask();desk_row_count=0;
    for(i=0;i<SESSION_COUNT;++i)if(m&(1u<<i))desk_rows[desk_row_count++]=(uint8_t)i;
    if(desk_row_count<SESSION_COUNT)desk_rows[desk_row_count++]=0xFFu;   /* Add Desk */
}
static void desk_icon_rect(wm_rect_t *r)
{
    unsigned extra=(desk_count()>1u)?DESK_NUM_W:0u;
    r->w=(uint16_t)(DESK_ICON_W+extra);
    r->h=DESK_ICON_H;
    r->x=(int16_t)(wm.screen_w-66-TASK_ICON_W-2-4-(int)r->w);
    r->y=1;
}
/* The bar and its drop-downs live in the framebuffer, outside the window-chrome
   clip deco_pixel enforces, so use the kernel text writer the task list uses. */
/* opaque=1 paints the glyph background as well; the desktop NUMBER must be see
   through (a white block on the steel bar), while drop-down rows need their
   background to match the row highlight. */
static void desk_text(int16_t x,int16_t y,const char *t,uint8_t ink,uint8_t paper,int opaque)
{
    kgfx_text((int32_t)x,(int32_t)y,t,ink,paper,opaque);
}
static void desk_list_geometry(void)
{
    desk_build_rows();
    desk_list_rect.w=DESK_LIST_W;
    desk_list_rect.h=(uint16_t)(desk_row_count*DESK_ROW_H+2u);
    desk_list_rect.x=(int16_t)(wm.screen_w-DESK_LIST_W-2);
    desk_list_rect.y=(int16_t)(DESK_ICON_H+4);
}
/* Consume a click on the icon or the open list.  1 = handled. */
static int desk_click(int16_t mx,int16_t my)
{
    wm_rect_t r;desk_icon_rect(&r);
    if(mx>=r.x&&my>=r.y&&mx<(int16_t)(r.x+r.w)&&my<(int16_t)(r.y+r.h)){
        desk_list_open=(uint8_t)!desk_list_open;
        ++kdbg_desk_menu;                    /* diagnostics: clicks that reached the icon */
        if(desk_list_open)task_list_open=0;   /* one drop-down at a time */
        task_draw();
        return 1;
    }
    if(desk_list_open){
        desk_list_geometry();
        if(mx>=desk_list_rect.x&&my>=desk_list_rect.y&&
           mx<(int16_t)(desk_list_rect.x+desk_list_rect.w)&&
           my<(int16_t)(desk_list_rect.y+desk_list_rect.h)){
            unsigned row=(unsigned)((my-desk_list_rect.y)/DESK_ROW_H);
            if(row<desk_row_count){
                unsigned slot=desk_rows[row];
                int32_t rc=-9;
                if(slot==0xFFu){   /* Add Desk: take the first free session */
                    unsigned i,m=ksessions_mask();
                    for(i=0;i<SESSION_COUNT;++i)if(!(m&(1u<<i)))break;
                    if(i<SESSION_COUNT){rc=ksessions_control(i+1u,0);
                        if(rc==0)ksessions_autostart_desktop();}
                }else rc=ksessions_control(slot+1u,0);
                (void)rc;
            }
            desk_list_open=0;task_draw();
            return 1;
        }
        desk_list_open=0;
        wm_damage_add(&wm,desk_list_rect);
        wm_damage_add(&wm,r);
    }
    return 0;
}
/* --- DOLNY PASEK (dock) -------------------------------------------------
   Nie jest stala czescia pulpitu: zwiniety to mala ikona przy LEWEJ krawedzi
   dolnej, a klik rozwija go W PRAWO.  Rysuje go WM (a nie pulpit), bo pasek
   musi byc NAD oknami - tak samo jak gorna belka.
   LEWA strona: skroty z \SYSTEM\LAUNCH.CFG - ikona 16 px z drugiego cache'u,
   klik uruchamia program, najechanie pokazuje etykiete nad paskiem.
   PRAWA strona: ikony URUCHOMIONYCH aplikacji (widoczne lub zminimalizowane),
   z mala zebatka; klik przywraca okno, a nie uruchamia drugiej kopii. */
#define DOCK_H 24
#define DOCK_ICON 16
#define DOCK_SLOT 24
#define DOCK_HAMBURGER 22
#define DOCK_MAX_SHORT 8u
#define DOCK_MAX_RUN 5u
#define DOCK_LABEL_MAX 12u
#define DOCK_PART_NONE 0xFFu
#define DOCK_PART_RUN 0x40u
/* Jasnoszare tlo paska (indeks 15 palety systemowej = 0xAAA).  W mono tlo
   zostaje biale, a kreski czarne - ten sam kontrast w obu trybach. */
#define DOCK_BG 15u
/* Menu kontekstowe paska (PPM) - klasyczny popup nad paskiem. */
#define DOCK_MENU_MAX 10u
#define DOCK_MENU_TITLE 16
#define DOCK_MENU_ROW 16
#define DOCK_MENU_W 232
#define DOCK_MENU_PAGE 7u
enum {DOCK_MENU_NONE,DOCK_MENU_REMOVE,DOCK_MENU_ADD,DOCK_MENU_CLOSE,
      DOCK_MENU_PICK,DOCK_MENU_NEXT,DOCK_MENU_PREV};

typedef struct {char label[13];char path[64];uint8_t icon;} dock_short_t;
/* Statyczna tablica w linii (nie wskaznik): jadro ma statyczny BSS, a GUI
   apps nie sa relokowane - patrz pulapka 4 w DOCK-STAN-I-PLAN.md. */
static dock_short_t dock_short[DOCK_MAX_SHORT];
static unsigned dock_short_count;
static uint8_t dock_short_state;    /* 0 = jeszcze nie czytane, 1 = wczytane */
static uint8_t dock_cfg[768];       /* LAUNCH.CFG jest maly; czytamy leniwie */
static uint8_t dock_open;
static uint8_t dock_dirty=1u;        /* pasek do przerysowania (stan/zmiana) */
static unsigned dock_state_sig_cache; /* sygnatura okien widocznych w pasku */
static uint8_t dock_press_active,dock_press_part;
static uint32_t dock_press_pid;
static uint8_t dock_tip_shown,dock_tip_slot;
static wm_rect_t dock_tip_last;
static int dock_hover_drawn=-1;     /* slot narysowany w ostatniej kompozycji */
typedef struct {uint32_t pid;uint8_t win,icon;char label[13];} dock_run_t;
static dock_run_t dock_run[DOCK_MAX_RUN];
static unsigned dock_run_count;
static unsigned dock_hidden_any;   /* wszystkie zminimalizowane okna (lewe+prawe) */
static uint8_t dock_short_running[DOCK_MAX_SHORT];
static unsigned dock_left_running;
static uint8_t dock_menu_open,dock_menu_mode,dock_menu_page;
static uint8_t dock_menu_slot,dock_menu_rows;
static int dock_menu_hover=-1;
static uint8_t dock_menu_action[DOCK_MENU_MAX],dock_menu_arg[DOCK_MENU_MAX];
static uint8_t dock_menu_icon[DOCK_MENU_MAX];
static char dock_menu_text[DOCK_MENU_MAX][24];
static wm_rect_t dock_menu_rect;
uint32_t kdbg_dock_click,kdbg_dock_open,kdbg_dock_draws;
uint32_t kdbg_dock_slots,kdbg_dock_launch,kdbg_dock_rc;
uint32_t kdbg_dock_menu,kdbg_dock_add,kdbg_dock_remove;
uint32_t kdbg_dock_save_rc,kdbg_dock_menu_draws;
uint32_t kdbg_dock_restore,kdbg_dock_running,kdbg_dock_left_running;
uint32_t kdbg_test_spawn;           /* hook testowy: 1 = uruchom PLASMA poza skrotami */
uint32_t kdbg_test_minimize;        /* hook testowy: 1 = zminimalizuj wierzchnie okno */
uint32_t kdbg_test_close;           /* hook testowy: 1 = zamknij wierzchnie okno (WCLS) */

static int dock_height(void){return DOCK_H;}
static void dock_geometry(wm_rect_t *r)
{
    int h=dock_height();
    int w=dock_open?(wm.screen_w-16):h;
    r->x=8;r->y=(int16_t)(wm.screen_h-h-2);r->w=(uint16_t)w;r->h=(uint16_t)h;
}
static unsigned dock_strlen(const char *text)
{
    unsigned n=0;while(text[n])++n;return n;
}
static void dock_copy(char *destination,unsigned capacity,const char *source)
{
    unsigned i=0;
    while(i+1u<capacity&&source[i]){destination[i]=source[i];++i;}
    destination[i]=0;
}
static char *dock_trim_left(char *text)
{
    while(*text==' '||*text=='\t')++text;
    return text;
}
static void dock_trim_right(char *text)
{
    unsigned n=dock_strlen(text);
    while(n&&(text[n-1u]==' '||text[n-1u]=='\t'||text[n-1u]=='\r'))text[--n]=0;
}
static int dock_ext_is(const char *path,const char *extension)
{
    unsigned n=dock_strlen(path),e=dock_strlen(extension),i;
    if(e==0u||n<e)return 0;
    for(i=0;i<e;++i){
        char a=path[n-e+i],b=extension[i];
        if(a>='a'&&a<='z')a=(char)(a-32);
        if(b>='a'&&b<='z')b=(char)(b-32);
        if(a!=b)return 0;
    }
    return 1;
}
static int dock_path_equal(const char *a,const char *b)
{
    unsigned i=0;
    for(;;){
        char ca=a[i],cb=b[i];
        if(ca>='a'&&ca<='z')ca=(char)(ca-32);
        if(cb>='a'&&cb<='z')cb=(char)(cb-32);
        if(ca!=cb)return 0;
        if(!ca)return 1;
        ++i;
    }
}
/* Katalog programow i gier: JEDNO miejsce, ktore daje kazdemu programowi
   wlasna, sensowna ikone (wczesniej kazdy .APP dostawal ten sam ICON_SYSTEM
   i pasek wygladal jak rząd jednakowych zebow).  Ten sam katalog jest zrodlem
   listy "ADD ITEM..." w menu kontekstowym paska. */
typedef struct {const char *label;const char *path;uint8_t icon;} dock_catalog_t;
static const dock_catalog_t dock_catalog[]={
    {"COMMANDER","\\SYSTEM\\APP\\AC.APP",(uint8_t)ICON_NC},
    {"SYSINFO","\\SYSTEM\\APP\\SYSINFO.APP",(uint8_t)ICON_INFORMACJE},
    {"TASKMGR","\\SYSTEM\\APP\\TASKMGR.APP",(uint8_t)ICON_KONFIGURACJA},
    {"PLASMA","\\SYSTEM\\APP\\PLASMA.APP",(uint8_t)ICON_OBRAZ},
    {"ARKANOID","\\SYSTEM\\APP\\ARKANOID.APP",(uint8_t)ICON_ARKANOID},
    {"FLASHBACK","\\SYSTEM\\APP\\FLASHBK.APP",(uint8_t)ICON_PLATFORMOWE},
    {"SYSTOS","\\SYSTEM\\TEST\\SYSINFO.PRG",(uint8_t)ICON_TOS_PRG},
    {"MODPLAY","\\SYSTEM\\APP\\MODPLAY.APP",(uint8_t)ICON_MUZYKA},
    {"PRE2","\\SYSTEM\\APP\\PRE2.APP",(uint8_t)ICON_GRY},
    {"SETTINGS","\\SYSTEM\\APP\\SETTINGS.APP",(uint8_t)ICON_USTAWIENIA},
    {"GFXBENCH","\\SYSTEM\\APP\\GFXBENCH.APP",(uint8_t)ICON_GRAFIKA},
    {"COMMAND","\\SYSTEM\\APP\\COMMAND.APP",(uint8_t)ICON_TERMINAL},
    {"NOTE","\\SYSTEM\\APP\\NOTE.APP",(uint8_t)ICON_EDYTOR},
    {"MEMTEST","\\SYSTEM\\APP\\MEMTEST.APP",(uint8_t)ICON_SYSTEM},
    {"DIRTEST","\\SYSTEM\\APP\\DIRTEST.APP",(uint8_t)ICON_FOLDER},
    {"FILETEST","\\SYSTEM\\APP\\FILETEST.APP",(uint8_t)ICON_DOKUMENT}
};
#define DOCK_CATALOG_COUNT ((unsigned)(sizeof(dock_catalog)/sizeof(dock_catalog[0])))
static int dock_catalog_icon(const char *path)
{
    unsigned i;
    for(i=0;i<DOCK_CATALOG_COUNT;++i)
        if(dock_path_equal(path,dock_catalog[i].path))return (int)dock_catalog[i].icon;
    return -1;
}
/* To samo, ale po nazwie procesu (8.3 base, np. "MODPLAY.APP"): pasek musi
   rozpoznac aplikacje uruchomiona z pulpitu/shellu, nie tylko z wlasnego menu. */
static int dock_catalog_icon_name(const char *name)
{
    unsigned i;
    if(!name||!name[0])return -1;
    for(i=0;i<DOCK_CATALOG_COUNT;++i){
        const char *base=dock_catalog[i].path;unsigned j=0;
        while(base[j])++j;
        while(j&&base[j-1]!='\\'&&base[j-1]!='/')--j;
        if(dock_path_equal(base+j,name))return (int)dock_catalog[i].icon;
    }
    return -1;
}
static const char *dock_catalog_label_name(const char *name)
{
    unsigned i;
    if(!name||!name[0])return 0;
    for(i=0;i<DOCK_CATALOG_COUNT;++i){
        const char *base=dock_catalog[i].path;unsigned j=0;
        while(base[j])++j;
        while(j&&base[j-1]!='\\'&&base[j-1]!='/')--j;
        if(dock_path_equal(base+j,name))return dock_catalog[i].label;
    }
    return 0;
}
/* "MODPLAY.APP" -> "MODPLAY" (etykieta tooltipa dla nieznanych aplikacji). */
static void dock_label_from_name(char *out,unsigned cap,const char *name)
{
    unsigned i=0;
    if(!name){out[0]=0;return;}
    while(name[i]&&name[i]!='.'&&i+1u<cap){out[i]=name[i];++i;}
    out[i]=0;
}
/* Ikona wg programu (katalog), a dla nieznanych - wg rodzaju pliku. */
static uint8_t dock_icon_for(const char *path)
{
    int known=dock_catalog_icon(path);
    if(known>=0)return (uint8_t)known;
    if(dock_ext_is(path,".PRG")||dock_ext_is(path,".TOS")||
       dock_ext_is(path,".TTP")||dock_ext_is(path,".GTP"))return (uint8_t)ICON_TOS_PRG;
    if(dock_ext_is(path,".TXT")||dock_ext_is(path,".DOC"))return (uint8_t)ICON_PLIK_TEKSTOWY;
    if(dock_ext_is(path,".JPG")||dock_ext_is(path,".JPEG")||dock_ext_is(path,".PNG")||
       dock_ext_is(path,".GIF")||dock_ext_is(path,".BMP")||dock_ext_is(path,".PCX"))return (uint8_t)ICON_OBRAZ;
    if(dock_ext_is(path,".MOD")||dock_ext_is(path,".SND")||dock_ext_is(path,".WAV")||
       dock_ext_is(path,".YM"))return (uint8_t)ICON_MUZYKA;
    if(dock_ext_is(path,".ZIP")||dock_ext_is(path,".LZH")||dock_ext_is(path,".ARJ")||
       dock_ext_is(path,".RAR")||dock_ext_is(path,".GZ"))return (uint8_t)ICON_KOMPRESJA;
    if(dock_ext_is(path,".CFG")||dock_ext_is(path,".SYS")||dock_ext_is(path,".INI"))return (uint8_t)ICON_USTAWIENIA;
    return (uint8_t)ICON_SYSTEM;
}
static void dock_short_add(const char *label,const char *value)
{
    dock_short_t *entry;
    unsigned n;
    /* Builtiny pulpitu (*TERMINAL, *TRASH) nie sa plikami do spawnowania. */
    if(dock_short_count>=DOCK_MAX_SHORT||!value[0]||value[0]=='*')return;
    entry=&dock_short[dock_short_count];
    dock_copy(entry->label,sizeof(entry->label),label);
    dock_copy(entry->path,sizeof(entry->path),value);
    entry->icon=dock_icon_for(entry->path);
    /* Opcjonalne |NN na koncu sciezki = numer ikony z \SYSTEM\ICONS.DAT,
       np.  COMMANDER=\SYSTEM\APP\AC.APP|72
       Dzieki temu LAUNCH.CFG wybiera ikone bez mapy etykiet w jadrze. */
    n=0;while(entry->path[n]&&entry->path[n]!='|')++n;
    if(entry->path[n]=='|'){
        const char *digit=entry->path+n+1;
        unsigned id=0;int ok=digit[0]!=0;
        while(*digit){if(*digit<'0'||*digit>'9'){ok=0;break;}id=id*10u+(unsigned)(*digit-'0');++digit;}
        if(ok&&id<ICON_COLOR_COUNT){entry->icon=(uint8_t)id;entry->path[n]=0;}
    }
    if(!entry->path[0])return;
    ++dock_short_count;
}
/* Gdy LAUNCH.CFG nie ma (albo jest pusty), pasek nie moze byc pusty: bierzemy
   te same trzy programy, ktore pulpit ma w swojej liscie domyslnej. */
static void dock_short_defaults(void)
{
    dock_short_add("COMMANDER","\\SYSTEM\\APP\\AC.APP");
    dock_short_add("SYSINFO","\\SYSTEM\\APP\\SYSINFO.APP");
    dock_short_add("TASKMGR","\\SYSTEM\\APP\\TASKMGR.APP");
}
/* Leniwie, przy pierwszym otwarciu paska: FAT jest juz gotowy, a boot nie
   placi za czytanie pliku, ktorego zwiniety pasek nie potrzebuje.
   Kolejnosc: \SYSTEM\DOCK.CFG (wlasna lista paska, tworzona przez menu
   kontekstowe), potem \SYSTEM\LAUNCH.CFG (zgodnosc z krokiem 3 i pulpitem).
   DOCK.CFG z samym naglowkiem = swiadomie PUSTY pasek (nie wskrzeszamy
   domyslnych wpisow). */
static void dock_shortcuts_load(void)
{
    uint32_t got=0;
    unsigned i=0;
    int from_dock=0;
    char *buffer=(char *)dock_cfg;
    if(dock_short_state)return;
    dock_short_state=1u;
    dock_short_count=0u;
    if(kfat_load_path("\\SYSTEM\\DOCK.CFG",dock_cfg,sizeof(dock_cfg)-1u,&got)==FAT_FILE_FOUND&&got>=2u){
        from_dock=1;
    }else{
        got=0;
        if(kfat_load_path("\\SYSTEM\\LAUNCH.CFG",dock_cfg,sizeof(dock_cfg)-1u,&got)!=FAT_FILE_FOUND||got<2u){
            dock_short_defaults();
            kdbg_dock_slots=(uint32_t)dock_short_count;
            return;
        }
    }
    if(got>sizeof(dock_cfg)-1u)got=sizeof(dock_cfg)-1u;
    buffer[got]=0;
    while(i<got){
        char *line=buffer+i,*equals,*label,*value;
        while(i<got&&buffer[i]!='\n')++i;
        if(i<got)buffer[i++]='\0';
        dock_trim_right(line);
        label=dock_trim_left(line);
        if(!*label||*label==';'||*label=='#')continue;
        equals=label;while(*equals&&*equals!='=')++equals;
        if(*equals!='=')continue;
        *equals=0;dock_trim_right(label);
        value=dock_trim_left(equals+1);
        if(!*label||!*value)continue;
        dock_short_add(label,value);
    }
    if(!dock_short_count&&!from_dock)dock_short_defaults();
    kdbg_dock_slots=(uint32_t)dock_short_count;
}
static unsigned dock_put_uint(char *out,unsigned value)
{
    char digits[4];unsigned n=0,i;
    if(!value){out[0]='0';return 1u;}
    while(value){digits[n++]=(char)('0'+value%10u);value/=10u;}
    for(i=0;i<n;++i)out[i]=digits[n-1u-i];
    return n;
}
/* Zapis wlasnej listy paska do \SYSTEM\DOCK.CFG.  LAUNCH.CFG zostaje nietkniety
   (pulpit ma swoje wpisy i builtiny *TERMINAL, *TRASH).  Po zapisie czyscimy
   cache blokow, inaczej natychmiastowy odczyt widzi stara zawartosc. */
static int dock_shortcuts_save(void)
{
    char buffer[800];
    unsigned len=0,i;
    const char *header="; ATOS dock - pozycje paska (zapis z menu kontekstowego)\n";
    int result;
    for(i=0;header[i]&&len<sizeof(buffer)-1u;++i)buffer[len++]=header[i];
    for(i=0;i<dock_short_count;++i){
        unsigned j;
        for(j=0;dock_short[i].label[j]&&len<sizeof(buffer)-1u;++j)buffer[len++]=dock_short[i].label[j];
        if(len<sizeof(buffer)-1u)buffer[len++]='=';
        for(j=0;dock_short[i].path[j]&&len<sizeof(buffer)-1u;++j)buffer[len++]=dock_short[i].path[j];
        if(len<sizeof(buffer)-3u){buffer[len++]='|';len+=dock_put_uint(buffer+len,dock_short[i].icon);}
        if(len<sizeof(buffer)-1u)buffer[len++]='\n';
    }
    result=kfat_save_path("\\SYSTEM\\DOCK.CFG",buffer,len);
    kblock_cache_clear();
    kdbg_dock_save_rc=(uint32_t)result;
    return result;
}
static void dock_shortcuts_reload(void)
{
    dock_short_state=0u;
    dock_short_count=0u;
    dock_shortcuts_load();
}
static int dock_path_is_tos(const char *path)
{
    return dock_ext_is(path,".PRG")||dock_ext_is(path,".TOS")||
           dock_ext_is(path,".TTP")||dock_ext_is(path,".GTP");
}
static void dock_launch(unsigned index)
{
    uint32_t pid=0u;
    unsigned session=0;
    int32_t result=-1;
    if(index>=dock_short_count)return;
    if(dock_path_is_tos(dock_short[index].path)){
#if CONFIG_TOS
        /* TOS async: jeden gosc na system.  Gdy juz chodzi, klik tylko
           przywraca jego okno - nie startujemy drugiej kopii. */
        if(kernel_tos_async_owns(TOS_ASYNC_PID)){
            session=kernel_tos_async_session();
            if(session!=(unsigned)ksessions_current())(void)ksessions_control(session+1u,0);
            if(kernel_wm_activate_pid(TOS_ASYNC_PID))++kdbg_dock_restore;
            ++kdbg_dock_click;
            return;
        }
        /* .PRG/.TOS nie maja naglowka ASYSBOOT, wiec ida przez persone TOS -
           w wariancie async, zeby nie zablokowac petli jadra (i paska). */
        {extern int32_t kernel_tos_start_async(const char *path);
         result=kernel_tos_start_async(dock_short[index].path);}
#else
        result=-0x2ff;   /* profil bez persony TOS: .PRG nie ma czym uruchomic */
#endif
    }else{
        /* "Jedna zywa kopia": jesli program juz gdzies chodzi, klik skrotu ma
           PRZYWROCIC jego okno, a nie uruchomic drugi proces. */
        pid=kernel_user_find_running_app(dock_short[index].path,&session);
        if(pid){
            if(session!=(unsigned)ksessions_current())(void)ksessions_control(session+1u,0);
            if(kernel_wm_activate_pid(pid))++kdbg_dock_restore;
            ++kdbg_dock_click;
            return;
        }
        result=kernel_spawn_user_async(dock_short[index].path,"","\\",&pid);
    }
    ++kdbg_dock_click;++kdbg_dock_launch;kdbg_dock_rc=(uint32_t)result;
}
static void dock_slot_rect(unsigned index,wm_rect_t *r)
{
    wm_rect_t d;
    dock_geometry(&d);
    r->x=(int16_t)(d.x+DOCK_HAMBURGER+2+(int)index*DOCK_SLOT);
    r->y=d.y;r->w=(uint16_t)DOCK_SLOT;r->h=d.h;
}
static void dock_run_rect(unsigned index,wm_rect_t *r)
{
    wm_rect_t d;
    dock_geometry(&d);
    r->x=(int16_t)(d.x+d.w-4-(int)(index+1u)*DOCK_SLOT);
    r->y=d.y;r->w=(uint16_t)DOCK_SLOT;r->h=d.h;
}
static int dock_short_for_name(const char *name)
{
    unsigned i;
    if(!name||!name[0])return -1;
    for(i=0;i<dock_short_count;++i){
        const char *base=dock_short[i].path;unsigned j=0;
        while(base[j])++j;
        while(j&&base[j-1]!='\\'&&base[j-1]!='/')--j;
        if(dock_path_equal(base+j,name))return (int)i;
    }
    return -1;
}
static int dock_short_for_tos(void)
{
    unsigned i;
    for(i=0;i<dock_short_count;++i)
        if(dock_path_is_tos(dock_short[i].path))return (int)i;
    return -1;
}
static void dock_run_add(unsigned win)
{
    uint32_t pid;char name[13];const char *label;int icon,si;uint8_t hidden;
    pid=window_owner_pid(win);
    if(!pid||pid==kernel_desktop_active_pid())return;
    if(wm.win[win].session!=(uint8_t)ksessions_current())return;
    hidden=(uint8_t)((wm.win[win].flags&8u)!=0u);
    if(hidden)++dock_hidden_any;
    icon=-1;name[0]=0;label=0;si=-1;
    if(pid==(uint32_t)TOS_ASYNC_PID){
        icon=(int)ICON_TOS_PRG;
        label=wm.win[win].title[0]?wm.win[win].title:"TOS";
        si=dock_short_for_tos();
    }else{
        (void)kernel_user_pid_name(pid,name);
        icon=dock_catalog_icon_name(name);
        label=dock_catalog_label_name(name);
        si=dock_short_for_name(name);
    }
    if(si>=0){
        /* Aplikacja ma skrot po lewej: znaczek [>] na TYM skrocie, bez
           duplikatu po prawej. */
        if(!dock_short_running[si]){dock_short_running[si]=1u;++dock_left_running;}
        return;
    }
    if(dock_run_count>=DOCK_MAX_RUN)return;
    {dock_run_t *e=&dock_run[dock_run_count];
     if(icon<0)icon=(int)ICON_SYSTEM;
     if(label)dock_copy(e->label,sizeof(e->label),label);
     else if(name[0])dock_label_from_name(e->label,sizeof(e->label),name);
     else dock_copy(e->label,sizeof(e->label),wm.win[win].title[0]?wm.win[win].title:"APP");
     e->pid=pid;e->win=(uint8_t)win;e->icon=(uint8_t)icon;
     ++dock_run_count;}
}
/* Prawa strona: tylko aplikacje BEZ skrotu po lewej (zminimalizowane najpierw,
   potem widoczne).  Skroty z lewej dostaja znaczek [>] i nie sa duplikowane. */
static void dock_run_build(void)
{
    unsigned i;
    dock_run_count=0u;dock_hidden_any=0u;dock_left_running=0u;
    for(i=0;i<dock_short_count;++i)dock_short_running[i]=0u;
    for(i=wm.count;i--;)
        if((wm.win[i].flags&8u)&&!(wm.win[i].flags&4u))dock_run_add(i);
    for(i=wm.count;i--;)
        if((wm.win[i].flags&1u)&&!(wm.win[i].flags&8u)&&!(wm.win[i].flags&4u))dock_run_add(i);
    kdbg_dock_running=(uint32_t)dock_run_count;
    kdbg_dock_left_running=(uint32_t)dock_left_running;
}
/* Mala zebatka w prawym gornym rogu ikony - "program dziala". */
static void dock_badge_draw(wm_fb_t *d,int16_t x,int16_t y)
{
    /* [>] - mala plakietka "program dziala": czarna ramka + biala strzalka. */
    static const uint8_t arrow[5]={0x04,0x02,0x1F,0x02,0x04};
    wm_rect_t r;unsigned row,col;
    r=(wm_rect_t){(int16_t)(x+DOCK_ICON-7),y,7,7};
    wm_fill_rect(d,&r,WM_INK);
    for(row=0;row<5u;++row)for(col=0;col<5u;++col)
        if(arrow[row]&(uint8_t)(0x10u>>col)){
            r=(wm_rect_t){(int16_t)(x+DOCK_ICON-6+(int16_t)col),(int16_t)(y+1+(int16_t)row),1,1};
            wm_fill_rect(d,&r,WM_PAPER);
        }
}
/* 0 = przelacznik (hamburger / cale zwinniete pudelko), 1.. = skrot,
   DOCK_PART_NONE = puste miejsce w pasku. */
/* Pasek jest NIEPRZEZROCZYSTYM overlayem: klik w jego puste miejsce tez nie
   moze przeleciec do okna pod spodem. */
static int dock_inside(int16_t x,int16_t y)
{
    wm_rect_t r;
    dock_geometry(&r);
    return x>=r.x&&y>=r.y&&x<(int16_t)(r.x+r.w)&&y<(int16_t)(r.y+r.h);
}

static void dock_redraw_request(void)
{
    wm_rect_t r;
    dock_dirty=1u;
    if(!wm_ready)return;
    dock_geometry(&r);
    wm_damage_add(&wm,r);
    wm_force_compose=1u;
}
static int dock_damage_hit(void)
{
    wm_rect_t d,c;unsigned i;
    if(!wm_ready)return 1;
    dock_geometry(&d);
    for(i=0;i<wm.damage_count;++i)
        if(wm_rect_intersect(&wm.damage[i],&d,&c))return 1;
    return 0;
}
static unsigned dock_state_sig(void)
{
    unsigned i,s=2166136261u;
    for(i=0;i<wm.count;++i){
        if(wm.win[i].flags&4u)continue;
        s=(s^(uint32_t)wm.win[i].pid)*16777619u;
        s=(s^(uint32_t)wm.win[i].flags)*16777619u;
    }
    return s;
}

/* WVIS: aplikacja dostaje 0/1, gdy okno przechodzi miedzy "calkowicie
   zasloniete/zminimalizowane" a "widoczne".  Animacje moga wstrzymac zegar
   bez rysowania pikseli, ktorych nikt nie widzi.  Stan trzymamy poza
   wm.win[].flags, bo niektore sciezki porownuja flags dokladnie (==1 / ==7). */
#define WM_VIS_MAX 16
static struct {uint32_t owner,surf;uint8_t known,visible,tries;} wm_vis[WM_VIS_MAX];
static void wm_notify_visibility(void)
{
    unsigned i,j;uint8_t seen[WM_VIS_MAX];
    extern int kernel_user_notify_visibility(uint32_t,uint32_t);
    if(!wm_ready)return;
    for(i=0;i<WM_VIS_MAX;++i)seen[i]=0u;
    for(i=0;i<wm.count;++i){
        uint32_t owner,surf;int vis;
        if(wm.win[i].flags&4u)continue;
        owner=window_owner_pid(i);surf=wm.win[i].pid;
        if(!owner)continue;
        vis=((wm.win[i].flags&1u)&&!(wm.win[i].flags&8u)&&wm.clip_count[i]>0u)?1:0;
        for(j=0;j<WM_VIS_MAX;++j)if(wm_vis[j].owner==owner&&wm_vis[j].surf==surf)break;
        if(j==WM_VIS_MAX)for(j=0;j<WM_VIS_MAX;++j)if(!wm_vis[j].owner)break;
        if(j==WM_VIS_MAX)continue;
        if(!wm_vis[j].owner){wm_vis[j].owner=owner;wm_vis[j].surf=surf;wm_vis[j].known=0u;wm_vis[j].visible=0u;wm_vis[j].tries=0u;}
        seen[j]=1u;
        if(!wm_vis[j].known||(int)wm_vis[j].visible!=vis){
            if(kernel_user_notify_visibility(owner,vis?1u:0u)==0){
                wm_vis[j].known=1u;wm_vis[j].visible=(uint8_t)vis;wm_vis[j].tries=0u;
            }else if(++wm_vis[j].tries>=8u){
                /* Skrzynka pelna/brak odbiorcy: nie zalewaj WM kolejnymi
                   probami.  Animacja moze stracic jedna zmiane, ale nie
                   zawiesi petli. */
                wm_vis[j].known=1u;wm_vis[j].visible=(uint8_t)vis;wm_vis[j].tries=0u;
            }
        }
    }
    /* Usun wpisy okien, ktorych juz nie ma (zwalnia slot dla nowych). */
    for(j=0;j<WM_VIS_MAX;++j)if(wm_vis[j].owner&&!seen[j]){
        wm_vis[j].owner=0;wm_vis[j].surf=0;wm_vis[j].known=0;wm_vis[j].visible=0;wm_vis[j].tries=0;
    }
}
static uint8_t dock_part_at(int16_t x,int16_t y)
{
    wm_rect_t r,s;unsigned i;
    dock_geometry(&r);
    if(!dock_inside(x,y))return DOCK_PART_NONE;
    if(!dock_open)return 0u;
    if(x<(int16_t)(r.x+DOCK_HAMBURGER))return 0u;
    for(i=0;i<dock_short_count;++i){
        dock_slot_rect(i,&s);
        if(x>=s.x&&y>=s.y&&x<(int16_t)(s.x+s.w)&&y<(int16_t)(s.y+s.h))return (uint8_t)(1u+i);
    }
    dock_run_build();
    for(i=0;i<dock_run_count;++i){
        dock_run_rect(i,&s);
        if(x>=s.x&&y>=s.y&&x<(int16_t)(s.x+s.w)&&y<(int16_t)(s.y+s.h))
            return (uint8_t)(DOCK_PART_RUN+i);
    }
    return DOCK_PART_NONE;
}
static void dock_tip_hide(void)
{
    if(!dock_tip_shown)return;
    wm_damage_add(&wm,dock_tip_last);
    wm_force_compose=1;
    dock_tip_shown=0u;dock_tip_slot=0xFFu;
}
/* Etykieta nad paskiem: rysowana tylko przy zmianie slotu (bez migania),
   a stary prostokat oddawany jako damage, zeby tlo wrocilo po zjechaniu. */
static void dock_tip_show(int key,int16_t anchor_x,const char *label)
{
    wm_fb_t d;wm_rect_t box,r,t;
    unsigned length=0;
    if(!wm_framebuffer||!label)return;
    if(key!=dock_tip_slot)dock_tip_hide();
    while(label[length]&&length<DOCK_LABEL_MAX)++length;
    if(!length)return;
    dock_geometry(&r);
    t.w=(uint16_t)(length*8u+8u);t.h=16u;
    t.x=(int16_t)(anchor_x+((DOCK_SLOT-(int)t.w)/2));
    if(t.x<1)t.x=1;
    if((int)t.x+(int)t.w>(int)wm.screen_w-1)t.x=(int16_t)((int)wm.screen_w-1-(int)t.w);
    t.y=(int16_t)(r.y-20);
    if(t.y<1)t.y=1;
    box.x=(int16_t)(t.x-1);box.y=(int16_t)(t.y-1);
    box.w=(uint16_t)(t.w+2u);box.h=18u;
    d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;
    d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
    wm_fill_rect(&d,&box,WM_INK);
    wm_fill_rect(&d,&t,WM_PAPER);
    kgfx_text((int32_t)(t.x+4),(int32_t)t.y,label,WM_INK,WM_PAPER,0);
    kvideo_damage(box.x,box.y,(int32_t)box.w,(int32_t)box.h);
    dock_tip_last=box;dock_tip_shown=1u;dock_tip_slot=(uint8_t)key;
}
static int dock_hover_target(void)
{
    uint8_t part;
    if(!dock_open||!wm_mouse_valid)return -1;
    part=dock_part_at((int16_t)wm_mouse_x,(int16_t)wm_mouse_y);
    if(part>=1u&&part<=DOCK_MAX_SHORT)return (int)(part-1u);
    if(part>=DOCK_PART_RUN&&part<DOCK_PART_RUN+DOCK_MAX_RUN)
        return 100+(int)(part-DOCK_PART_RUN);
    return -1;
}
/* Zmiana slotu pod wskaznikiem musi WYMUSIC kompozycje: bez damage'u petla
   jadra konczy sie na `if(!wm.damage_count)` i pasek nie dostalby szansy
   przerysowac podswietlenia ani podpowiedzi (mysz nie generuje damage'u WM). */
static void dock_hover_poll(void)
{
    int hover;
    if(!dock_open)return;
    hover=dock_hover_target();
    if(hover==dock_hover_drawn)return;
    dock_dirty=1u;
    {wm_rect_t r;dock_geometry(&r);wm_damage_add(&wm,r);}
    wm_force_compose=1;
}
static void dock_icon_draw(wm_fb_t *d,int16_t x,int16_t y,uint8_t icon)
{
    if(wm_bpp==8u&&kgfx_icons_small_ready()){
        kgfx_icon_small_draw((int32_t)x,(int32_t)y,icon);
        return;
    }
    /* Mono albo brak cache'u 16 px: maly glyph okna aplikacji. */
    {wm_rect_t r;
     r=(wm_rect_t){x,y,DOCK_ICON,DOCK_ICON};wm_fill_rect(d,&r,WM_INK);
     r=(wm_rect_t){(int16_t)(x+1),(int16_t)(y+1),DOCK_ICON-2,DOCK_ICON-2};wm_fill_rect(d,&r,WM_PAPER);
     r=(wm_rect_t){(int16_t)(x+2),(int16_t)(y+2),DOCK_ICON-4,3};wm_fill_rect(d,&r,WM_INK);}
}
/* --- Menu kontekstowe paska (PPM) ----------------------------------------
   PPM na pasku otwiera popup: dla slotu "REMOVE: <name>", zawsze "ADD
   ITEM..." (katalog programow/gier) i "CLOSE".  Pozycje dodane/usuniete
   laduja w \SYSTEM\DOCK.CFG, wiec LAUNCH.CFG pulpitu zostaje nietkniety. */
static int dock_short_has_path(const char *path)
{
    unsigned i;
    for(i=0;i<dock_short_count;++i)
        if(dock_path_equal(dock_short[i].path,path))return 1;
    return 0;
}
static void dock_menu_row(unsigned row,uint8_t action,uint8_t arg,uint8_t icon,const char *text)
{
    if(row>=DOCK_MENU_MAX)return;
    dock_menu_action[row]=action;dock_menu_arg[row]=arg;dock_menu_icon[row]=icon;
    dock_copy(dock_menu_text[row],sizeof(dock_menu_text[row]),text);
}
static void dock_menu_geometry(void)
{
    wm_rect_t d;unsigned h;
    dock_geometry(&d);
    h=DOCK_MENU_TITLE+dock_menu_rows*DOCK_MENU_ROW+2u;
    dock_menu_rect.x=d.x;
    dock_menu_rect.w=(uint16_t)DOCK_MENU_W;
    dock_menu_rect.h=(uint16_t)h;
    dock_menu_rect.y=(int16_t)((int)d.y-(int)h-2);
    if(dock_menu_rect.y<16)dock_menu_rect.y=16;
}
static void dock_menu_build_main(void)
{
    dock_menu_rows=0u;
    if(dock_menu_slot<dock_short_count){
        char text[24];unsigned i=0;
        const char *prefix="REMOVE: ";
        while(prefix[i]&&i+1u<sizeof(text)){text[i]=prefix[i];++i;}
        dock_copy(text+i,sizeof(text)-i,dock_short[dock_menu_slot].label);
        dock_menu_row(dock_menu_rows++,DOCK_MENU_REMOVE,dock_menu_slot,
                      dock_short[dock_menu_slot].icon,text);
    }
    if(dock_short_count<DOCK_MAX_SHORT)
        dock_menu_row(dock_menu_rows++,DOCK_MENU_ADD,0u,0xFFu,"ADD ITEM...");
    else
        dock_menu_row(dock_menu_rows++,DOCK_MENU_NONE,0u,0xFFu,"DOCK FULL (8 ITEMS)");
    dock_menu_row(dock_menu_rows++,DOCK_MENU_CLOSE,0u,0xFFu,"CLOSE");
}
static void dock_menu_build_add(void)
{
    unsigned available[DOCK_CATALOG_COUNT],n=0u,i,page,pages,start;
    for(i=0;i<DOCK_CATALOG_COUNT;++i)
        if(!dock_short_has_path(dock_catalog[i].path))available[n++]=i;
    pages=(n+DOCK_MENU_PAGE-1u)/DOCK_MENU_PAGE;
    if(!pages)pages=1u;
    if((unsigned)dock_menu_page>=pages)dock_menu_page=(uint8_t)(pages-1u);
    page=dock_menu_page;
    dock_menu_rows=0u;
    if(!n){
        dock_menu_row(dock_menu_rows++,DOCK_MENU_NONE,0u,0xFFu,"NO NEW ITEMS");
    }else{
        start=page*DOCK_MENU_PAGE;
        for(i=start;i<n&&i<start+DOCK_MENU_PAGE;++i){
            unsigned ci=available[i];
            dock_menu_row(dock_menu_rows++,DOCK_MENU_PICK,(uint8_t)ci,
                          dock_catalog[ci].icon,dock_catalog[ci].label);
        }
    }
    if(pages>1u){
        if(page+1u<pages)dock_menu_row(dock_menu_rows++,DOCK_MENU_NEXT,0u,0xFFu,"MORE >");
        if(page>0u)dock_menu_row(dock_menu_rows++,DOCK_MENU_PREV,0u,0xFFu,"< BACK");
    }
    dock_menu_row(dock_menu_rows++,DOCK_MENU_CLOSE,0u,0xFFu,"CANCEL");
}
static void dock_menu_build(void)
{
    if(dock_menu_mode)dock_menu_build_add();
    else dock_menu_build_main();
    dock_menu_geometry();
}
static void dock_menu_close(void)
{
    if(!dock_menu_open)return;
    wm_damage_add(&wm,dock_menu_rect);
    wm_force_compose=1;
    dock_menu_open=0u;dock_menu_hover=-1;dock_menu_rows=0u;
    dock_dirty=1u;
}
static void dock_menu_open_at(int16_t x,int16_t y)
{
    uint8_t part;
    if(dock_menu_open)dock_menu_close();
    dock_menu_mode=0u;dock_menu_page=0u;dock_menu_slot=0xFFu;dock_menu_hover=-1;
    part=dock_part_at(x,y);
    if(part>=1u&&part<=DOCK_MAX_SHORT)dock_menu_slot=(uint8_t)(part-1u);
    dock_menu_build();
    dock_menu_open=1u;
    dock_dirty=1u;
    ++kdbg_dock_menu;
    wm_damage_add(&wm,dock_menu_rect);
    wm_force_compose=1;
}
static void dock_menu_changed(void)
{
    wm_rect_t old=dock_menu_rect;
    dock_menu_build();
    wm_damage_add(&wm,old);
    wm_damage_add(&wm,dock_menu_rect);
    dock_menu_hover=-1;
    wm_force_compose=1;
}
static int dock_menu_row_at(int32_t x,int32_t y)
{
    wm_rect_t r=dock_menu_rect;
    int32_t top;int row;
    if(!dock_menu_open)return -1;
    if(x<r.x+1||x>=(int32_t)(r.x+r.w-1))return -1;
    top=(int32_t)r.y+1+DOCK_MENU_TITLE;
    if(y<top||y>=(int32_t)(r.y+r.h-1))return -1;
    row=(int)((y-top)/DOCK_MENU_ROW);
    return (row>=0&&(unsigned)row<dock_menu_rows)?row:-1;
}
static void dock_menu_add(unsigned catalog)
{
    if(catalog>=DOCK_CATALOG_COUNT||dock_short_count>=DOCK_MAX_SHORT)return;
    if(dock_short_has_path(dock_catalog[catalog].path))return;
    dock_copy(dock_short[dock_short_count].label,sizeof(dock_short[dock_short_count].label),
              dock_catalog[catalog].label);
    dock_copy(dock_short[dock_short_count].path,sizeof(dock_short[dock_short_count].path),
              dock_catalog[catalog].path);
    dock_short[dock_short_count].icon=dock_catalog[catalog].icon;
    ++dock_short_count;
    ++kdbg_dock_add;
    (void)dock_shortcuts_save();
    dock_shortcuts_reload();
    dock_menu_close();
}
static void dock_menu_remove(unsigned index)
{
    unsigned i;
    if(index>=dock_short_count)return;
    for(i=index;i+1u<dock_short_count;++i)dock_short[i]=dock_short[i+1u];
    --dock_short_count;
    ++kdbg_dock_remove;
    (void)dock_shortcuts_save();
    dock_shortcuts_reload();
    dock_menu_close();
}
static void dock_menu_activate(unsigned row)
{
    uint8_t action,arg;
    if(row>=dock_menu_rows)return;
    action=dock_menu_action[row];arg=dock_menu_arg[row];
    if(action==DOCK_MENU_CLOSE){dock_menu_close();return;}
    if(action==DOCK_MENU_ADD){dock_menu_mode=1u;dock_menu_page=0u;dock_menu_changed();return;}
    if(action==DOCK_MENU_NEXT){++dock_menu_page;dock_menu_changed();return;}
    if(action==DOCK_MENU_PREV){if(dock_menu_page)--dock_menu_page;dock_menu_changed();return;}
    if(action==DOCK_MENU_REMOVE){dock_menu_remove(arg);return;}
    if(action==DOCK_MENU_PICK){dock_menu_add(arg);return;}
}
static int dock_menu_press(int16_t x,int16_t y)
{
    int row;
    if(!dock_menu_open)return 0;
    row=dock_menu_row_at(x,y);
    if(row>=0)dock_menu_activate((unsigned)row);
    else dock_menu_close();
    return 1;
}
static void dock_right_click(int16_t x,int16_t y)
{
    if(dock_menu_open){dock_menu_close();return;}
    if(!dock_inside(x,y))return;
    dock_menu_open_at(x,y);
}
static void dock_menu_hover_poll(void)
{
    int row;
    if(!dock_menu_open)return;
    row=dock_menu_row_at(wm_mouse_x,wm_mouse_y);
    if(row==dock_menu_hover)return;
    dock_menu_hover=row;
    wm_damage_add(&wm,dock_menu_rect);
    wm_force_compose=1;
}
static void dock_menu_paint(void)
{
    wm_fb_t d;wm_rect_t box,title,row;unsigned i;int32_t top;
    if(!dock_menu_open||!wm_framebuffer)return;
    ++kdbg_dock_menu_draws;
    d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;
    d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
    wm_fill_rect(&d,&dock_menu_rect,WM_INK);
    box=dock_menu_rect;
    box.x=(int16_t)(box.x+1);box.y=(int16_t)(box.y+1);
    box.w=(uint16_t)(box.w-2u);box.h=(uint16_t)(box.h-2u);
    wm_fill_rect(&d,&box,WM_PAPER);
    title=box;title.h=DOCK_MENU_TITLE;
    wm_fill_rect(&d,&title,(wm_bpp==8u)?NC_BLUE:WM_INK);
    kgfx_text((int32_t)title.x+4,(int32_t)title.y,
              dock_menu_mode?"ADD ITEM":"DOCK - SETTINGS",
              WM_PAPER,(wm_bpp==8u)?NC_BLUE:WM_INK,0);
    top=(int32_t)title.y+DOCK_MENU_TITLE;
    for(i=0;i<dock_menu_rows;++i){
        int16_t ry=(int16_t)(top+(int32_t)i*DOCK_MENU_ROW);
        uint8_t ink=WM_INK,paper=WM_PAPER;
        row=(wm_rect_t){box.x,ry,box.w,DOCK_MENU_ROW};
        if((int)i==dock_menu_hover){
            wm_fill_rect(&d,&row,(wm_bpp==8u)?NC_BRIGHT:WM_INK);
            ink=WM_PAPER;paper=(wm_bpp==8u)?NC_BRIGHT:WM_INK;
        }
        if(dock_menu_icon[i]!=0xFFu)
            dock_icon_draw(&d,(int16_t)(row.x+3),ry,dock_menu_icon[i]);
        kgfx_text((int32_t)(row.x+(dock_menu_icon[i]!=0xFFu?22:4)),(int32_t)ry,
                  dock_menu_text[i],ink,paper,0);
    }
    kvideo_damage(dock_menu_rect.x,dock_menu_rect.y,
                  (int32_t)dock_menu_rect.w,(int32_t)dock_menu_rect.h);
}
/* Router myszy (kernel_exception.c) pyta o ten obszar PRZED oddaniem zdarzenia
   aplikacji.  Przy otwartym menu zwracamy 1 wszedzie: menu jest modalne i
   klik poza nim tylko je zamyka, nie klikajac okna pod spodem. */
int kernel_wm_dock_at(int32_t x,int32_t y)
{
    if(!wm_ready||!kgfx_desktop_active())return 0;
    if(dock_menu_open)return 1;
    return dock_inside((int16_t)x,(int16_t)y);
}
static void dock_draw(void)
{
    wm_fb_t d;wm_rect_t r,c;unsigned i;int k,hover;
    if(!wm_framebuffer)return;
    if(dock_open&&!dock_short_state)dock_shortcuts_load();
    dock_run_build();
    dock_geometry(&r);++kdbg_dock_draws;
    d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;
    d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
    wm_fill_rect(&d,&r,(wm_bpp==8u)?DOCK_BG:WM_PAPER);
    for(k=0;k<3;++k){
        c=(wm_rect_t){(int16_t)(r.x+5),(int16_t)(r.y+6+k*5),12,2};
        wm_fill_rect(&d,&c,WM_INK);   /* czarne kreski na jasnym tle (i w mono) */
    }
    hover=dock_hover_target();
    dock_hover_drawn=hover;
    if(dock_open){
        for(i=0;i<dock_short_count;++i){
            wm_rect_t s;dock_slot_rect(i,&s);
            if((int)i==hover&&wm_bpp==8u)wm_fill_rect(&d,&s,NC_BRIGHT);
            dock_icon_draw(&d,(int16_t)(s.x+4),(int16_t)(s.y+4),dock_short[i].icon);
            /* [>] na skrocie programu, ktory juz dziala (widoczny lub
               zminimalizowany) - klik przywroci okno, nie uruchomi kopii. */
            if(dock_short_running[i])dock_badge_draw(&d,(int16_t)(s.x+4),(int16_t)(s.y+4));
        }
        /* Prawa strona: aplikacje BEZ skrotu po lewej.  [>] = program dziala. */
        for(i=0;i<dock_run_count;++i){
            wm_rect_t s;dock_run_rect(i,&s);
            if(hover==100+(int)i&&wm_bpp==8u)wm_fill_rect(&d,&s,NC_BRIGHT);
            dock_icon_draw(&d,(int16_t)(s.x+4),(int16_t)(s.y+4),dock_run[i].icon);
            dock_badge_draw(&d,(int16_t)(s.x+4),(int16_t)(s.y+4));
        }
    }else if(dock_hidden_any){
        /* Zwiniety pasek: [>] na hamburgerze, gdy jakas aplikacja jest UKRYTA
           (zminimalizowana) - np. MODPLAY zwiniety do paska. */
        dock_badge_draw(&d,(int16_t)(r.x+8),(int16_t)r.y);
    }
    kvideo_damage(r.x,r.y,(int32_t)r.w,(int32_t)r.h);
    if(hover>=100){
        unsigned idx=(unsigned)(hover-100);
        if(idx<dock_run_count){
            wm_rect_t s;dock_run_rect(idx,&s);
            dock_tip_show(hover,s.x,dock_run[idx].label);
        }else dock_tip_hide();
    }else if(hover>=0&&hover<(int)dock_short_count){
        wm_rect_t s;dock_slot_rect((unsigned)hover,&s);
        dock_tip_show(hover,s.x,dock_short[hover].label);
    }else dock_tip_hide();
    dock_dirty=0u;
}
/* Klik dziala na PUSZCZENIU (MagiC/Win98): press tylko zapamietuje slot, a
   akcja jest, gdy release wypadnie w tym samym slocie.  Dzieki temu "wiszący"
   press przy starcie niczego nie przelacza (pulapka 1). */
static int dock_press(int16_t x,int16_t y)
{
    uint8_t part;
    if(!dock_inside(x,y))return 0;
    part=dock_part_at(x,y);
    dock_press_active=1u;dock_press_part=part;dock_press_pid=0u;
    if(part>=DOCK_PART_RUN&&part<DOCK_PART_RUN+DOCK_MAX_RUN){
        unsigned i=(unsigned)(part-DOCK_PART_RUN);
        if(i<dock_run_count)dock_press_pid=dock_run[i].pid;
    }
    return 1;
}
static void dock_release(int16_t x,int16_t y)
{
    uint8_t part;
    if(!dock_press_active)return;
    dock_press_active=0u;
    part=dock_part_at(x,y);
    if(part!=dock_press_part)return;
    if(part==0u){
        wm_rect_t old_rect,new_rect;
        dock_geometry(&old_rect);
        dock_open=(uint8_t)!dock_open;
        dock_dirty=1u;
        dock_geometry(&new_rect);
        /* Zwiniete 24x24, rozwiniety caly pas: damage OBU prostokatow, bo przy
           zamykaniu trzeba zamazac caly pasek, a przy otwieraniu domalowac go. */
        wm_damage_add(&wm,old_rect);
        wm_damage_add(&wm,new_rect);
        wm_force_compose=1;
        ++kdbg_dock_click;kdbg_dock_open=dock_open;
        if(!dock_open){dock_tip_hide();dock_hover_drawn=-1;}
    }else if(part<=DOCK_MAX_SHORT){
        dock_launch((unsigned)(part-1u));
    }else if(part>=DOCK_PART_RUN&&part<DOCK_PART_RUN+DOCK_MAX_RUN){
        /* Klik w ikone uruchomionej aplikacji: PRZYWRACAMY okno (zdjecie
           minimalizacji / podniesienie), nie startujemy drugiej kopii. */
        if(dock_press_pid&&kernel_wm_activate_pid(dock_press_pid)){
            ++kdbg_dock_restore;wm_force_compose=1;
        }
    }
}

static void task_draw(void)
{
    wm_fb_t d;wm_rect_t r,t;unsigned i,k;int16_t y;
    ++kdbg_task_draws;
    if(!wm_framebuffer)return;
    task_build_rows();
    d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
    /* Gorna belka nie rysuje juz ikon task/desk - pasek na dole przejmuje ich
       role.  Zostaje tylko maszyneria dropdownow (hit-test + listy), na wypadek
       gdy ktos w przyszlosci podepnie je pod inny skrot/menu. */

    if(desk_list_open){
        /* The desktop drop-down lives in the framebuffer just like the task
           list; hand its rectangle back so the composition restores it. */
        desk_list_geometry();r=desk_list_rect;desk_list_last=r;desk_list_drawn=1u;++kdbg_desk_list_draws;
        wm_fill_rect(&d,&r,0);
        t.x=(int16_t)(r.x+1);t.y=(int16_t)(r.y+1);
        t.w=(uint16_t)(r.w-2);t.h=(uint16_t)(r.h-2);
        wm_fill_rect(&d,&t,1);
        kvideo_damage(r.x,r.y,(int32_t)r.w,(int32_t)r.h);
        y=(int16_t)(r.y+1);
        for(k=0;k<desk_row_count;++k){
            char lbl[12];unsigned li=0;
            uint8_t slot=desk_rows[k];
            int cur=(slot!=0xFFu&&(unsigned)slot==ksessions_current());
            const char *w=slot==0xFFu?"Add Desk":"Desk ";
            while(*w)lbl[li++]=*w++;
            if(slot!=0xFFu)lbl[li++]=(char)('1'+slot);
            lbl[li]=0;
            if(cur){wm_rect_t hb;hb.x=(int16_t)(r.x+1);hb.y=y;
                    hb.w=(uint16_t)(r.w-2);hb.h=DESK_ROW_H;
                    wm_fill_rect(&d,&hb,WM_INK);}
            desk_text((int16_t)(r.x+5),y,lbl,cur?WM_PAPER:WM_INK,cur?WM_INK:WM_PAPER,1);
            y=(int16_t)(y+DESK_ROW_H);
        }
        return;
    }
    if(desk_list_drawn){
        wm_damage_add(&wm,desk_list_last);
        wm_force_compose=1;
        kvideo_damage(desk_list_last.x,desk_list_last.y,
                      (int32_t)desk_list_last.w,(int32_t)desk_list_last.h);
        task_strip_repaint=1u;
        desk_list_drawn=0u;
    }
    if(!task_list_open){
        if(task_list_drawn){
            /* Closing must ERASE the list.  It lives in the framebuffer, and the
               compositor owns the canvas underneath, so hand the rectangle back
               as WM damage (plus video damage) instead of leaving it stale -
               that was the "garbage in the top bar" and the disappearing icons. */
            wm_damage_add(&wm,task_list_last);
            wm_force_compose=1;
            kvideo_damage(task_list_last.x,task_list_last.y,
                          (int32_t)task_list_last.w,(int32_t)task_list_last.h);
            task_strip_repaint=1u;   /* put the top-bar icons back afterwards */
            ++kdbg_task_closes;
            task_list_drawn=0u;
        }
        return;
    }
    task_list_geometry();r=task_list_rect;
    task_list_last=r;task_list_drawn=1u;
    wm_fill_rect(&d,&r,0);
    t.x=(int16_t)(r.x+1);t.y=(int16_t)(r.y+1);t.w=(uint16_t)(r.w-2);t.h=(uint16_t)(r.h-2);
    wm_fill_rect(&d,&t,1);
    kvideo_damage(r.x,r.y,(int32_t)r.w,(int32_t)r.h);
    y=(int16_t)(r.y+1);
    if(!task_row_count)desk_text((int16_t)(r.x+5),y,"No minimized windows",WM_INK,WM_PAPER,1);
    for(k=0;k<task_row_count;++k){
        i=task_rows[k];
        const char *label=wm.win[i].title[0]?wm.win[i].title:"(window)";
        desk_text((int16_t)(r.x+5),y,label,WM_INK,WM_PAPER,1);
        y=(int16_t)(y+TASK_ROW_H);
    }
}
/* Draw the top-right applet modules (the clock) into the live framebuffer.
   The clock shows HH:MM, so the caller decides when a redraw is warranted. */
extern void clock_set_time(uint32_t day_seconds);
extern void clock_set_colors(uint8_t ink,uint8_t paper);
extern void clock_set_transparent(uint8_t on);
static void applet_draw(uint32_t now)
{
#if !WM_CLOCK_APPLET
    /* Clock disabled (requested): wipe its strip cell once and do nothing else,
       so the top bar costs only the badge + task icon redraw. */
    (void)now;
    /* Nothing to erase: the bar is painted by the desktop canvas and this strip
       was never drawn on.  Filling it with WM_PAPER left a white block on a
       themed (steel) bar. */
    return;
#else
    video_surface_t vs;uint16_t pitch=wm_pitch;uint8_t bpp=wm_bpp;uint8_t *fb;
    static uint8_t ready;int x;
    if(!ready){
        desktop_module_register(&desktop_mod_clock);desktop_module_init_all(0);
        /* Seed the clock from the RTC once; no clock means 12:00 AM. */
        clock_set_colors(WM_INK,WM_PAPER);
        /* A themed (dark) top bar is a gradient: the clock draws glyphs only. */
        clock_set_transparent(kgfx_theme_ink_on_bar());
        {rtc_datetime_t dt;
         if(kernel_rtc_read(&dt))
             clock_set_time((uint32_t)dt.hour*3600u+(uint32_t)dt.minute*60u+dt.second);}
        ready=1u;
    }
    fb=wm_get_fb(&pitch,&bpp);
    if(!fb)return;
    x=(int)wm.screen_w-66;
    desktop_module_tick_all(now);
    vs.width=(uint16_t)wm.screen_w;vs.height=(uint16_t)wm.screen_h;vs.pitch=pitch;
    vs.format=(uint8_t)((bpp==8u)?VIDEO_FMT_CHUNKY_8BPP:VIDEO_FMT_PLANAR_1BPP);
    vs.pixels=fb;
    desktop_module_draw_all(&vs,x,1,64,7);
    kvideo_damage((int32_t)x,1,64,7);
#endif
}
/* The compositor-owned top-bar strip: session badge + task icon + applet.  When
   the composition covered it (the desktop canvas spans the whole screen) it has
   to be repainted; otherwise a minute change is the only reason. */
static int topstrip_damaged(void)
{
    wm_rect_t strip,hit;unsigned d;
    strip.x=(int16_t)(wm.screen_w-106);strip.y=0;strip.w=106;strip.h=16;
    for(d=0;d<wm.damage_count;++d){
        if(wm_rect_intersect(&wm.damage[d],&strip,&hit))return 1;
        /* The OPEN task list also lives in the framebuffer, below the strip.
           Without this check a composition over the list area painted the
           desktop canvas over it and never put it back. */
        if(task_list_open&&wm_rect_intersect(&wm.damage[d],&task_list_last,&hit))return 1;
        if(desk_list_open&&wm_rect_intersect(&wm.damage[d],&desk_list_last,&hit))return 1;
    }
    return 0;
}
/* Consume a click on the icon or the open list.  1 = handled. */
static int task_click(int16_t mx,int16_t my)
{
    wm_rect_t r;task_icon_rect(&r);
    task_build_rows();
    if(mx>=r.x&&my>=r.y&&mx<(int16_t)(r.x+r.w)&&my<(int16_t)(r.y+r.h)){
        task_list_open=(uint8_t)!task_list_open;if(task_list_open)++kdbg_task_opens;else ++kdbg_task_closes;task_draw();return 1;}
    if(task_list_open){
        task_list_geometry();
        if(mx>=task_list_rect.x&&my>=task_list_rect.y&&
           mx<(int16_t)(task_list_rect.x+task_list_rect.w)&&my<(int16_t)(task_list_rect.y+task_list_rect.h)){
            unsigned row=(unsigned)((my-task_list_rect.y-1)/TASK_ROW_H);
            if(row<task_row_count){
                unsigned idx=task_rows[row];
                if(wm.win[idx].flags&8u)window_restore(idx);
                else{wm_damage_add(&wm,wm.win[idx].frame);wm_raise(&wm,idx);}
            }
            task_list_open=0;task_draw();
            return 1;
        }
        /* Click elsewhere closes the list; only its own rect is repainted. */
        task_list_open=0;
        wm_damage_add(&wm,task_list_rect);
        wm_damage_add(&wm,r);
        task_draw();return 1;
    }
    return 0;
}
/* --- Damage-clipped window chrome -----------------------------------------
   The compositor clears its damage list while recomposing, but the chrome is
   drawn afterwards.  Redrawing the whole title bar and scroll track on every
   drag step cost ~18 ms per frame (measured); only the strips the move actually
   exposed need repainting.  This mirrors GEM's w_move(): the window's own
   pixels move as a block, while only the exposed strips are redrawn.  Every
   decoration primitive is clipped to the snapshot below, so the cost is
   proportional to the moved area instead of the window size. */
static wm_rect_t deco_dmg[WM_DAMAGE_MAX];
static unsigned deco_nd;
/* Damage INTERSECTED with the window's visible clip (wm.clip).  A window's
   chrome must never be painted over the client of a window stacked above it:
   the compositor blits every client first, so a lower window's title bar or
   frame drawn afterwards would punch through the window on top. */
static wm_rect_t deco_vis[WM_DAMAGE_MAX*4];
static unsigned deco_nv;
/* a minus b -> up to four strips (the same decomposition wm_core uses). */
static unsigned deco_sub(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out,unsigned max)
{
    wm_rect_t i;unsigned n=0;
    int16_t ay1=(int16_t)(a->y+a->h),ax1=(int16_t)(a->x+a->w);
    if(!wm_rect_intersect(a,b,&i)){if(max){out[0]=*a;return 1;}return 0;}
    if(i.y>a->y&&n<max){out[n].x=a->x;out[n].y=a->y;out[n].w=a->w;out[n].h=(uint16_t)(i.y-a->y);++n;}
    if((int16_t)(i.y+i.h)<ay1&&n<max){out[n].x=a->x;out[n].y=(int16_t)(i.y+i.h);out[n].w=a->w;out[n].h=(uint16_t)(ay1-(i.y+i.h));++n;}
    if(i.x>a->x&&n<max){out[n].x=a->x;out[n].y=i.y;out[n].w=(uint16_t)(i.x-a->x);out[n].h=i.h;++n;}
    if((int16_t)(i.x+i.w)<ax1&&n<max){out[n].x=(int16_t)(i.x+i.w);out[n].y=i.y;out[n].w=(uint16_t)(ax1-(i.x+i.w));out[n].h=i.h;++n;}
    return n;
}
/* Chrome visibility for window w: (damage INTERSECT frame) minus every window
   stacked above it.  The compositor's own clip starts from the CLIENT (it blits
   client pixels), but the frame, title bar and gadgets live outside it, so the
   decoration pass has to build its own visible region - otherwise a lower
   window's chrome is either never drawn or painted over the window on top. */
static void deco_begin(unsigned w)
{
    wm_rect_t list[WM_CLIP_MAX*2];unsigned n=0,i,j;
    deco_nv=0;
    if(w>=wm.count)return;
    for(i=0;i<deco_nd&&n<WM_CLIP_MAX;++i)
        if(wm_rect_intersect(&deco_dmg[i],&wm.win[w].frame,&list[n]))++n;
    for(j=w+1;j<wm.count&&n;++j){
        wm_rect_t next[WM_CLIP_MAX*2];unsigned m=0,k;
        if(!(wm.win[j].flags&1u))continue;
        for(k=0;k<n&&m<WM_CLIP_MAX;++k)m+=deco_sub(&list[k],&wm.win[j].frame,&next[m],WM_CLIP_MAX-m);
        for(k=0;k<m;++k)list[k]=next[k];
        n=m;
    }
    for(i=0;i<n&&deco_nv<sizeof deco_vis/sizeof deco_vis[0];++i)deco_vis[deco_nv++]=list[i];
}
static int deco_hit(const wm_rect_t *r)
{
    unsigned i;wm_rect_t h;
    for(i=0;i<deco_nv;++i)if(wm_rect_intersect(&deco_vis[i],r,&h))return 1;
    return 0;
}
/* Set one framebuffer pixel only if it lies inside the window's visible chrome
   (deco_vis).  Glyphs and diagonals must not punch through a window above. */
static void deco_pixel(wm_fb_t *fb,int16_t x,int16_t y,uint8_t level)
{
    wm_rect_t p;
    if(!fb||x<0||y<0||x>=(int16_t)fb->w||y>=(int16_t)fb->h)return;
    p.x=x;p.y=y;p.w=1;p.h=1;
    if(!deco_hit(&p))return;
    if(fb->bpp==8u)fb->data[(uint32_t)y*fb->pitch+(uint32_t)x]=level;
    else{
        uint8_t m=(uint8_t)(0x80u>>(x&7)),*rp=fb->data+(uint32_t)y*fb->pitch+(uint32_t)(x>>3);
        if(level)*rp=(uint8_t)(*rp|m);else *rp=(uint8_t)(*rp&(uint8_t)~m);
    }
}
/* One 8x16 glyph clipped to the visible chrome: a glyph partly under a higher
   window keeps its exposed pixels instead of being skipped entirely. */
/* Strzalka paska przewijania rysowana jako TROJKAT z klockow, a nie glif
   0x01/0x02: font systemowy nie ma tych znakow, wiec deco_text wypisywal "?"
   w obu strzalkach (widoczne na zrzutach).  MagiC rysuje strzalki jako
   wzorce obiektow, nie jako tekst - tu robimy to samo dwoma prostokatami
   na wiersz, przycietymi do widocznego chromu. */
static void deco_fill(wm_fb_t *fb,const wm_rect_t *r,uint8_t level);
static void deco_arrow(wm_fb_t *fb,int32_t cx,int32_t cy,int up)
{
    int k;
    for(k=0;k<4;++k){
        int w=(k+1)*2;
        wm_rect_t r;
        r.x=(int16_t)(cx-w/2);
        r.y=(int16_t)(up?(cy-4+k*2):(cy+4-k*2));
        r.w=(uint16_t)w;r.h=2;
        deco_fill(fb,&r,WM_INK);
    }
}
static void deco_text(wm_fb_t *fb,int16_t x,int16_t y,uint8_t code,uint8_t ink,uint8_t paper,uint8_t opaque)
{
    const uint8_t *glyph=font_glyph((char)code,1u);
    int row,bit;
    if(!glyph)return;
    for(row=0;row<16;++row){
        uint8_t bits=glyph[row];
        if(!bits)continue;
        for(bit=0;bit<8;++bit){
            if(bits&(uint8_t)(0x80u>>bit))
                deco_pixel(fb,(int16_t)(x+bit),(int16_t)(y+row),ink);
            else if(opaque)
                deco_pixel(fb,(int16_t)(x+bit),(int16_t)(y+row),paper);
        }
    }
}
static void deco_fill(wm_fb_t *fb,const wm_rect_t *r,uint8_t level)
{
    unsigned i;wm_rect_t h;
    for(i=0;i<deco_nv;++i)
        if(wm_rect_intersect(&deco_vis[i],r,&h))wm_fill_rect(fb,&h,level);
}
/* 50% checkerboard: the only "grey" a 1bpp screen has (EmuTOS IP_4PATT).
   Used for the INACTIVE title bar; the active one is solid ink. */
static void deco_dither_rect(wm_fb_t *fb,const wm_rect_t *r)
{
    int y,x0,x1;
    if(!fb||!r)return;
    x0=r->x;x1=(int)r->x+(int)r->w;
    if(x0<0)x0=0;
    if(x1>(int)fb->w)x1=(int)fb->w;
    if(x1<=x0)return;
    for(y=r->y;y<(int)r->y+(int)r->h;++y){
        uint8_t *row;int x;
        if(y<0||y>=(int)fb->h)continue;
        row=fb->data+(uint32_t)y*fb->pitch;
        if(fb->bpp==8u){for(x=x0;x<x1;++x)row[x]=(uint8_t)((x^y)&1u);continue;}
        {uint8_t pat=(uint8_t)((y&1)?0x55u:0xAAu);
         int b,b0=x0>>3,b1=(x1-1)>>3;
         for(b=b0;b<=b1;++b){
             uint8_t m=0xFFu;
             if(b==b0)m&=(uint8_t)(0xFFu>>(x0&7));
             if(b==b1)m&=(uint8_t)(0xFFu<<(7-((x1-1)&7)));
             row[b]=(uint8_t)((row[b]&(uint8_t)~m)|(pat&m));
         }}
    }
}
static void deco_dither(wm_fb_t *fb,const wm_rect_t *r)
{
    unsigned i;wm_rect_t h;
    for(i=0;i<deco_nv;++i)
        if(wm_rect_intersect(&deco_vis[i],r,&h))deco_dither_rect(fb,&h);
}
/* Same 3D bevel as wm_bevel_rect(), but every span goes through deco_fill so a
   clipped bevel keeps its edges and only paints the exposed rows. */
static void deco_bevel(wm_fb_t *fb,const wm_rect_t *r,uint8_t ink,uint8_t paper,uint8_t sunken)
{
    wm_rect_t e;uint8_t hi=sunken?ink:paper,lo=sunken?paper:ink;
    if(!fb||!r||r->w<5||r->h<5)return;
    e.x=r->x;e.y=r->y;e.w=r->w;e.h=1;deco_fill(fb,&e,ink);
    e.y=(int16_t)(r->y+r->h-1);deco_fill(fb,&e,ink);
    e.y=r->y;e.w=1;e.h=r->h;deco_fill(fb,&e,ink);
    e.x=(int16_t)(r->x+r->w-1);deco_fill(fb,&e,ink);
    e.x=(int16_t)(r->x+1);e.y=(int16_t)(r->y+1);e.w=(uint16_t)(r->w-2);e.h=1;deco_fill(fb,&e,hi);
    e.w=1;e.h=(uint16_t)(r->h-2);deco_fill(fb,&e,hi);
    e.x=(int16_t)(r->x+r->w-2);e.y=(int16_t)(r->y+1);e.h=(uint16_t)(r->h-2);deco_fill(fb,&e,lo);
    e.x=(int16_t)(r->x+1);e.y=(int16_t)(r->y+r->h-2);e.w=(uint16_t)(r->w-2);e.h=1;deco_fill(fb,&e,lo);
}

/* Dashed outline drawn with XOR straight into the frame buffer: XOR-ing the
   same rectangle twice restores the exact original pixels, so nothing has to be
   repainted while the pointer moves (this is what GEM's graf_dragbox does). */
static void xor_rect(wm_fb_t *fb,const wm_rect_t *r)
{
    int y,x0,x1;
    if(!fb||!r)return;
    x0=r->x;x1=(int)r->x+(int)r->w;
    if(x0<0)x0=0;
    if(x1>(int)fb->w)x1=(int)fb->w;
    if(x1<=x0)return;
    for(y=r->y;y<(int)r->y+(int)r->h;++y){
        uint8_t *row;
        int x;
        if(y<0||y>=(int)fb->h)continue;
        row=fb->data+(uint32_t)y*fb->pitch;
        if(fb->bpp==8u){for(x=x0;x<x1;++x)row[x]^=0x01u;}
        else{for(x=x0;x<x1;++x){uint8_t m=(uint8_t)(0x80u>>(x&7u));uint8_t *p=row+(x>>3);*p^=m;}}
    }
}
static void xor_outline(wm_fb_t *d,const wm_rect_t *o)
{
    int k;wm_rect_t s;
    for(k=0;k<(int)o->w;k+=4){
        uint16_t len=(uint16_t)((k+2<=(int)o->w)?2u:(uint16_t)((int)o->w-k));
        if(!len)break;
        s.x=(int16_t)(o->x+k);s.y=o->y;s.w=len;s.h=1;xor_rect(d,&s);
        s.y=(int16_t)(o->y+o->h-1);xor_rect(d,&s);
    }
    for(k=0;k<(int)o->h;k+=4){
        uint16_t len=(uint16_t)((k+2<=(int)o->h)?2u:(uint16_t)((int)o->h-k));
        if(!len)break;
        s.x=o->x;s.y=(int16_t)(o->y+k);s.w=1;s.h=len;xor_rect(d,&s);
        s.x=(int16_t)(o->x+o->w-1);xor_rect(d,&s);
    }
}
/* Toggle the active drag/resize outline.  XOR is its own inverse, so this both
   removes it before a composition and restores it after (the compositor would
   otherwise overwrite part of the dashed frame, leaving traces on the next
   removal). */
static void outline_xor_active(wm_fb_t *d)
{
    if(move_drawn_valid)xor_outline(d,&move_drawn);
    if(rsz_drawn_valid)xor_outline(d,&rsz_drawn);
}
/* Only the 1px border reaches the card, not the whole window-sized rect. */
static void outline_video_damage(const wm_rect_t *o)
{
    wm_rect_t e;
    e.x=o->x;e.y=o->y;e.w=o->w;e.h=1;kvideo_damage(e.x,e.y,(int32_t)e.w,(int32_t)e.h);
    e.y=(int16_t)(o->y+o->h-1);kvideo_damage(e.x,e.y,(int32_t)e.w,(int32_t)e.h);
    e.x=o->x;e.y=o->y;e.w=1;e.h=o->h;kvideo_damage(e.x,e.y,(int32_t)e.w,(int32_t)e.h);
    e.x=(int16_t)(o->x+o->w-1);kvideo_damage(e.x,e.y,(int32_t)e.w,(int32_t)e.h);
}
/* Menu pop-up overlay.  The desktop application draws its pull-down list into
   its own full-screen canvas, which is the BOTTOM window: any application
   window that overlaps the list (they may sit as high as y=16) would cover it.
   The app declares the pop-up rectangle through GFX opcode 31; after the
   compositor and the window chrome the kernel copies that rectangle from the
   canvas surface on top, so the menu always wins.  x must be 8-aligned so the
   1bpp copy is a plain byte run. */
static uint8_t overlay_on;
static wm_rect_t overlay_rect;
void kernel_wm_overlay(int16_t x,int16_t y,uint16_t w,uint16_t h,int on)
{
    if(!wm_ready)return;
    if(overlay_on)wm_damage_add(&wm,overlay_rect);   /* erase the old list */
    if(on&&w&&h){
        overlay_rect.x=x;overlay_rect.y=y;overlay_rect.w=w;overlay_rect.h=h;
        overlay_on=1;wm_damage_add(&wm,overlay_rect);
    }else{overlay_on=0;overlay_rect.x=overlay_rect.y=0;overlay_rect.w=overlay_rect.h=0;}
    wm_force_compose=1;
}
/* Copy one rectangle of the desktop surface (the overlay source) over the
   composed frame and report it to the video layer. */
static void overlay_copy(const uint8_t *src,const wm_rect_t *r)
{
    unsigned i,y;int x0=r->x,y0=r->y;unsigned w=r->w,h=r->h;
    if(x0<0){if((int)w<=-x0)return;w=(unsigned)((int)w+x0);x0=0;}
    if(y0<0){if((int)h<=-y0)return;h=(unsigned)((int)h+y0);y0=0;}
    if(x0>=(int)wm.screen_w||y0>=(int)wm.screen_h)return;
    if((int)x0+(int)w>wm.screen_w)w=(unsigned)(wm.screen_w-x0);
    if((int)y0+(int)h>wm.screen_h)h=(unsigned)(wm.screen_h-y0);
    for(y=0;y<h;++y){
        unsigned yy=(unsigned)(y0+(int)y);
        if(wm_bpp==8u){
            uint8_t *d=wm_framebuffer+yy*wm_pitch+(unsigned)x0;
            const uint8_t *s=src+yy*wm_surface_pitch+(unsigned)x0;
            for(i=0;i<w;++i)d[i]=s[i];
        }else{
            unsigned bx=(unsigned)x0>>3;
            unsigned nb=(((unsigned)x0&7u)+w+7u)>>3;
            uint8_t *d=wm_framebuffer+yy*wm_pitch+bx;
            const uint8_t *s=src+yy*wm_surface_pitch+bx;
            for(i=0;i<nb;++i)d[i]=s[i];
        }
    }
    kvideo_damage(x0,y0,(int32_t)w,(int32_t)h);
}
/* The overlay (pull-down menu, RUN prompt) only has to be restored where this
   composition rewrote pixels, i.e. inside the damage snapshot (deco_dmg) taken
   before wm_recompose().  Copying the whole overlay on every frame made one
   typed RUN character cost a 352x56 copy plus its VRAM flush; a newly opened or
   moved overlay is damaged as a whole by kernel_wm_overlay(), so it is still
   copied completely the first time. */
static void overlay_blit(void)
{
    unsigned i,d;uint8_t *src=0;wm_rect_t c;
    if(!overlay_on||!wm_framebuffer)return;
    for(i=0;i<wm.count;++i)
        if((wm.win[i].flags&5u)==5u){src=(uint8_t *)(uintptr_t)wm.win[i].pid;break;}
    if(!src)return;
    for(d=0;d<deco_nd;++d)
        if(wm_rect_intersect(&deco_dmg[d],&overlay_rect,&c))overlay_copy(src,&c);
}
void kernel_wm_frame_tick(uint32_t now)
{
    /* Zabezpieczenie zmiany trybu: brak potwierdzenia w 4 s => powrot.
       Zegar bierzemy z przerwania Timer-C (kernel_timer_ticks), a nie z 'now'
       (tykniecia scheduler-a, ktore gubia okresy przy zwolnionej petli i w
       trybie-smieciach 4 bpp wygasaly po 8-14 s zamiast po 4 s). */
    {extern int kvideo_guard_rollback(uint32_t);
     if(kvideo_guard_rollback(kernel_timer_ticks))return;}
    extern uint32_t kernel_user_hires_clock(void);
    uint32_t t_all=kernel_user_hires_clock();
    ++kdbg_wm_calls;
    /* One composite per tick (50 Hz): everything else only appends damage.
       This is the structural guard that keeps the WM from eating the CPU. */
    if(!wm_ready)return;
    /* Exclusive game mode: the game draws the whole screen itself. */
    if(kgame_owns_screen())return;
    wm_sync_view();
    window_pending_zoom();
    wm_notify_visibility();
    /* The compositor only owns the screen in desktop mode.  Without this gate
       a WM-enabled kernel would paint the applet bar/decorations over the text
       console every main-loop iteration. */
    if(!wm_compose_forced&&!kgfx_desktop_active()&&!kernel_wm_desktop_session())return;
    /* Hook testowy (jak kdbg_test_mouse): 1 = zminimalizuj wierzchnie okno
       aplikacji.  Pozwala testowac pasek bez trafiania w maly gadzet okna. */
    if(kdbg_test_minimize){
        unsigned mi;
        for(mi=wm.count;mi--;){
            if((wm.win[mi].flags&1u)&&!(wm.win[mi].flags&4u)){window_minimize(mi);break;}
        }
        kdbg_test_minimize=0u;
    }
    /* Hook testowy: zamknij wierzchnie okno przez te sama sciezke co X
       (window_close -> WCLS), zeby sprawdzic, czy aplikacja konczy proces. */
    if(kdbg_test_close){
        unsigned ci;
        for(ci=wm.count;ci--;)
            if((wm.win[ci].flags&1u)&&!(wm.win[ci].flags&4u)){window_close(ci);break;}
        kdbg_test_close=0u;
    }
    /* Hook testowy: uruchom PLASMA poza skrotami paska (test prawej strony). */
    if(kdbg_test_spawn){
        uint32_t tp=0u;
        (void)kernel_spawn_user_async("\\SYSTEM\\APP\\PLASMA.APP","","\\",&tp);
        kdbg_test_spawn=0u;
    }
    /* TOS/desktop mode: no text console at all. The desktop is graphics only
       (background + application windows); the shell lives in the console
       session, reachable from the top switcher. */
    /* The shell window is shown on the desktop (it is raised above app
       surfaces); keyboard routing in kernel_main.c then feeds the shell while
       it is visible. */
    /* Desktop session: paint the desktop background DIRECTLY every frame.
       wm_recompose only paints background for damaged rects, and with no
       window it produced nothing - the screen kept the console image. */
    /* EmuTOS style: paint the desktop background ONCE per desktop entry, not
       every frame.  Later changes come only from damage rectangles. */
    {if(kernel_wm_desktop_session()){
        if(!wm_bg_done){uint8_t *dfb=(uint8_t *)kgfx_framebuffer(&wm_pitch,&wm_bpp);if(dfb){int dy,db;int bw=(int)(wm.screen_w>>3);for(dy=0;dy<(int)wm.screen_h;++dy){uint8_t dpat=(dy&1)?0x55u:0xAAu;uint8_t *drow=dfb+(uint32_t)dy*wm_pitch;for(db=0;db<bw;++db){if(db>=bw-8&&dy<80)continue;if(drow[db]!=dpat)drow[db]=dpat;}}}wm_bg_done=1u;kvideo_damage(0,0,(int32_t)wm.screen_w,(int32_t)wm.screen_h);}
     }else wm_bg_done=0u;}
    /* Refresh the surface every frame: the graphics layer switches framebuffer
       and bpp (1bpp card vs 8bpp surface), so a value cached at init made the
       decorations and the desktop dither use the wrong path. */
    wm_framebuffer=wm_get_fb(&wm_pitch,&wm_bpp);
    /* The top bar is NOT painted here any more: it is repainted after the
       composition (only when the strip was covered) and by kernel_wm_applet_bar()
       when the clock minute rolls over - never once per frame. */
    /* Register the console window as soon as the desktop takes over, even if
       nothing was printed since: otherwise the first full-screen desktop
       paint would leave no console at all. */
    /* Task 4: still composite when the console is unregistered but an
       application window exists - kernel_wm_console_hide() clears
       console_registered while leaving app windows in the list. */
    if(!console_registered&&kgfx_desktop_active())kernel_wm_console_present();
    if(!console_registered&&!wm.count)return;
    /* The main loop skips kconsole_flush() while the desktop is active, so the
       shell window would refresh only in 320-character chunks.  Flush it here;
       kconsole_flush() returns immediately unless something was written. */
    kconsole_flush();
    /* Mouse: title drags move the window, body/edges resize it.  Runs even
       when there is no damage, otherwise idle would swallow mouse events. */
    if(kdbg_autodrag){
        uint32_t ph=200u-kdbg_autodrag;
        if(ph<180u){
            int32_t ax=200+(int32_t)ph;
            int32_t ay=55+(int32_t)(ph/2);
            uint32_t btn=(ph>0u&&ph<179u)?1u:0u;
            ksessions_mouse_inject(ax,ay,btn);
            wm_force_compose=1;
        }else ksessions_mouse_inject(200,55,0);
        --kdbg_autodrag;
    }
    {uint32_t buttons=ksessions_mouse_buttons();
     int32_t mx=ksessions_mouse_x(),my=ksessions_mouse_y();
     int32_t px=mx,py=my;
     static uint32_t prev_buttons;
     int press_pos=ksessions_mouse_press_pending();
     if(press_pos){px=ksessions_mouse_press_x();py=ksessions_mouse_press_y();}
     /* The recorded button-down drives the press even when the frame lands a
        step later (a scripted injection burst, or a click faster than the
        frame): otherwise the click is hit-tested at the already-moved cursor.
        The state read here is the session's, because the GUI wake path
        consumes the forwarded event. */
     /* A RECORDED press is enough.  Requiring the button to still be down in
        this frame lost short clicks: while a TOS program owns the screen the
        compositor only runs on presents (~5 Hz), so a normal 100 ms click could
        fall between two frames and never be seen - and the stale pending flag
        then leaked forever.  ksessions_mouse_press_consume() below clears it. */
     int pressed=press_pos||((buttons&1u)&&!(prev_buttons&1u));
     int right_pressed=((buttons&2u)&&!(prev_buttons&2u));
     if(pressed&&press_pos)ksessions_mouse_press_consume();
     prev_buttons=buttons;
     ++kdbg_frames;wm_dbg_mx=(uint32_t)mx;wm_dbg_my=(uint32_t)my;wm_mouse_x=mx;wm_mouse_y=my;wm_mouse_valid=1;
     cursor_sync();
     dock_hover_poll();   /* otwarty pasek: zmiana slotu = wymus kompozycje */
     dock_menu_hover_poll();
     if(right_pressed)dock_right_click((int16_t)mx,(int16_t)my);
     if(1){
         if((buttons&0x1u)||pressed){
             if(pressed&&!drag_active&&!rsz_active){
                 /* px,py is the recorded button-down position (see above). */
                 /* The top-bar icon / open task list consume the click first. */
                 ++wm_dbg_press;wm_dbg_pmx=(uint32_t)px;wm_dbg_pmy=(uint32_t)py;wm_dbg_press_frame=kdbg_frames;
                 if(dock_menu_press((int16_t)px,(int16_t)py)==0&&dock_press((int16_t)px,(int16_t)py)==0&&desk_click((int16_t)px,(int16_t)py)==0&&
                   task_click((int16_t)px,(int16_t)py)==0){
                     unsigned hit=wm_hit(&wm,(int16_t)px,(int16_t)py);wm_dbg_phit=hit;
                     /* Click-to-focus: the pressed window's client takes the
                        keyboard; the desktop canvas / task bar clears it. */
                     {extern void kernel_gui_focus_set(uint32_t);
                      extern uint32_t kernel_gui_focus_pid(void);
                      extern void kernel_user_notify_focus(uint32_t);
                      uint32_t nf=hit==WM_NO_WINDOW?0u:window_owner_pid(hit);
                      if(nf!=kernel_gui_focus_pid()){kernel_gui_focus_set(nf);if(nf)kernel_user_notify_focus(nf);}}
                     /* Only manageable windows are hit-testable, so a hit means
                        the shell; anywhere else focuses the desktop app. */
                     console_focused=(hit!=WM_NO_WINDOW)?1u:0u;
                     if(hit!=WM_NO_WINDOW&&sb_click(hit,px,py)){
                         /* the scrollbar consumed the click */
                     }else if(hit!=WM_NO_WINDOW){
                         int part=wm_hit_part(&wm,hit,(int16_t)px,(int16_t)py);
                         if(part==WM_PART_MINIMIZE||part==WM_PART_CLOSE||part==WM_PART_ZOOM){
                             /* Press, don't act: draw the button sunken and
                                run the action on release (Win98 behaviour). */
                             gpress_win=hit;gpress_part=(uint8_t)part;
                             wm_damage_add(&wm,wm.win[hit].frame);
                         }else if(part==WM_PART_FRAME){
                             /* Resize: outline only, commit on release. */
                             wm_raise(&wm,hit);
                             hit=wm.count-1u;   /* wm_raise reorders: use the top slot */
                             rsz_win=hit;
                             rsz_gx=px;rsz_gy=py;
                             rsz_x0=wm.win[hit].frame.x;rsz_y0=wm.win[hit].frame.y;
                             rsz_w0=wm.win[hit].frame.w;rsz_h0=wm.win[hit].frame.h;
                             rsz_nw=rsz_w0;rsz_nh=rsz_h0;
                             rsz_edge=(uint8_t)(((px>=wm.win[hit].frame.x+wm.win[hit].frame.w-4)?1u:0u)|
                                                ((py>=wm.win[hit].frame.y+wm.win[hit].frame.h-4)?2u:0u));
                             if(!rsz_edge)rsz_edge=3u;
                             /* the 16x16 GEM sizer always resizes both ways */
                             if(px>=wm.win[hit].frame.x+wm.win[hit].frame.w-16&&
                                py>=wm.win[hit].frame.y+wm.win[hit].frame.h-16)rsz_edge=3u;
                             rsz_active=1;rsz_drawn_valid=0;++wm_dbg_rsz_start;
                         }else{
                             wm_raise(&wm,hit);   /* click to focus/raise */
                             if(part==WM_PART_TITLE){
                                 /* Dwa tryby jak w MagiC: domyslnie okno jedzie
                                    z zawartoscia, a prawy przycisk (jak Ctrl w
                                    MagiC) przelacza na sama gumke - przydatne,
                                    gdy okno jest duze albo drugi plan muli. */
                                 drag_win=wm.count-1u;
                                 drag_dx=px-wm.win[drag_win].frame.x;
                                 drag_dy=py-wm.win[drag_win].frame.y;
                                 drag_w=wm.win[drag_win].frame.w;
                                 drag_h=wm.win[drag_win].frame.h;
                                 drag_nx=wm.win[drag_win].frame.x;
                                 drag_ny=wm.win[drag_win].frame.y;
                                 wm_dbg_dx=(uint32_t)drag_dx;wm_dbg_dy=(uint32_t)drag_dy;
                                 drag_live=(uint8_t)((buttons&2u)?0u:1u);
                                 if((uint32_t)drag_w*(uint32_t)drag_h>WM_LIVE_MAX_PIXELS)
                                     drag_live=0;   /* za duze okno: gumka (koszt) */
                                 drag_step_valid=0;
                                 drag_active=1;move_drawn_valid=0;++wm_dbg_drag_start;
                             }
                         }
                     }
                 }
             }else if(sb_drag_win>=0){
                 unsigned w=(unsigned)sb_drag_win;
                 if(w<wm.count){
                     int32_t cys=(int32_t)wm.win[w].client.y;
                     int32_t track=sb_h(w)-2*(int32_t)WM_SB_W;
                     int32_t ths=sb_thumb_size(w),range=sb_range(w),thp,den;
                     thp=my-(cys+(int32_t)WM_SB_W)-sb_drag_grab;
                     if(thp<0)thp=0;
                     den=track-ths;
                     if(thp>den)thp=den;
                     {int32_t old=(int32_t)wm.win[w].src_y;
                      wm.win[w].src_y=(int16_t)(den>0?(range*thp/den):0);
                      sb_damage_from(w,old);}
                     wm_force_compose=1;
                 }
             }else if(rsz_active){
                 if(rsz_edge&1u)rsz_nw=rsz_w0+(mx-rsz_gx);
                 if(rsz_edge&2u)rsz_nh=rsz_h0+(my-rsz_gy);
                 if(rsz_nw<64)rsz_nw=64;
                 if(rsz_nh<50)rsz_nh=50;
                 if(rsz_x0+rsz_nw>(int32_t)wm.screen_w)rsz_nw=(int32_t)wm.screen_w-rsz_x0;
                 if(rsz_y0+rsz_nh>(int32_t)wm.screen_h)rsz_nh=(int32_t)wm.screen_h-rsz_y0;
                 wm_dbg_rszw=(uint32_t)rsz_nw;wm_dbg_rszh=(uint32_t)rsz_nh;
                 /* Konsola (owner pid 0): zapamietaj i utrwal nowy rozmiar. */
                 if(window_owner_pid(rsz_win)==0u&&rsz_nw>=200&&rsz_nh>=64){
                     char b[16];unsigned k=0;uint16_t v;
                     console_geom_w=(uint16_t)rsz_nw;console_geom_h=(uint16_t)rsz_nh;
                     console_geom_valid=1u;console_geom_tried=1u;
                     v=console_geom_w;
                     {char d[6];int m=0;if(!v)d[m++]='0';while(v){d[m++]=(char)('0'+v%10u);v/=10u;}while(m)b[k++]=d[--m];}
                     b[k++]='x';
                     v=console_geom_h;
                     {char d[6];int m=0;if(!v)d[m++]='0';while(v){d[m++]=(char)('0'+v%10u);v/=10u;}while(m)b[k++]=d[--m];}
                     (void)kfat_save_path("\\SYSTEM\\CONSOLE.CFG",b,(uint32_t)k);
                 }
                 /* Same cheap technique as the move: XOR the dashed outline
                    straight into the frame buffer and push only its border.
                    Damaging it through the compositor made every mouse packet
                    repaint window-sized strips, which at 1024x768 queued the
                    pointer behind dozens of packets (resize felt frozen). */
                 {wm_rect_t o;o.x=(int16_t)rsz_x0;o.y=(int16_t)rsz_y0;o.w=(uint16_t)rsz_nw;o.h=(uint16_t)rsz_nh;
                  uint32_t rnow=(uint32_t)kernel_user_hires_clock();
                  if((!rsz_drawn_valid||o.x!=rsz_drawn.x||o.y!=rsz_drawn.y||
                     o.w!=rsz_drawn.w||o.h!=rsz_drawn.h)&&
                     (!rsz_drawn_valid||(uint32_t)(rnow-outline_shown_tick)>=WM_OUTLINE_PERIOD)){
                      outline_shown_tick=rnow;
                      uint16_t pitch=wm_pitch;uint8_t bpp=wm_bpp;
                      uint8_t *fb=wm_get_fb(&pitch,&bpp);
                      if(fb){
                          wm_fb_t d;
                          d.data=fb;d.pitch=pitch;d.w=(uint16_t)wm.screen_w;
                          d.h=(uint16_t)wm.screen_h;d.bpp=bpp;
                          if(kgfx_cursor_visible())kgfx_cursor_hide();
                          if(rsz_drawn_valid){xor_outline(&d,&rsz_drawn);outline_video_damage(&rsz_drawn);}
                          xor_outline(&d,&o);outline_video_damage(&o);
                          rsz_drawn=o;rsz_drawn_valid=1;
                          cursor_sync();kvideo_flush();
                      }
                  }}
             }else if(drag_active){
                 drag_nx=mx-drag_dx;drag_ny=my-drag_dy;++wm_dbg_drag_move;
                 wm_dbg_mx=(uint32_t)mx;wm_dbg_my=(uint32_t)my;wm_dbg_nx=(uint32_t)drag_nx;wm_dbg_ny=(uint32_t)drag_ny;
                 if(drag_nx<0)drag_nx=0;
                 if(drag_ny<16)drag_ny=16;
                 if(drag_nx>(int32_t)wm.screen_w-(int32_t)drag_w)drag_nx=(int32_t)wm.screen_w-(int32_t)drag_w;
                 if(drag_ny>(int32_t)wm.screen_h-(int32_t)drag_h)drag_ny=(int32_t)wm.screen_h-(int32_t)drag_h;
                 /* Classic GEM hybrid: only a dashed outline follows the
                    pointer; the content is placed once on release.  Damage is
                    posted only when the outline actually MOVES, and the
                    compose is forced immediately (not one per 50 Hz tick), so
                    the box tracks the pointer instead of lagging. */
                 /* Live move: the content follows the pointer.  Damage is the
                    symmetric difference of the old and new frame (mostly
                    full-width strips, which blit fast), and the compose is
                    forced immediately so the window tracks the pointer. */
                 uint32_t mnow=(uint32_t)kernel_user_hires_clock();
                 if(drag_live){
                     /* Okno jedzie z zawartoscia: wm_move_live() przesuwa ramke
                        i uszkadza TYLKO odsloniete pasy, a kompozytor w tej
                        samej klatce dociaga tlo i chrom (przyciete do damage).
                        Koszt kroku jest proporcjonalny do przesuniecia, nie do
                        powierzchni okna.  Limit tempa ten sam, co dla gumki. */
                     if((int16_t)drag_nx!=wm.win[drag_win].frame.x||
                        (int16_t)drag_ny!=wm.win[drag_win].frame.y){
                         if(!drag_step_valid||(uint32_t)(mnow-outline_shown_tick)>=WM_OUTLINE_PERIOD){
                             outline_shown_tick=mnow;drag_step_valid=1;
                             wm_move(&wm,drag_win,(int16_t)drag_nx,(int16_t)drag_ny);
                             notify_geometry(drag_win);
                             wm_force_compose=1;
                         }
                     }
                 }else if((!move_drawn_valid||
                    (int16_t)drag_nx!=move_drawn.x||(int16_t)drag_ny!=move_drawn.y)&&
                    (!move_drawn_valid||(uint32_t)(mnow-outline_shown_tick)>=WM_OUTLINE_PERIOD)){
                     outline_shown_tick=mnow;
                     /* graf_dragbox: XOR-erase the old outline, XOR-draw the new
                        one, push only its border to the card.  Neither the
                        compositor nor the chrome pass runs, so a drag step costs
                        ~2*(w+h) pixels instead of a full window repaint. */
                     uint16_t pitch=wm_pitch;uint8_t bpp=wm_bpp;
                     uint8_t *fb=wm_get_fb(&pitch,&bpp);
                     if(fb){
                         wm_fb_t d;wm_rect_t o;
                         d.data=fb;d.pitch=pitch;d.w=(uint16_t)wm.screen_w;
                         d.h=(uint16_t)wm.screen_h;d.bpp=bpp;
                         if(kgfx_cursor_visible())kgfx_cursor_hide();
                         if(move_drawn_valid){
                             xor_outline(&d,&move_drawn);
                             outline_video_damage(&move_drawn);
                         }
                         o.x=(int16_t)drag_nx;o.y=(int16_t)drag_ny;
                         o.w=drag_w;o.h=drag_h;
                         xor_outline(&d,&o);
                         outline_video_damage(&o);
                         move_drawn=o;move_drawn_valid=1;
                         cursor_sync();
                         kvideo_flush();
                     }
                 }
             }
         }
         /* A short click may arrive entirely between frames. Process its
            recorded press above, then its release in THIS same frame. */
                  if(!(buttons&0x1u)){
             if(dock_press_active)dock_release((int16_t)mx,(int16_t)my);
             sb_drag_win=-1;
             if(gpress_part){
                 unsigned hit=wm_hit(&wm,(int16_t)mx,(int16_t)my);
                 if(hit==gpress_win&&wm_hit_part(&wm,hit,(int16_t)mx,(int16_t)my)==gpress_part){
                     if((gpress_part==WM_PART_CLOSE||gpress_part==WM_PART_MINIMIZE||gpress_part==WM_PART_ZOOM)&&
                        wm_open_guard_active(kernel_timer_ticks,wm_open_tick)){
                         /* Just-opened window: swallow the gesture instead of
                            closing/minimising it out from under the user. */
                         wm_damage_add(&wm,wm.win[gpress_win].frame);
                     }else if(gpress_part==WM_PART_MINIMIZE)window_minimize(gpress_win);
                      else if(gpress_part==WM_PART_ZOOM)window_zoom(gpress_win);
                     else window_close(gpress_win);
                 }else if(gpress_win<wm.count)wm_damage_add(&wm,wm.win[gpress_win].frame);
                 gpress_part=0;
             }
             if(rsz_active){
                 if(rsz_drawn_valid){
                     uint16_t pitch=wm_pitch;uint8_t bpp=wm_bpp;
                     uint8_t *fb=wm_get_fb(&pitch,&bpp);
                     if(fb){
                         wm_fb_t d;
                         d.data=fb;d.pitch=pitch;d.w=(uint16_t)wm.screen_w;
                         d.h=(uint16_t)wm.screen_h;d.bpp=bpp;
                         if(kgfx_cursor_visible())kgfx_cursor_hide();
                         xor_outline(&d,&rsz_drawn);outline_video_damage(&rsz_drawn);
                         kvideo_flush();
                     }
                     rsz_drawn_valid=0;
                 }
                 wm_dbg_rszw=(uint32_t)rsz_nw;wm_dbg_rszh=(uint32_t)rsz_nh;
                 /* AES wind_calc(WC_WORK): the work area follows the frame, so an
                    application window can be enlarged, not only shrunk.  The shell
                    window keeps its fixed bitmap (owner pid 0) and only needs the
                    plain frame resize.  One commit per gesture: frame, work area
                    and clipping together, damaged once. */
                 if(window_owner_pid(rsz_win)!=0u){
                     wm_rect_t oldr=wm.win[rsz_win].frame;
                     wm_resize_commit(&wm,rsz_win,(uint16_t)rsz_nw,(uint16_t)rsz_nh);
                     /* Zmiana rozmiaru okna aplikacji: stara deklaracja
                        content_h (sprzed resize) byla rowna STAREJ wysokosci
                        klienta, wiec po zmniejszeniu okna pasek przewijania
                        mrugal na jedna-dwie klatki, dopoki aplikacja nie
                        zadeklaruje nowej wartosci.  Przycinamy od razu, zeby
                        paska nie bylo widac ani przez chwile.  Aplikacja z
                        prawdziwa trescia przewijana zadeklaruje swoje
                        content_h przy najblizszym rysowaniu. */
                     notify_geometry(rsz_win);
                     wm_redraw_old_new(oldr,wm.win[rsz_win].frame);
                 }else{
                     wm_rect_t oldr=wm.win[rsz_win].frame;
                     /* The shell window: the text area follows the frame too
                        (clamped to the 80x25 grid) and the view re-follows the
                        cursor, so the command line never ends up under the
                        bottom border. */
                     wm_resize(&wm,rsz_win,(uint16_t)rsz_nw,(uint16_t)rsz_nh);
                     if(console_fit(rsz_win))wm_force_compose=1;
                     notify_geometry(rsz_win);
                     wm_redraw_old_new(oldr,wm.win[rsz_win].frame);
                 }
                 rsz_active=0;
             }
             if(drag_active){
                 wm_dbg_mvx=(uint32_t)drag_nx;wm_dbg_mvy=(uint32_t)drag_ny;wm_dbg_mvwin=drag_win;
                 if(drag_live){
                     wm_rect_t oldr=wm.win[drag_win].frame;
                     /* Ostatni krok mogl wypasc przez limit tempa - domknij go
                        tym samym wm_move_live(), zeby nie zostal ani piksel
                        rozjazdu miedzy ramka a powierzchnia klienta. */
                     if((int16_t)drag_nx!=wm.win[drag_win].frame.x||
                        (int16_t)drag_ny!=wm.win[drag_win].frame.y)
                         wm_move(&wm,drag_win,(int16_t)drag_nx,(int16_t)drag_ny);
                     notify_geometry(drag_win);
                     /* Wyczysc pelna liste damage i odswiez TYLKO stara i nowa
                        ramke; pelny ekran na 1024x768 trwal 1-2 s i smieci
                        byly widoczne do konca kompozycji. */
                     wm_redraw_old_new(oldr,wm.win[drag_win].frame);
                     drag_active=0;drag_live=0;drag_step_valid=0;move_drawn_valid=0;++wm_dbg_drag_end;
                 }else{
                 /* Erase the XOR outline before the window is placed. */
                 if(move_drawn_valid){
                     uint16_t pitch=wm_pitch;uint8_t bpp=wm_bpp;
                     uint8_t *fb=wm_get_fb(&pitch,&bpp);
                     if(fb){
                         wm_fb_t d;d.data=fb;d.pitch=pitch;d.w=(uint16_t)wm.screen_w;
                         d.h=(uint16_t)wm.screen_h;d.bpp=bpp;
                         if(kgfx_cursor_visible())kgfx_cursor_hide();
                         xor_outline(&d,&move_drawn);
                         outline_video_damage(&move_drawn);
                         cursor_sync();
                         kvideo_flush();
                     }
                     move_drawn_valid=0;
                 }
                 {wm_rect_t oldr=wm.win[drag_win].frame;
                 wm_move(&wm,drag_win,(int16_t)drag_nx,(int16_t)drag_ny);   /* final position */
                 notify_geometry(drag_win);
                 wm_redraw_old_new(oldr,wm.win[drag_win].frame);}
                 drag_active=0;move_drawn_valid=0;++wm_dbg_drag_end;
                 /* No routine full refresh: wm_move() already damaged the OLD and
                    NEW frame, and wm_recompose() redraws the background plus every
                    window (and its chrome) inside those rects, so the uncovered
                    area is restored by the same pass.  The full refresh stays on
                    the mode-change and buffer-invalidation paths. */
                 }
             }
         }
     }}
    /* Task 4: desktop mode with application windows - the direct background
       paint above would erase them on the next tick, and windows only append
       damage when they draw.  Force one full composite per tick so the app
       window stays visible. */
    /* Do NOT force a full composite every tick: the recompose erases the
       XOR cursor for its duration (no double buffer), which showed up as a
       pointer blinking at the tick rate.  Windows report real damage when
       their surface changes, moves or is raised, and the compositor already
       re-blits the windows on top of the desktop canvas for those rects. */
    if(0&&kernel_wm_desktop_session()&&wm.count)kernel_wm_damage_all();
    /* Dock: sygnatura widocznych okien (open/minimize/close) wymusza redraw
       tylko wtedy, gdy stan paska naprawde sie zmienil. */
    {unsigned ds=dock_state_sig();if(ds!=dock_state_sig_cache){dock_state_sig_cache=ds;dock_redraw_request();}}
    /* Cursor save/restore produces video damage without WM damage. */
    if(!wm.damage_count){kvideo_flush();return;}
    if(now==wm_last_frame&&!wm_force_compose&&!wm_compose_forced)return;
    wm_last_frame=now;wm_force_compose=0;
    /* Do the damaged rectangles cover the compositor top bar?  If so it must be
       repainted after the composition (the desktop canvas spans the screen). */
    int topstrip_hit=topstrip_damaged();
    /* Which windows does this composition touch?  Snapshot before recompose
       clears the damage list (see the decoration pass below). */
    uint8_t win_dirty[WM_MAX];
    /* Snapshot the damage for the decoration pass: wm_recompose() below clears
       the live list, and the chrome is drawn after it. */
    {unsigned d;deco_nd=wm.damage_count;if(deco_nd>WM_DAMAGE_MAX)deco_nd=WM_DAMAGE_MAX;
     for(d=0;d<deco_nd;++d)deco_dmg[d]=wm.damage[d];}
    /* Przeciaganie z zawartoscia uszkadza tylko cienkie pasy, a chrom jest
       przycinany do uszkodzenia (deco_vis) - wtedy tytul i ramka rysowalyby
       sie FRAGMENTAMI i zostawialy "schodki" starych ramek.  Dla ciagnietego
       okna chrom odswiezamy w CALOSCI: to kilka span-ow, a nie powierzchnia
       klienta, wiec koszt jest znikomy. */
    if(drag_active&&drag_win<wm.count&&(wm.win[drag_win].flags&1u)){
        deco_nd=1;deco_dmg[0]=wm.win[drag_win].frame;
    }
    {unsigned i,d;for(i=0;i<wm.count;++i){
        wm_rect_t bar,*bp=0;win_dirty[i]=0;
        if(sb_visible(i)){bar.x=(int16_t)cxs_of(i);bar.y=wm.win[i].client.y;
            bar.w=WM_SB_W;bar.h=wm.win[i].client.h;bp=&bar;}
        for(d=0;d<deco_nd;++d)
            if(wm_chrome_damaged(&wm.win[i].frame,bp,&deco_dmg[d])){
                win_dirty[i]=1;break;}
    }}
    /* Tell the video layer only the rectangles that actually changed: the
       VOFA flush then touches those scanlines, not the whole screen. */
    {unsigned d;for(d=0;d<wm.damage_count;++d){
        kvideo_damage(wm.damage[d].x,wm.damage[d].y,(int32_t)wm.damage[d].w,(int32_t)wm.damage[d].h);
        wm_damage_pixels+=(uint32_t)wm.damage[d].w*(uint32_t)wm.damage[d].h;}}
    /* Only the session being displayed composes and hit-tests its windows. */
    wm_sync_view();
    ++wm_frames;++kdbg_wm_composes;
    {uint32_t seen=kgfx_request_count();uint32_t delta=seen-wm_requests_seen;
     wm_requests_seen=seen;wm_requests_last=delta;wm_requests_total+=delta;
     if(delta>wm_requests_max)wm_requests_max=delta;}
    if(kgfx_cursor_visible())kgfx_cursor_hide();   /* never compose over it */
    /* Undraw the dashed outline first: the recompose + decorations rewrite the
       pixels under it, and XOR-ing it off afterwards would leave traces. */
    {wm_fb_t d;d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
     outline_xor_active(&d);}
    int dock_hit=dock_damage_hit();
    {extern uint32_t kernel_user_hires_clock(void);uint32_t t0=kernel_user_hires_clock();
     wm_recompose(&wm,glue_blit,0);kdbg_recompose_cycles+=kernel_user_hires_clock()-t0;}
    /* Composing client damage cannot erase the top strip.  Repaint it only
       when covered, or while closing a popup whose save-under needs repair.
       topstrip_damaged also accounts for open task/desktop dropdowns. */
    if(topstrip_hit||task_strip_repaint){task_draw();applet_draw(now);}
    task_strip_repaint=0u;
    /* Window decorations: border + title bar. Cheap (a few spans) and drawn
       every frame so they survive the desktop application's own painting. */
    {extern uint32_t kernel_user_hires_clock(void);uint32_t td0=kernel_user_hires_clock();
     wm_fb_t d;unsigned i;
     d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
     for(i=0;i<wm.count;++i){
         wm_rect_t r=wm.win[i].frame,clip,c,c2;int active=(i+1==wm.count),have=0;unsigned d2;
         if(!(wm.win[i].flags&1u))continue;
         if(wm.win[i].flags&2u)continue;   /* full-screen surface: no title bar/border */
         if(!win_dirty[i])continue;
         /* Live move: only the dragged window redraws its chrome; other windows
            would otherwise repaint their whole frame on every step. */
         if(drag_active&&(int)i!=(int)drag_win)continue;
         deco_begin(i);                 /* this window's visible part only */
         if(!deco_nv)continue;
         ++kdbg_deco_draws;
         /* Clip the chrome to the damaged part of this window.  Redrawing the
            whole frame on every outline step made the border and the close /
            minimize boxes visibly flicker. */
         /* Redraw the full chrome.  A damage-clipped version hid the title
            bar and the close/minimize boxes, which made the window look (and
            feel) ungrabbable; correct clipping needs the drawing helpers to be
            clip-aware, so it is left for a follow-up. */
         clip=r;(void)have;(void)d2;
          c.x=r.x;c.y=r.y;c.w=r.w;c.h=16;
          /* Norton-style chrome in 8 bpp: the active (top) window gets a bright
             blue title bar, the rest a darker blue; mono keeps solid vs dither. */
          if(wm_rect_intersect(&c,&clip,&c2)){
              if(active)deco_fill(&d,&c2,(wm_bpp==8u)?NC_BRIGHT:WM_INK);
              else if(wm_bpp==8u)deco_fill(&d,&c2,NC_BLUE);
              else deco_dither(&d,&c2);
          }
          /* Window title: white on the blue band (8 bpp) or inverse video (mono),
             clipped so it never runs into the gadgets. */
          {char tb[24];unsigned k,lim=(unsigned)(r.w>56?(r.w-56)/8:0);
           if(lim>23u)lim=23u;
           for(k=0;k<lim&&wm.win[i].title[k];++k)tb[k]=wm.win[i].title[k];
           tb[k]=0;
           if(tb[0]){
               unsigned ci;
               for(ci=0;ci<k;++ci)
                   deco_text(&d,(int16_t)(r.x+18+(int)ci*8),(int16_t)(r.y+1),
                             (uint8_t)tb[ci],
                             (wm_bpp==8u||active)?WM_PAPER:WM_INK,
                             (wm_bpp==8u)?(active?NC_BRIGHT:NC_BLUE):(active?WM_INK:WM_PAPER),1);
           }}
          /* Win98-style raised frame: blue shades in 8 bpp, black/white in mono,
             drawn after the title so it caps the frame. */
          deco_bevel(&d,&r,(wm_bpp==8u)?NC_BG:WM_INK,(wm_bpp==8u)?NC_BRIGHT:WM_PAPER,0);
         /* Title-bar gadgets, matching wm_hit_part(): rightmost 16px closes,
            the next 16px minimizes.  Each is a small outlined box. */
         {wm_rect_t g;int k;
          for(k=0;k<3;++k){
              uint8_t part=(uint8_t)(k==0?WM_PART_CLOSE:(k==1?WM_PART_MINIMIZE:WM_PART_ZOOM));
              int zoomed=(part==WM_PART_ZOOM&&(wm.win[i].flags&16u));
              int down=(i==gpress_win&&gpress_part==part);
              g.x=(int16_t)(r.x+r.w-17-k*17);g.y=(int16_t)(r.y+2);
              g.w=15;g.h=13;c=g;
              if(wm_rect_intersect(&c,&clip,&c2)&&deco_hit(&c2)){
                  deco_fill(&d,&g,WM_PAPER);
                  deco_bevel(&d,&g,WM_INK,WM_PAPER,(uint8_t)down);
                  if(part==WM_PART_CLOSE){
                      /* a crisp X: two 1px diagonals (nicer than font code 5) */
                      /* a crisp X: two 1px diagonals, pixel-clipped so a partly covered
                         gadget keeps its exposed strokes. */
                      {int px;
                       for(px=0;px<7;++px){
                        deco_pixel(&d,(int16_t)(g.x+4+px),(int16_t)(g.y+3+px),WM_INK);
                        deco_pixel(&d,(int16_t)(g.x+10-px),(int16_t)(g.y+3+px),WM_INK);
                      }}
                  }else if(part==WM_PART_MINIMIZE){
                      /* the classic minimise bar, centred */
                      wm_rect_t b;b.x=(int16_t)(g.x+4);b.y=(int16_t)(g.y+9);b.w=7;b.h=2;
                      deco_fill(&d,&b,WM_INK);
                  }else{
                      /* Zoom: an outlined box; inset when the window already
                         fills the screen, so the gadget shows the way back. */
                      wm_rect_t b;int o=zoomed?1:0;
                      b.x=(int16_t)(g.x+3+o);b.y=(int16_t)(g.y+2+o);
                      b.w=(uint16_t)(9-2*o);b.h=(uint16_t)(9-2*o);
                      deco_fill(&d,&b,WM_PAPER);
                      deco_bevel(&d,&b,WM_INK,WM_PAPER,0);
                  }
              }
          }
          /* GEM sizer (W_SIZER): a small 3D box with a diagonal dot hatch in the
             bottom-right corner, the standard resize affordance. */
          g.x=(int16_t)(r.x+r.w-15);g.y=(int16_t)(r.y+r.h-15);g.w=14;g.h=14;c=g;
          if(wm_rect_intersect(&c,&clip,&c2)&&deco_hit(&c2)){
              deco_fill(&d,&g,WM_PAPER);
              deco_bevel(&d,&g,WM_INK,WM_PAPER,0);
              for(k=0;k<4;++k){int j2;
                  for(j2=0;j2<=k;++j2){wm_rect_t dot;
                      dot.x=(int16_t)(g.x+g.w-3-(k-j2)*3);
                      dot.y=(int16_t)(g.y+g.h-3-j2*3);
                      dot.w=2;dot.h=2;deco_fill(&d,&dot,WM_INK);}}
          }
          /* GEM vertical scrollbar (W_VBAR): up arrow, track, down arrow, thumb.
             Scrolling moves src_y into the surface, so the client never redraws. */
          if(sb_visible(i)&&sb_h(i)>2*(int32_t)WM_SB_W){
              int32_t cxs=cxs_of(i);
              int32_t cys=(int32_t)wm.win[i].client.y,chs=sb_h(i);
              int32_t range=sb_range(i),track=chs-2*(int32_t)WM_SB_W;
              wm_rect_t u,dn,tk,th;
              u.x=(int16_t)cxs;u.y=(int16_t)cys;u.w=WM_SB_W;u.h=WM_SB_W;
              dn.x=(int16_t)cxs;dn.y=(int16_t)(cys+chs-(int32_t)WM_SB_W);dn.w=WM_SB_W;dn.h=WM_SB_W;
              tk.x=(int16_t)cxs;tk.y=(int16_t)(cys+(int32_t)WM_SB_W);tk.w=WM_SB_W;tk.h=(uint16_t)track;
              deco_fill(&d,&u,WM_PAPER);deco_bevel(&d,&u,WM_INK,WM_PAPER,0);
              deco_arrow(&d,(int32_t)u.x+8,(int32_t)u.y+8,1);
              deco_fill(&d,&dn,WM_PAPER);deco_bevel(&d,&dn,WM_INK,WM_PAPER,0);
              deco_arrow(&d,(int32_t)dn.x+8,(int32_t)dn.y+8,0);
              deco_fill(&d,&tk,WM_PAPER);
              if(range>0){
                  int32_t ths=sb_thumb_size(i);
                  int32_t thp=track?(range?(track*(int32_t)wm.win[i].src_y/range):0):0;
                  if(thp>track-ths)thp=track-ths;
                  if(thp<0)thp=0;
                  th.x=(int16_t)(tk.x+1);th.y=(int16_t)(tk.y+thp);
                  th.w=(uint16_t)(WM_SB_W-2);th.h=(uint16_t)ths;
                  deco_fill(&d,&th,WM_PAPER);deco_bevel(&d,&th,WM_INK,WM_PAPER,0);
              }
          }}
         /* Every chrome helper clips to deco_vis, a subset of deco_dmg.
            Those regions were already submitted to kvideo_damage above.
            Marking the entire frame here made a tiny HUD update scan the
            whole 448x552 window even though only a border fragment changed. */
     }
     kdbg_deco_cycles+=kernel_user_hires_clock()-td0;
     /* The resize outline is XOR-ed straight into the frame buffer in the mouse
        handler (same as the move), so the compositor must not repaint it - that
        repaint was why every resize step cost a window-sized recompose. */
     /* The move outline is XOR-ed straight into the frame buffer in the mouse
        handler (see the drag branch), so the compositor must not repaint it. */
     }
    /* Popups must remain above chrome when another window repaints. */
    if(task_list_open||desk_list_open)task_draw();
    overlay_blit();   /* menu pop-up: above window chrome, below the pointer */
    /* Dolny pasek rysujemy NA SAMYM KONCU, po oknach, dekoracjach i menu:
       jest overlayem NAD nimi (wymaganie), a nie elementem pulpitu.  Wczesniej
       stal przed petla dekoracji, wiec dolna ramka/uchwyt okna zaslanialy pasek
       i klik w slot bywal niewidoczny. */
    if(dock_dirty||dock_hit||dock_menu_open)dock_draw();
    dock_menu_paint();   /* PPM-menu paska: nad paskiem, pod kursorem */
    /* Redraw the dashed outline on top of the freshly composed pixels. */
    {wm_fb_t d;d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
     outline_xor_active(&d);}
    {extern uint32_t kernel_user_hires_clock(void);uint32_t t0=kernel_user_hires_clock();
     cursor_sync();kdbg_cursor_cycles+=kernel_user_hires_clock()-t0;}   /* pointer is topmost */
    /* The pointer sits ON the top-bar strip: its save-under restores pixels
       captured before the bar was drawn, so the task icon / session badge /
       clock disappear the moment the cursor is over them.  Redraw the bar and
       re-place the pointer - but ONLY when the pointer actually overlaps the
       strip (about 106x16 px), so the normal case costs nothing extra. */
    if(wm_mouse_valid&&wm_mouse_y>=0&&wm_mouse_y<16&&
       wm_mouse_x>=(int32_t)wm.screen_w-106){
        uint32_t t1=kernel_user_hires_clock();
        task_draw();
        applet_draw(now);
        cursor_sync();
        kdbg_cursor_cycles+=kernel_user_hires_clock()-t1;
    }
    /* Only the regions actually drawn this frame reach the card: window damage
       is reported above, the decorations and the one-time desktop background
       report themselves, and the applet bar reports its own strip.  The flush
       stays LAST so nothing painted after it is lost.  This replaces the old
       full-screen kvideo_damage(), which forced kvideo_flush() to scan all 400
       scanlines after every composite. */
    kvideo_flush();
    kdbg_total_cycles+=kernel_user_hires_clock()-t_all;
}
/* Redraw the desktop top-bar applets directly on the active framebuffer.  The
   clock is HH:MM, so a redraw is only needed when the minute rolls over: this
   is the whole point of dropping the seconds.  Called from the kernel main
   loop; must not run outside desktop mode (the text profile has its own top
   bar with the session marker). */
void kernel_wm_applet_bar(uint32_t now)
{
    static uint32_t last_minute=0xFFFFFFFFu;uint32_t minute;
    if(kconsole_boot_active())return;
    if(!kgfx_desktop_active()&&!kernel_wm_desktop_session())return;
    if(kgame_owns_screen())return;
    minute=now/(50u*60u);
    if(minute==last_minute)return;
    last_minute=minute;
    task_draw();          /* first call also brings the task icon/badge up */
    applet_draw(now);
    kvideo_flush();
}

int kernel_wm_enabled(void){return wm_ready?1:0;}
void kernel_wm_debug(int *count,int *flags,int *hidden,int *damage){if(count)*count=(int)wm.count;if(flags)*flags=wm.count>0u?(int)wm.win[0].flags:-1;if(hidden)*hidden=(int)console_hidden;if(damage)*damage=(int)wm.damage_count;}

/* The console as a compositor window: its grid is rendered into a private
   surface and composed like any other window. Inset so the desktop shows. */
static uint8_t *console_surface; /* 32 KB, only when the WM runs */
/* Wczytaj zapisany rozmiar konsoli (raz).  Zly plik ignorujemy. */
static void console_geom_load(void)
{
    char b[16];uint32_t got=0;
    if(console_geom_tried)return;
    console_geom_tried=1u;
    if(kfat_load_path("\\SYSTEM\\CONSOLE.CFG",b,sizeof(b)-1u,&got)!=FAT_FILE_FOUND||
       got<3u||got>sizeof(b)-1u)return;
    b[got]=0;
    {uint32_t k=0,w=0,h=0;
     while(k<got&&b[k]>='0'&&b[k]<='9'){w=w*10u+(uint32_t)(b[k]-'0');++k;}
     if(k>=got||b[k]!='x')return;
     ++k;
     while(k<got&&b[k]>='0'&&b[k]<='9'){h=h*10u+(uint32_t)(b[k]-'0');++k;}
     if(w>=200u&&w<=(uint32_t)wm.screen_w&&h>=64u&&h<=(uint32_t)wm.screen_h){
         console_geom_w=(uint16_t)w;console_geom_h=(uint16_t)h;console_geom_valid=1u;}}
}
/* A low strip showing the last console rows (where the prompt and the typed
   text live) so the desktop and the application windows stay visible. */
/* The console is a bottom strip; the application window owns the top.
   Deliberately non-overlapping so nothing is mis-stacked while the WM is
   still being built. */
/* Full 80x25 at 8x8 = 640x200 client; title bar makes 216. Application takes
   the top, console the rest: 216 + 184 = 400 exactly, no overlap. */
/* Leave desktop margins so the windows read as windows, not as a full
   screen: 8 pixels on the sides and between them. */
/* Console strip below; the application window above shows the WHOLE 80x25
   (640x200 client). Only a vertical gap separates them so the desktop shows. */
/* Smaller than the screen on purpose: at 640x400 an 80 column 8x8 window is
   exactly full width, which makes the desktop unclickable. Desktop visible
   around both windows until 1024x768 and text wrapping arrive. */
/* 8x16 cells: 80x25 needs 640x400, i.e. the whole screen. Windows are kept
   smaller than that on purpose so the desktop stays clickable; the shell
   surface will wrap text to the window width (next step). */
#define CONSOLE_WX 0
#define CONSOLE_WY 16          /* one text line below the screen top */
/* The console window follows the active mode: full width, as tall as the
   screen allows (it is hidden in desktop mode anyway). */
/* The shell window owns the fixed 640x400 text surface, so even on a larger
   card mode it is capped at its own bitmap size instead of reading past it. */
/* Rozmiar okna konsoli zapamietany miedzy restartami.  Wczesniej byl ZAWSZE
   wyliczany z ekranu, wiec po resecie "uparcie ginal".  Zapis do
   \SYSTEM\CONSOLE.CFG ("WxH"), odczyt raz przy starcie. */
#define CONSOLE_WW ((uint16_t)(console_geom_valid?console_geom_w:\
                    (uint16_t)((int)wm.screen_w>640?640:(int)wm.screen_w)))
/* One third of the screen: the shell grid stays 80x25, so the window shows the
   rows around the cursor (src_y) instead of taking the whole display. */
#define CONSOLE_WH ((uint16_t)(console_geom_valid?console_geom_h:\
                    (uint16_t)(((int)wm.screen_h/3>96)?(int)wm.screen_h/3:96)))


/* Optional per-process window geometry (GFX opcode 20).  A process that asks
   for a window gets a real, decorated, movable WM window; without one it keeps
   the legacy full-screen desktop canvas (the desktop background). */
void kernel_wm_set_window(uint32_t pid,uint16_t handle,int16_t x,int16_t y,uint16_t w,uint16_t h)
{
    unsigned i,slot=WM_APP_WINDOWS;
    if(!pid)return;
    /* A 0x0 declaration hides the window: a client closes its own dialog (the
       SETUP window's Escape) without waiting for the X gadget. */
    if(w==0u&&h==0u){
        int sl=app_surface_find(pid,handle);uint8_t *surf=sl<0?0:app_surface[sl];
        if(surf)for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surf){
            if(wm.win[i].flags&1u){wm.win[i].flags&=(uint8_t)~1u;wm_damage_add(&wm,wm.win[i].frame);}
            wm_rebuild_clip(&wm);wm_force_compose=1;break;}
        return;
    }
    for(i=0;i<WM_APP_WINDOWS;++i)if(wm_window_req[i].set&&wm_window_req[i].pid==pid&&wm_window_req[i].handle==handle)slot=i;
    if(slot==WM_APP_WINDOWS)for(i=0;i<WM_APP_WINDOWS;++i)if(!wm_window_req[i].set){slot=i;break;}
    if(slot==WM_APP_WINDOWS)return;
    if(x<0)x=0;
    if(y<16)y=16;
    if(w<64u)w=64u;
    if(h<50u)h=50u;
    if((int32_t)x+(int32_t)w>wm.screen_w)w=(uint16_t)(wm.screen_w-x);
    if((int32_t)y+(int32_t)h>wm.screen_h)h=(uint16_t)(wm.screen_h-y);
    wm_window_req[slot].pid=pid;wm_window_req[slot].handle=handle;
    wm_window_req[slot].x=x;wm_window_req[slot].y=y;
    wm_window_req[slot].w=w;wm_window_req[slot].h=h;wm_window_req[slot].set=1u;
    wm_open_tick=kernel_timer_ticks;
    /* Re-declared geometry keeps any document height the client already sent
       (opcode 29 often arrives before the first draw creates the surface). */
    /* Reopening a dialog: show it again if it was hidden.  Geometry is left
       alone (the client only hides/shows its own window). */
    {int sl=app_surface_find(pid,handle);uint8_t *surf=sl<0?0:app_surface[sl];
     if(surf)for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surf){
         int changed=0;wm_rect_t old=wm.win[i].frame;
         /* opcode 20 is authoritative for a normal application window: a client
            that re-declares its geometry (re-centred dialog, or a window that
            re-fits itself after a resolution change) must actually move.  The
            visible background canvas (flag bit 2) is exempt. */
         if(!(wm.win[i].flags&4u)&&
            (wm.win[i].frame.x!=x||wm.win[i].frame.y!=y||
             wm.win[i].frame.w!=w||wm.win[i].frame.h!=h)){
             wm.win[i].frame.x=x;wm.win[i].frame.y=y;
             wm.win[i].frame.w=w;wm.win[i].frame.h=h;
             wm.win[i].client.x=x;wm.win[i].client.y=(int16_t)(y+16);
             wm.win[i].client.w=w;wm.win[i].client.h=(uint16_t)(h-16u);
             changed=1;
         }
         if(!(wm.win[i].flags&1u)){wm.win[i].flags|=1u;changed=1;}
         if(changed){
             wm_damage_add(&wm,old);wm_damage_add(&wm,wm.win[i].frame);
             wm_rebuild_clip(&wm);wm_force_compose=1;
         }
         break;}}
}
static int wm_window_rect(uint32_t pid,uint16_t handle,int16_t *x,int16_t *y,uint16_t *w,uint16_t *h)
{
    unsigned i;
    for(i=0;i<WM_APP_WINDOWS;++i)if(wm_window_req[i].set&&wm_window_req[i].pid==pid&&wm_window_req[i].handle==handle){
        *x=wm_window_req[i].x;*y=wm_window_req[i].y;*w=wm_window_req[i].w;*h=wm_window_req[i].h;return 1;}
    return 0;
}
uint8_t *kernel_wm_app_surface(uint32_t pid,uint16_t handle)
{
    int found;unsigned i,slot;
    if(!wm_ready||!pid)return 0;
    found=app_surface_find(pid,handle);
    if(found>=0)return app_surface[found];
    for(i=0;i<WM_APP_WINDOWS;++i)if(!app_surface_used[i])break;
    if(i==WM_APP_WINDOWS)return 0;
    slot=i;app_surface_used[i]=1u;app_surface_pid[i]=pid;app_surface_handle[i]=handle;
    /* (Re)allocate when the mode's surface size changed: switching to 8 bpp
       needs 480000 B instead of 98304, and a window already on screen keeps
       its backing buffer - remap every reference to the new one. */
    if(app_surface[slot]&&app_surface_bytes[slot]<wm_surface_bytes()){
        uint8_t *old=app_surface[slot];
        uint8_t *fresh=kmem_alloc(wm_surface_bytes(),16u,KMEM_OWNER_KERNEL,
                                  KMEM_READ|KMEM_WRITE,wm_surface_prefer_tt());
        if(!fresh){app_surface_used[slot]=0u;return 0;}
        {unsigned k;for(k=0;k<wm.count;++k)
             if(wm.win[k].pid==(uint32_t)(uintptr_t)old)wm.win[k].pid=(uint32_t)(uintptr_t)fresh;
         for(k=0;k<WM_SURFACE_MAX;++k)if(wm_surface[k]==old)wm_surface[k]=fresh;}
        kmem_free(old,KMEM_OWNER_KERNEL);
        app_surface[slot]=fresh;app_surface_bytes[slot]=wm_surface_bytes();
    }
    if(!app_surface[slot]){
        app_surface[slot]=kmem_alloc(wm_surface_bytes(),16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,wm_surface_prefer_tt());
        if(!app_surface[slot]){app_surface_used[slot]=0u;return 0;}
        app_surface_bytes[slot]=wm_surface_bytes();
    }
    /* Compute the size once, not once per byte (480001 calls at 800x600). */
    chunky_span(app_surface[slot],wm_surface_bytes(),0u);
    /* Register the application's window next to the console window.  The TOS
       persona opens a window even without the desktop canvas, so a declared
       rect (kernel_wm_set_window) must register too. */
    {int16_t rx,ry;uint16_t rw,rh;
     int has_rect=wm_window_rect(pid,handle,&rx,&ry,&rw,&rh);
     console_geom_load();   /* raz: zapamietany rozmiar konsoli, jesli jest */
     if(wm.count<WM_MAX&&(console_registered||console_hidden||kgfx_desktop_active()||has_rect)){
        unsigned w=wm.count;
        wm_surface[1u+slot]=app_surface[slot];
        /* SysInfo (and GEM screen programs in general) lay their UI out for
           the full 640x400 desktop it reports through wind_get(), so give the
           window the whole screen: a 400x294 window clipped the right-hand
           value column and the F-key menu.  The frame carries flags bit 1,
           which tells the decorator to skip the kernel title bar - the app
           draws its own. */
        if(has_rect){
            wm.win[w].frame.x=rx;wm.win[w].frame.y=ry;
            wm.win[w].frame.w=rw;wm.win[w].frame.h=rh;
            wm.win[w].client.x=rx;wm.win[w].client.y=(int16_t)(ry+16);
            wm.win[w].client.w=rw;wm.win[w].client.h=(uint16_t)(rh-16);
            wm.win[w].flags=1;   /* real window: decorated + hit-testable */
        }else{
        wm.win[w].frame.x=0;
        wm.win[w].frame.y=0;
        wm.win[w].frame.w=(uint16_t)wm.screen_w;wm.win[w].frame.h=(uint16_t)wm.screen_h;
        wm.win[w].client.x=0;
        wm.win[w].client.y=0;
        wm.win[w].client.w=(uint16_t)wm.screen_w;
        wm.win[w].client.h=(uint16_t)wm.screen_h;
        wm.win[w].flags=7;              /* visible + no decorations + background */
        wm_background_canvas=1u;        /* the WM dither is redundant now */
        }
        wm.win[w].src_x=0;wm.win[w].src_y=0;wm.win[w].content_h=wm.win[w].client.h;
        /* Windows belong to the session of their process: the compositor shows
           only the current session's, so a desktop living on another session
           keeps its windows instead of painting over this one. */
        wm.win[w].session=(uint8_t)kernel_user_pid_terminal(pid);
        {unsigned k;const char *d="APPLICATION";
         for(k=0;k<23u&&d[k];++k){wm.win[w].title[k]=d[k];}
         wm.win[w].title[k]=0;}
        /* Apply a title the app set before its first draw (opcode 21). */
        {unsigned s=wm_window_slot(pid,handle);
         if(s<WM_APP_WINDOWS&&wm_window_req[s].title_set){
             unsigned k;for(k=0;k<23u;++k){wm.win[w].title[k]=wm_window_req[s].title[k];if(!wm.win[w].title[k])break;}
             wm.win[w].title[23]=0;}
         /* Apply a document height declared before the first draw (opcode 29):
            that is exactly when a client knows its content but has no surface
            yet, so the height must be remembered here, not dropped. */
         if(s<WM_APP_WINDOWS&&wm_window_req[s].content_set)
             wm.win[w].content_h=wm_window_req[s].content_h;}
        /* The compositor identifies a window's backing store by this field
           (the console window stores its surface pointer here too). */
        wm.win[w].pid=(uint32_t)(uintptr_t)app_surface[slot];
        wm.count=w+1u;
        wm_rebuild_clip(&wm);
        wm_damage_add(&wm,wm.win[w].frame);
        /* Only the full-screen desktop canvas stays under the shell window (so
           the shell is still reachable).  A decorated application window is
           what the user just opened, so it must stay on TOP: raising the
           console over it covered the entire client with the shell's blank
           surface and looked like "the app window is empty". */
        if(wm.win[w].flags&4u)console_raise();
     }
    }   /* close the rect/registration block */
    return app_surface[slot];
}
/* Release the surface a client acquired with kernel_wm_app_surface.  Closes
   any registered window and, when the window was never registered (desktop
   inactive at open time), frees the backing-slot directly so repeated opens do
   not leak slots. */
void kernel_wm_release_surface(uint32_t pid,uint16_t handle)
{
    int sl;
    if(!pid)return;
    sl=app_surface_find(pid,handle);
    /* Only this window: the caller may own others (see kernel_wm_close_window). */
    kernel_wm_close_window(pid,handle);
    if(sl>=0&&app_surface_used[sl]){
        app_surface_used[sl]=0;app_surface_pid[sl]=0;app_surface_handle[sl]=0;
        wm_surface[1u+sl]=0;
    }
    /* No window was ever registered (desktop inactive at open time) but the
       geometry request still holds a slot: drop it too. */
    {unsigned r;for(r=0;r<WM_APP_WINDOWS;++r)
        if(wm_window_req[r].set&&wm_window_req[r].pid==pid&&
           wm_window_req[r].handle==handle){wm_window_req[r].set=0u;break;}}
}
/* Adopt a new geometry for an already-registered window (a TOS program can
   call Setrez/Setscreen after its window opened).  Recompute the client rect
   from the frame exactly like kernel_wm_app_surface does. */
void kernel_wm_resize_window(uint32_t pid,uint16_t handle,int16_t x,int16_t y,uint16_t w,uint16_t h)
{
    unsigned i;
    if(!wm_ready)return;
    for(i=0;i<wm.count;++i){
        if(wm.win[i].flags&4u)continue;
        if(window_owner_pid(i)==pid&&window_owner_handle(i)==handle){
            wm_damage_add(&wm,wm.win[i].frame);
            wm.win[i].frame.x=x;wm.win[i].frame.y=y;
            wm.win[i].frame.w=w;wm.win[i].frame.h=h;
            wm.win[i].client.x=x;wm.win[i].client.y=(int16_t)(y+16);
            wm.win[i].client.w=w;wm.win[i].client.h=(uint16_t)(h>16?h-16:0);
            wm.win[i].content_h=wm.win[i].client.h;
            wm_damage_add(&wm,wm.win[i].frame);
            wm_rebuild_clip(&wm);
            return;
        }
    }
}
int kernel_wm_dragging(void){return drag_active!=0;}
/* The client declares its document height; a value above client.h enables the
   GEM-style vertical scrollbar (which then scrolls src_y, not the content). */
uint32_t kdbg_setc_pid,kdbg_setc_handle,kdbg_setc_h,kdbg_setc_found;
void kernel_wm_set_content(uint32_t pid,uint16_t handle,uint16_t content_h)
{
    unsigned i;int sl=app_surface_find(pid,handle);uint8_t *surf=sl<0?0:app_surface[sl];
    if(!surf){
        /* No surface yet: the window is created on the first draw.  Remember
           the declared height so kernel_wm_app_surface can apply it. */
        int s=wm_window_slot(pid,handle);
        if(s<(int)WM_APP_WINDOWS&&wm_window_req[s].set){
            wm_window_req[s].content_h=content_h;wm_window_req[s].content_set=1u;}
        return;
    }
    kdbg_setc_pid=pid;kdbg_setc_handle=handle;kdbg_setc_h=(uint32_t)content_h;kdbg_setc_found=0u;
    for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surf){
        kdbg_setc_found=1u;
        wm.win[i].content_h=content_h;
        if(wm.win[i].src_y>(int16_t)sb_range(i))wm.win[i].src_y=(int16_t)sb_range(i);
        wm_damage_add(&wm,wm.win[i].frame);return;}
}
/* Offset przewiniecia okna aplikacji (src_y), zeby klient mogl mapowac
   klikniecia na wiersze, gdy rysuje CALA liste w powierzchni, a przewija WM. */
uint16_t kernel_wm_get_scroll(uint32_t pid,uint16_t handle)
{
    int sl=app_surface_find(pid,handle);uint8_t *surf=sl<0?0:app_surface[sl];
    unsigned i;
    if(!surf)return 0;
    for(i=0;i<wm.count;++i)
        if(wm.win[i].pid==(uint32_t)(uintptr_t)surf)return (uint16_t)wm.win[i].src_y;
    return 0;
}
/* Input coordinates must follow a dragged client's actual WM rectangle. */
int kernel_wm_client_origin(uint8_t *surface,int32_t *x,int32_t *y)
{
    unsigned i;
    if(!wm_ready||!surface||!x||!y)return 0;
    for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surface){
        *x=wm.win[i].client.x;*y=wm.win[i].client.y;return 1;
    }
    return 0;
}
void kernel_wm_app_dirty(uint8_t *surface,int32_t l,int32_t t,int32_t r,int32_t b)
{
    unsigned i,j;wm_rect_t d,c;
    if(!wm_ready||!surface)return;
    for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surface){
        /* Niewidoczne/zwinięte okno nie ma czego składać.  Dla widocznego
           okna damage jest przycinany do wm.clip[] - czyli do faktycznie
           widocznej części (okna wyżej, ekran, inna sesja). */
        if(!(wm.win[i].flags&1u)||(wm.win[i].flags&8u))return;
        if(l>=r||t>=b)d=wm.win[i].client;
        else{
            /* wspolrzedne powierzchni -> ekran, z przewinieciem WM */
            d.x=(int16_t)(wm.win[i].client.x+(int16_t)(l-wm.win[i].src_x));
            d.y=(int16_t)(wm.win[i].client.y+(int16_t)(t-wm.win[i].src_y));
            d.w=(uint16_t)(r-l);d.h=(uint16_t)(b-t);
        }
        for(j=0;j<wm.clip_count[i];++j)
            if(wm_rect_intersect(&d,&wm.clip[i][j],&c))wm_damage_add(&wm,c);
        return;}
}
void kernel_wm_damage_all(void)
{
    wm_rect_t r;
    if(!wm_ready)return;
    r.x=0;r.y=0;r.w=(uint16_t)wm.screen_w;r.h=(uint16_t)wm.screen_h;
    wm_damage_add(&wm,r);
}

/* Live video-mode change: the compositor surface geometry follows the new mode,
   the shell window is re-capped and the whole desktop is damaged so the next
   frame repaints at the new pitch.  Backing buffers already have the maximum
   size, so nothing is reallocated here. */
/* Growing a live mode must not turn existing windows into out-of-bounds
   backing stores. Allocations may grow harmlessly even if the driver refuses. */
int kernel_wm_prepare_mode(uint16_t width,uint16_t height,uint8_t bpp)
{
    uint32_t bytes=(uint32_t)(bpp==1u?(width+7u)/8u:width)*height;
    unsigned i,k;
    if(!wm_ready)return 0;
    for(i=0;i<=WM_APP_WINDOWS;++i){
        uint8_t **slot=i==WM_APP_WINDOWS?&console_surface:&app_surface[i];
        uint32_t *size=i==WM_APP_WINDOWS?&console_surface_bytes:&app_surface_bytes[i];
        uint8_t *fresh,*old=*slot;
        if(!old||*size>=bytes)continue;
        fresh=kmem_alloc(bytes,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
        if(!fresh)return -1;
        for(k=0;k<*size;++k)fresh[k]=old[k];
        for(;k<bytes;++k)fresh[k]=0;
        for(k=0;k<wm.count;++k)if(wm.win[k].pid==(uint32_t)(uintptr_t)old)
            wm.win[k].pid=(uint32_t)(uintptr_t)fresh;
        for(k=0;k<WM_SURFACE_MAX;++k)if(wm_surface[k]==old)wm_surface[k]=fresh;
        *slot=fresh;*size=bytes;kmem_free(old,KMEM_OWNER_KERNEL);
    }
    return 0;
}
/* Change stride in place without losing the retained client image. New pixels
   are blank. Clients get VCHG to relayout; legacy clients retain valid pixels. */
void kernel_wm_reconfigure(void)
{
    const kgfx_info_t *g;
    unsigned i;
    if(!wm_ready)return;
    g=kgfx_get_info();
    {unsigned pitch=g->line_bytes,w=g->width,h=g->height,bpp=g->mode==8u?8u:1u;
     for(i=0;i<WM_APP_WINDOWS;++i)if(app_surface_used[i])
         repitch_surface(app_surface[i],wm_surface_pitch,wm_surface_w,wm_surface_h,wm_bpp,pitch,w,h,bpp);
     repitch_surface(console_surface,wm_surface_pitch,wm_surface_w,wm_surface_h,wm_bpp,pitch,w,h,bpp);}
    wm_surface_w=(uint16_t)(g&&g->width?g->width:640u);
    wm_surface_h=(uint16_t)(g&&g->height?g->height:400u);
    wm_surface_pitch=(uint16_t)(g&&g->line_bytes?g->line_bytes:
                                (uint16_t)((wm_surface_w+7u)/8u));
    wm.screen_w=(int16_t)wm_surface_w;
    wm.screen_h=(int16_t)wm_surface_h;
    for(i=0;i<wm.count;++i){
        if(console_registered&&console_surface&&
           wm.win[i].pid==(uint32_t)(uintptr_t)console_surface){
            wm.win[i].frame.w=CONSOLE_WW;wm.win[i].frame.h=CONSOLE_WH;
            wm.win[i].client.w=CONSOLE_WW;
            wm.win[i].client.h=(uint16_t)(CONSOLE_WH-16);
            (void)console_fit(i);
        }else if(wm.win[i].flags==7u){
            /* Full-screen application canvas (the desktop background).  It was
               registered for the old mode; grow it so the compositor does not
               leave a black band outside the previous 640x400. */
            wm.win[i].frame.x=0;wm.win[i].frame.y=0;
            wm.win[i].frame.w=wm_surface_w;wm.win[i].frame.h=wm_surface_h;
            wm.win[i].client.x=0;wm.win[i].client.y=0;
            wm.win[i].client.w=wm_surface_w;wm.win[i].client.h=wm_surface_h;
        }else if(wm.win[i].flags==1u){
            /* A normal application window.  Keep it on the new screen and tell
               its owner when the frame changed, so a windowed client (AC) can
               re-lay itself out for the new resolution; a client that ignores
               the notification is at least not left hanging off-screen. */
            /* Wtlocenie na ekran: najpierw rozmiar, potem PRZESUNIECIE okna.
               Poprzednia wersja liczyla "screen_w - x", co dla okna calkiem
               poza ekranem dawalo wartosc ujemna rzutowana na uint16_t, czyli
               ogromny rozmiar (audyt r1).  Matematyka jest w include/wm_clamp.h,
               wiec ten sam kod sprawdza test hostowy. */
            {wm_clamp_rect_t cr;
             cr.x=wm.win[i].frame.x;cr.y=wm.win[i].frame.y;
             cr.w=wm.win[i].frame.w;cr.h=wm.win[i].frame.h;
             if(wm_clamp_rect(&cr,wm.screen_w,wm.screen_h,64,50)){
                 wm.win[i].frame.x=(int16_t)cr.x;wm.win[i].frame.y=(int16_t)cr.y;
                 wm.win[i].frame.w=(uint16_t)cr.w;wm.win[i].frame.h=(uint16_t)cr.h;
                 wm.win[i].client.x=(int16_t)cr.x;
                 wm.win[i].client.y=(int16_t)(cr.y+16);
                 wm.win[i].client.w=(uint16_t)cr.w;
                 wm.win[i].client.h=(uint16_t)(cr.h-16);
                 notify_geometry(i);
             }}
        }
    }
    {extern void kernel_user_notify_display(uint32_t);
     unsigned j;
     for(i=0;i<WM_APP_WINDOWS;++i)if(app_surface_used[i]){
         for(j=0;j<i;++j)if(app_surface_used[j]&&app_surface_pid[j]==app_surface_pid[i])break;
         if(j==i)kernel_user_notify_display(app_surface_pid[i]);
     }}
    wm_framebuffer=wm_get_fb(&wm_pitch,&wm_bpp);
    wm_bg_done=0;
    wm_rebuild_clip(&wm);
    kernel_wm_damage_all();
    wm_force_compose=1;
}

int kernel_wm_console_visible(void){return (wm_ready&&console_registered&&!console_hidden)?1:0;}
/* True when the last click landed on the shell window: the keyboard then feeds
   the shell instead of the desktop application. */
int kernel_wm_console_focused(void){return console_focused!=0u;}
/* Desktop mode: hide the shell's window so the desktop itself is visible.
   Taskbar/menu opens a new terminal (tts2) when needed. */
int kernel_wm_desktop_session(void){extern unsigned ksessions_current(void);return (wm_desktop_session!=0xFFu && ksessions_current()==(unsigned)wm_desktop_session)?1:0;}
void kernel_wm_set_desktop_session(int session){wm_desktop_session=(uint8_t)(session<0?0xFF:session);}
void kernel_wm_console_hide(void)
{
    extern void kgfx_fill_rect(int32_t,int32_t,int32_t,int32_t,uint8_t);
    console_hidden=1;console_registered=0;
    /* Hide only the CONSOLE window.  It is not always win[0]: in desktop mode
       the application can register its window first, and moving win[0] then
       sent the APPLICATION to x=-2048 (its content vanished) while the console
       stayed on screen.  Match the console surface instead of the index. */
    {unsigned i;for(i=0;i<wm.count;++i)
        if(console_surface&&wm.win[i].pid==(uint32_t)(uintptr_t)console_surface){
            wm.win[i].flags=1;wm.win[i].frame.x=-2048;wm.win[i].client.x=-2048;}}
    wm_surface[0]=0;
    /* The compositor only paints windows, so with no window nothing would be
       drawn and the previous framebuffer content stayed on screen. Paint the
       desktop background explicitly. */
    if(wm_ready)kgfx_fill_rect(0,0,(int32_t)wm.screen_w,(int32_t)wm.screen_h,0u);
    kernel_wm_damage_all();
}
/* Scroll the shell window so the output cursor row is visible, the way a
   terminal follows its cursor.  The view used to be pinned to the bottom rows
   of the 25-row grid; after "OPEN TERMINAL" clears the console the cursor is at
   the TOP, so the prompt and everything typed landed above the visible part
   ("the terminal does not take the keyboard").  Only a MOVED cursor scrolls the
   view (force=1 on show), so the scrollbar can still be used to look back. */
static void console_follow_cursor(unsigned i,int force)
{
    static unsigned last_cur=0xFFFFu;
    unsigned rows=(unsigned)(wm.win[i].client.h>2u?wm.win[i].client.h-2u:0u)/16u,cur=kconsole_cursor_row(),first=0u;
    if(cur>=TERM_ROWS)cur=TERM_ROWS-1u;
    if(!force&&cur==last_cur)return;
    last_cur=cur;
    if(!rows)rows=1u;
    if(cur+1u>rows)first=cur+1u-rows;
    if(rows<TERM_ROWS&&first>TERM_ROWS-rows)first=TERM_ROWS-rows;
    if(rows>=TERM_ROWS)first=0u;
    if((uint16_t)wm.win[i].src_y!=(uint16_t)(first*16u)){
        wm.win[i].src_y=(int16_t)(first*16u);
        wm_damage_add(&wm,wm.win[i].client);
    }
}
/* Fit the shell window to its frame.  The shell draws a fixed 80x25 grid of
   8x16 cells (640x400); the window can show a part of it, never more.
     * the frame is clamped to the grid plus the chrome (title 16, bottom 2), so
       a zoom or an enlarge never exposes surface pixels outside the grid (the
       desktop "leaking" through the terminal after zoom/restore, section 41),
     * the frame height is snapped to whole text rows, so the last visible row
       is not cut by the bottom border,
     * the client follows the frame (wm_resize() only ever shrank it),
     * content_h is the grid height, so the scrollbar can look back,
     * the view re-follows the cursor (the prompt stays visible).
   Returns 1 when window i is the shell window. */
#define CON_GRID_W 640u
#define CON_GRID_H ((uint16_t)(TERM_ROWS*16u))
#define CON_PAD 2u
/* Diagnostyka paska terminala: ostatnie wartosci, ktore decyduja o widocznosci
   paska (content_h > client.h) oraz wynik sb_h.  Czytane przez memdump, gdy
   uzytkownik zglosi "pasek znika po zmianie rozmiaru". */
uint32_t kdbg_sb_content,kdbg_sb_client,kdbg_sb_h,kdbg_sb_vis;
static int console_fit(unsigned i)
{
    wm_rect_t old;uint16_t fw,fh,rows;
    if(i>=wm.count||!console_surface||wm.win[i].pid!=(uint32_t)(uintptr_t)console_surface)return 0;
    old=wm.win[i].frame;
    fw=wm.win[i].frame.w;fh=wm.win[i].frame.h;
    if(fw>CON_GRID_W)fw=CON_GRID_W;
    if(fw<64u)fw=64u;
    rows=(uint16_t)(fh>16u+CON_PAD?(fh-16u-CON_PAD)/16u:1u);
    if(rows<1u)rows=1u;
    if(rows>TERM_ROWS)rows=TERM_ROWS;
    fh=(uint16_t)(16u+rows*16u+CON_PAD);
    wm.win[i].frame.w=fw;wm.win[i].frame.h=fh;
    if(wm.win[i].flags&16u){
        /* zoomed: centre the grid-sized window horizontally under the bar */
        int16_t x=(int16_t)(((int)wm.screen_w-(int)fw)/2);
        wm.win[i].frame.x=x>0?x:0;
    }
    wm.win[i].client.x=wm.win[i].frame.x;
    wm.win[i].client.y=(int16_t)(wm.win[i].frame.y+16);
    wm.win[i].client.w=fw;
    wm.win[i].client.h=(uint16_t)(fh-16u);
    wm.win[i].content_h=(uint16_t)(CON_GRID_H+CON_PAD);
    wm.win[i].src_x=0;
    if(wm.win[i].src_y>(int16_t)sb_range(i))wm.win[i].src_y=(int16_t)sb_range(i);
    console_follow_cursor(i,1);
    kdbg_sb_content=(uint32_t)wm.win[i].content_h;
    kdbg_sb_client=(uint32_t)wm.win[i].client.h;
    kdbg_sb_h=(uint32_t)sb_h(i);
    kdbg_sb_vis=(uint32_t)sb_visible(i);
    wm_damage_add(&wm,old);
    wm_damage_add(&wm,wm.win[i].frame);
    wm_rebuild_clip(&wm);
    return 1;
}
/* "Open terminal": bring the existing shell window back instead of appending a
   second one.  kernel_wm_console_hide() parked it off-screen at x=-2048, so the
   window (matched by its backing surface) is moved back and re-raised. */
void kernel_wm_console_show(void)
{
    unsigned i;
    console_hidden=0;
    if(!wm_ready||!console_surface)return;
    for(i=0;i<wm.count;++i)
        if(wm.win[i].pid==(uint32_t)(uintptr_t)console_surface){
            wm.win[i].flags=1;
            wm.win[i].frame.x=CONSOLE_WX;wm.win[i].frame.y=CONSOLE_WY;
            wm.win[i].frame.w=CONSOLE_WW;wm.win[i].frame.h=CONSOLE_WH;
            wm.win[i].client.x=CONSOLE_WX;wm.win[i].client.y=(int16_t)(CONSOLE_WY+16);
            wm.win[i].client.w=CONSOLE_WW;wm.win[i].client.h=(uint16_t)(CONSOLE_WH-16);
            wm.win[i].src_x=0;
            (void)console_fit(i);
            wm_surface[0]=console_surface;
            console_registered=1;
            wm_rebuild_clip(&wm);
            wm_damage_add(&wm,wm.win[i].frame);
            kgfx_console_force_redraw();
            console_raise();
            return;}
    /* No shell window registered yet: let the frame loop create it. */
    console_registered=0;
}
/* Give the shell the keyboard (the desktop keeps the screen).  A click on the
   desktop background takes it back through the hit test in frame_tick. */
void kernel_wm_console_focus(void){console_focused=1u;}

/* Keep the shell window above application surfaces.  Window and backing-store
   indices are independent: glue_blit matches a window to its surface by the pid
   pointer, so raising reorders the Z list only. */
static void console_raise(void)
{
    unsigned i;
    if(!console_registered||!console_surface)return;
    for(i=0;i<wm.count;++i)
        if(wm.win[i].pid==(uint32_t)(uintptr_t)console_surface){
            if(i+1u!=wm.count){wm_raise(&wm,i);wm_damage_add(&wm,wm.win[wm.count-1u].frame);}
            return;}
}
/* Wake the window owned by a pid: un-minimise, raise and focus it.  This is
   what the desktop launcher uses so clicking a running application's button
   restores it instead of starting a second copy (a second copy of a GUI app
   used to fight over the keyboard/screen and look like a hang). */
/* 0 = the pid owns no window, 1 = visible, 2 = minimised.  Lets the desktop
   launcher colour its buttons per application state. */
int kernel_wm_window_state(uint32_t pid)
{
    unsigned i;
    if(!pid)return 0;
    for(i=0;i<wm.count;++i){
        if(window_owner_pid(i)!=pid)continue;
        if(wm.win[i].flags&4u)continue;
        return (wm.win[i].flags&8u)?2:1;
    }
    return 0;
}
int kernel_wm_activate_pid(uint32_t pid)
{
    unsigned i;
    if(!pid)return 0;
    for(i=0;i<wm.count;++i){
        if(window_owner_pid(i)!=pid)continue;
        if(wm.win[i].flags&4u)continue;              /* never the background */
        if(wm.win[i].flags&8u)window_restore(i);     /* un-minimise */
        else {wm_damage_add(&wm,wm.win[i].frame);wm_raise(&wm,i);}
        wm_force_compose=1;
        kernel_gui_focus_set(pid);
        {extern void kernel_user_notify_focus(uint32_t);kernel_user_notify_focus(pid);}
        return 1;
    }
    return 0;
}
void kernel_wm_console_present(void)
{
    uint16_t dx=0,dy=0,dw=0,dh=0;
    wm_rect_t r;
    if(!wm_ready)return;
    if(console_hidden){
        /* The console was hidden BEFORE the WM was ready, so the damage
           registered then was dropped (wm_ready was false). Register it now
           once, otherwise the compositor never runs (it only composites on
           damage) and the desktop is never painted. */
        static uint8_t painted;
        if(!painted){kernel_wm_damage_all();painted=1u;}
        return;   /* desktop mode: no shell window */
    }
    /* Keep the full 80x25 grid for now: narrowing it wipes the shell's cells
       and the prompt only returns on the shell's own refresh, which looked
       broken. Text wrapping comes back once prompt refresh is coordinated. */
    /* Keep the shell's native 80x25 grid: shrinking it used to clear the whole
       console.  The window shows the BOTTOM 18 rows (see src_y below), exactly
       like a terminal that scrolls to the cursor. */
    /* Jak powierzchnie okien: 8 bpp to 480000 B, wiec TT-RAM (wm_surface_prefer_tt). */
    if(!console_surface){console_surface=kmem_alloc(wm_surface_bytes(),16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,wm_surface_prefer_tt());if(!console_surface)return;console_surface_bytes=wm_surface_bytes();}
    if(!kgfx_console_render(kconsole_cells(ksessions_current()),console_surface,&dx,&dy,&dw,&dh))return;
    if(!console_registered){                     /* register once, not per flush */
        /* Append at the current end instead of resetting the whole WM state:
           an application window can already be registered before the first
           console present, and wm_init() here used to wipe it. */
        unsigned w=wm.count;
        if(w>=WM_MAX)return;
        wm_surface[0]=console_surface;
        /* The shell grid is always 80x25 cells of 8x16 pixels.  In mono the
           bitmap is the fixed 640x400 / 80 bytes-per-row block; in 8 bpp each
           pixel is a byte, so the surface follows the framebuffer pitch. */
        if(wm_bpp==8u){
            const kgfx_info_t *g=kgfx_get_info();
            wm_surface_pitch=(uint16_t)(g&&g->line_bytes?g->line_bytes:800u);
            wm_surface_w=(uint16_t)(g&&g->width?g->width:800u);
            wm_surface_h=(uint16_t)(g&&g->height?g->height:600u);
        }else{
            wm_surface_pitch=80;wm_surface_w=640;wm_surface_h=400;
        }
        wm.win[w].frame.x=CONSOLE_WX;wm.win[w].frame.y=CONSOLE_WY;
        wm.win[w].frame.w=CONSOLE_WW;wm.win[w].frame.h=CONSOLE_WH;
        /* Client is the text area only: the title bar must not overlap it. */
        wm.win[w].client.x=wm.win[w].frame.x;
        wm.win[w].client.y=(int16_t)(wm.win[w].frame.y+16);
        wm.win[w].client.w=wm.win[w].frame.w;
        wm.win[w].client.h=(uint16_t)(wm.win[w].frame.h-16);
        wm.win[w].src_x=0;
        /* Show the rows around the cursor: the prompt lives at the bottom, so
           anchor near the end of the 25-row grid. */
        {uint16_t rows=(uint16_t)((CONSOLE_WH-16u)/16u);
         wm.win[w].src_y=(uint16_t)((rows<TERM_ROWS?(TERM_ROWS-rows):0u)*16u);}
        wm.win[w].pid=(uint32_t)(uintptr_t)console_surface;wm.win[w].flags=1;
        wm.win[w].session=0;   /* the shell lives in the first session */
        {unsigned k;
         wm.win[w].title[0]='T';wm.win[w].title[1]='E';wm.win[w].title[2]='R';
         wm.win[w].title[3]='M';wm.win[w].title[4]='I';wm.win[w].title[5]='N';
         wm.win[w].title[6]='A';wm.win[w].title[7]='L';wm.win[w].title[8]=0;
         for(k=9;k<24u;++k){wm.win[w].title[k]=0;}}
        wm.count=w+1u;
        console_registered=1;
        (void)console_fit(w);             /* rows snapped, scroll range, cursor */
        wm_rebuild_clip(&wm);
        wm_damage_add(&wm,wm.win[w].frame);
        console_raise();
    }else{
        unsigned k;
        for(k=0;k<wm.count;++k)
            if(wm.win[k].pid==(uint32_t)(uintptr_t)console_surface)break;
        if(k<wm.count){
            wm_rect_t band,vis;
            console_follow_cursor(k,0);
            /* The changed band is in SURFACE coordinates; the window shows the
               surface from (src_x,src_y) at its client origin.  Mapping it as
               CONSOLE_WX/WY+d ignored the title bar and the scroll offset, so a
               typed character was damaged outside the visible rows and never
               reached the screen. */
            band.x=(int16_t)(wm.win[k].client.x+(int)dx-(int)wm.win[k].src_x);
            band.y=(int16_t)(wm.win[k].client.y+(int)dy-(int)wm.win[k].src_y);
            band.w=dw;band.h=dh;
            if(dw&&dh&&wm_rect_intersect(&band,&wm.win[k].client,&vis))wm_damage_add(&wm,vis);
        }else{
            r.x=(int16_t)(CONSOLE_WX+dx);r.y=(int16_t)(CONSOLE_WY+dy);r.w=dw;r.h=dh;
            if(r.w&&r.h)wm_damage_add(&wm,r);        /* only the changed band */
        }
    }
    /* No composite here: the frame loop does it once per tick. */
}
