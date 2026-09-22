#include "kernel_wm.h"
#include "kernel_graphics.h"
#include "kernel_console.h"
#include "kernel_sessions.h"
#include "kernel_video.h"
#include "video_driver.h"
#include "desktop_module.h"
#include "wm_core.h"
extern desktop_module_t desktop_mod_clock;
#include "wm_blit.h"

#define WM_SURFACE_MAX 4u

static wm_state_t wm;
static uint8_t wm_ready;
static uint8_t *wm_framebuffer;
static uint16_t wm_pitch;
static uint8_t wm_bpp;
static uint8_t *wm_surface[WM_SURFACE_MAX];
static uint16_t wm_surface_pitch,wm_surface_w,wm_surface_h;

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

/* The core passes the window pid; the glue maps it to the surface pointer. */
static void glue_blit(void *ctx,uint32_t pid,const wm_rect_t *dst,uint16_t sx,uint16_t sy)
{
    wm_fb_t d;wm_fb_t s;uint32_t i;
    (void)ctx;
    if(!wm_ready)return;
    /* Live buffer lookup: the graphics layer switches framebuffers
       (desktop / console / backbuffer), so a cached pointer would make the
       compositor paint into a buffer that is never displayed. */
    {uint16_t lp=wm_pitch;uint8_t lb=wm_bpp;uint8_t *lfb=wm_get_fb(&lp,&lb);
     if(!lfb)return;
     d.data=lfb;d.pitch=lp;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=lb;}
    if(!pid){
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
            s.data=wm_surface[i];s.pitch=wm_surface_pitch;s.w=wm_surface_w;s.h=wm_surface_h;s.bpp=wm_bpp;
            (void)i;
            /* v10: no wm_app_flip on the new path - single top-left convention. */
            wm_copy_rect(&d,&s,dst,sx,sy);
            return;}
}

void kernel_wm_init(void){wm_ready=0;wm_init(&wm,640,400);}

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
void kernel_wm_stats(uint32_t *frames,uint32_t *pixels,uint32_t *cells)
{
    if(frames)*frames=wm_frames;
    if(pixels)*pixels=wm_damage_pixels;
    if(cells)*cells=wm_console_cells;
}
static uint8_t console_registered;
static wm_drag_t wm_drag;
extern int kernel_user_mouse_event(uint32_t *buttons,int32_t *x,int32_t *y);
void kernel_wm_frame(void)
{
    if(!wm_ready||!wm.damage_count)return;
    wm_recompose(&wm,glue_blit,0);
}
static uint8_t console_hidden;   /* desktop mode: shell window not shown */
static uint8_t wm_desktop_session=0xFFu;  /* session number that is a pure desktop (0xFF = none) */
void kernel_wm_frame_tick(uint32_t now)
{
    /* One composite per tick (50 Hz): everything else only appends damage.
       This is the structural guard that keeps the WM from eating the CPU. */
    if(!wm_ready)return;
    /* TOS/desktop mode: no text console at all. The desktop is graphics only
       (background + application windows); the shell lives in the console
       session, reachable from the top switcher. */
    if(kgfx_desktop_active())console_hidden=1;
    /* Desktop mode: force one full paint. The force-paint inside the hidden
       branch of kernel_wm_console_present() never runs, because the guarded
       kconsole_flush() is what calls present(). Without this the compositor
       finds no damage and the screen keeps the stale console image. */
    if(console_hidden){static uint8_t dp;if(!dp){kernel_wm_console_hide();kernel_wm_damage_all();dp=1u;}}
    /* Desktop session: paint the desktop background DIRECTLY every frame.
       wm_recompose only paints background for damaged rects, and with no
       window it produced nothing - the screen kept the console image. */
    if(kernel_wm_desktop_session()){uint8_t *dfb=(uint8_t *)kgfx_framebuffer(&wm_pitch,&wm_bpp);if(dfb){int dy,db;for(dy=0;dy<400;++dy){uint8_t dpat=(dy&1)?0x55u:0xAAu;uint8_t *drow=dfb+(uint32_t)dy*wm_pitch;for(db=0;db<80;++db){if(db>=72&&dy<80)continue;if(drow[db]!=dpat)drow[db]=dpat;}}}}   /* write only what differs: static desktop, no flicker */
    /* Refresh the surface every frame: the graphics layer switches framebuffer
       and bpp (1bpp card vs 8bpp surface), so a value cached at init made the
       decorations and the desktop dither use the wrong path. */
    wm_framebuffer=wm_get_fb(&wm_pitch,&wm_bpp);
    /* v10: draw the applet bar early (Shifter framebuffer is screen memory). */
    {static uint8_t applets_ready;
     if(!applets_ready){desktop_module_register(&desktop_mod_clock);desktop_module_init_all(0);applets_ready=1u;}
     desktop_module_tick_all(now);
     {video_surface_t vs;
      vs.width=(uint16_t)wm.screen_w;vs.height=(uint16_t)wm.screen_h;vs.pitch=wm_pitch;
      vs.format=(uint8_t)((wm_bpp==8u)?VIDEO_FMT_CHUNKY_8BPP:VIDEO_FMT_PLANAR_1BPP);
      vs.pixels=wm_framebuffer;
      desktop_module_draw_all(&vs,(int)wm.screen_w-66,1,64,7);}}
    /* Register the console window as soon as the desktop takes over, even if
       nothing was printed since: otherwise the first full-screen desktop
       paint would leave no console at all. */
    /* Task 4: still composite when the console is unregistered but an
       application window exists - kernel_wm_console_hide() clears
       console_registered while leaving app windows in the list. */
    if(!console_registered&&!wm.count){
        if(kgfx_desktop_active())kernel_wm_console_present();
        return;
    }
    /* Mouse: title drags move the window, body/edges resize it.  Runs even
       when there is no damage, otherwise idle would swallow mouse events. */
    {uint32_t buttons=0;int32_t mx=0,my=0;
     if(kernel_user_mouse_event(&buttons,&mx,&my)>0){
         if(buttons&0x1u){
             if(!wm_drag.active){
                 unsigned hit=wm_hit(&wm,(int16_t)mx,(int16_t)my);
                 if(hit!=WM_NO_WINDOW)wm_raise(&wm,hit);   /* click to focus */
                 wm_drag_begin(&wm_drag,&wm,(int16_t)mx,(int16_t)my);
             }
             else wm_drag_update(&wm_drag,&wm,(int16_t)mx,(int16_t)my);
         }else if(wm_drag.active){
             wm_drag_end(&wm_drag);
         }
     }}
    /* Task 4: desktop mode with application windows - the direct background
       paint above would erase them on the next tick, and windows only append
       damage when they draw.  Force one full composite per tick so the app
       window stays visible. */
    if(kernel_wm_desktop_session()&&wm.count)kernel_wm_damage_all();
    if(!wm.damage_count)return;
    if(now==wm_last_frame)return;
    wm_last_frame=now;
    /* Tell the video layer only the rectangles that actually changed: the
       VOFA flush then touches those scanlines, not the whole screen. */
    {unsigned d;for(d=0;d<wm.damage_count;++d){
        kvideo_damage(wm.damage[d].x,wm.damage[d].y,(int32_t)wm.damage[d].w,(int32_t)wm.damage[d].h);
        wm_damage_pixels+=(uint32_t)wm.damage[d].w*(uint32_t)wm.damage[d].h;}}
    ++wm_frames;
    wm_recompose(&wm,glue_blit,0);
    /* v10: desktop applet modules (clock) on the top bar.  Drawn after window
       composition so it is not erased; the final flush below carries it. */
    {static uint8_t applets_ready;
     if(!applets_ready){desktop_module_register(&desktop_mod_clock);desktop_module_init_all(0);applets_ready=1u;}
     desktop_module_tick_all(now);
     {video_surface_t vs;
      vs.width=(uint16_t)wm.screen_w;vs.height=(uint16_t)wm.screen_h;vs.pitch=wm_pitch;
      vs.format=(uint8_t)((wm_bpp==8u)?VIDEO_FMT_CHUNKY_8BPP:VIDEO_FMT_PLANAR_1BPP);
      vs.pixels=wm_framebuffer;
      desktop_module_draw_all(&vs,(int)wm.screen_w-66,1,64,7);}}
    /* Window decorations: border + title bar. Cheap (a few spans) and drawn
       every frame so they survive the desktop application's own painting. */
    {wm_fb_t d;unsigned i;
     d.data=wm_framebuffer;d.pitch=wm_pitch;d.w=(uint16_t)wm.screen_w;d.h=(uint16_t)wm.screen_h;d.bpp=wm_bpp;
     for(i=0;i<wm.count;++i){
         wm_rect_t r=wm.win[i].frame;wm_rect_t t;int active=(i+1==wm.count);
         if(!(wm.win[i].flags&1u))continue;
         if(wm.win[i].flags&2u)continue;   /* full-screen surface: no title bar/border */
         t.x=r.x;t.y=r.y;t.w=r.w;t.h=16;wm_fill_rect(&d,&t,active?1:0);
         t.x=r.x;t.y=r.y;t.w=r.w;t.h=1;wm_fill_rect(&d,&t,1);
         t.y=(int16_t)(r.y+r.h-1);wm_fill_rect(&d,&t,1);
         t.y=r.y;t.w=1;t.h=r.h;wm_fill_rect(&d,&t,1);
         t.x=(int16_t)(r.x+r.w-1);wm_fill_rect(&d,&t,1);
     }}
    /* Everything drawn this frame (desktop dither, windows, decorations) must
       reach the card: report the full screen and flush LAST. Drawing after a
       flush meant those pixels never reached VRAM. */
    kvideo_damage(0,0,(int32_t)wm.screen_w,(int32_t)wm.screen_h);
    kvideo_flush();
}
/* v10: draw the desktop applet bar directly to the active framebuffer and
   flush, independent of sessions/windows.  Called from the kernel main loop. */
void kernel_wm_applet_bar(uint32_t now)
{
    static uint8_t applets_ready;
    uint16_t pitch=0u;uint8_t bpp=0u;uint8_t *fb;
    video_surface_t vs;
    if(!applets_ready){desktop_module_register(&desktop_mod_clock);desktop_module_init_all(0);applets_ready=1u;}
    fb=kgfx_framebuffer(&pitch,&bpp);
    if(!fb||!pitch)return;
    desktop_module_tick_all(now);
    vs.width=(uint16_t)wm.screen_w;vs.height=(uint16_t)wm.screen_h;vs.pitch=pitch;
    vs.format=(uint8_t)((bpp==8u)?VIDEO_FMT_CHUNKY_8BPP:VIDEO_FMT_PLANAR_1BPP);
    vs.pixels=fb;
    desktop_module_draw_all(&vs,(int)wm.screen_w-66,1,64,7);
    kvideo_damage((int32_t)wm.screen_w-66,1,64,7);
    kvideo_flush();
}

int kernel_wm_enabled(void){return wm_ready?1:0;}
void kernel_wm_debug(int *count,int *flags,int *hidden,int *damage){if(count)*count=(int)wm.count;if(flags)*flags=wm.count>0u?(int)wm.win[0].flags:-1;if(hidden)*hidden=(int)console_hidden;if(damage)*damage=(int)wm.damage_count;}

/* The console as a compositor window: its grid is rendered into a private
   surface and composed like any other window. Inset so the desktop shows. */
static uint8_t console_surface[32000];
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
#define CONSOLE_WY 96
#define CONSOLE_WW 640
#define CONSOLE_WH 288
static uint8_t app_surface[2][32000];
static uint32_t app_surface_pid[2];
static uint8_t app_surface_used[2];
uint8_t *kernel_wm_app_surface(uint32_t pid)
{
    unsigned i,slot;
    if(!wm_ready||!pid)return 0;
    for(i=0;i<2u;++i)if(app_surface_used[i]&&app_surface_pid[i]==pid){slot=i;return app_surface[slot];}
    for(i=0;i<2u;++i)if(!app_surface_used[i])break;
    if(i==2u)return 0;
    slot=i;app_surface_used[i]=1u;app_surface_pid[i]=pid;
    for(i=0;i<32000u;++i)app_surface[slot][i]=0u;
    /* Register the application's window next to the console window. */
    if((console_registered||console_hidden||kgfx_desktop_active())&&wm.count<WM_MAX){
        unsigned w=wm.count;
        wm_surface[1u+slot]=app_surface[slot];
        /* SysInfo (and GEM screen programs in general) lay their UI out for
           the full 640x400 desktop it reports through wind_get(), so give the
           window the whole screen: a 400x294 window clipped the right-hand
           value column and the F-key menu.  The frame carries flags bit 1,
           which tells the decorator to skip the kernel title bar - the app
           draws its own. */
        wm.win[w].frame.x=0;
        wm.win[w].frame.y=0;
        wm.win[w].frame.w=640;wm.win[w].frame.h=400;
        wm.win[w].client.x=0;
        wm.win[w].client.y=0;
        wm.win[w].client.w=640;
        wm.win[w].client.h=400;
        wm.win[w].src_x=0;wm.win[w].src_y=0;
        /* The compositor identifies a window's backing store by this field
           (the console window stores its surface pointer here too). */
        wm.win[w].pid=(uint32_t)(uintptr_t)app_surface[slot];
        wm.win[w].flags=3;   /* visible (bit 0) + no kernel decorations (bit 1) */
        wm.count=w+1u;
        wm_rebuild_clip(&wm);
        wm_damage_add(&wm,wm.win[w].frame);
    }
    return app_surface[slot];
}
void kernel_wm_damage_all(void)
{
    wm_rect_t r;
    if(!wm_ready)return;
    r.x=0;r.y=0;r.w=(uint16_t)wm.screen_w;r.h=(uint16_t)wm.screen_h;
    wm_damage_add(&wm,r);
}

int kernel_wm_console_visible(void){return (wm_ready&&console_registered&&!console_hidden)?1:0;}
/* Desktop mode: hide the shell's window so the desktop itself is visible.
   Taskbar/menu opens a new terminal (tts2) when needed. */
int kernel_wm_desktop_session(void){extern unsigned ksessions_current(void);return (wm_desktop_session!=0xFFu && ksessions_current()==(unsigned)wm_desktop_session)?1:0;}
void kernel_wm_set_desktop_session(int session){wm_desktop_session=(uint8_t)(session<0?0xFF:session);}
void kernel_wm_console_hide(void)
{
    extern void kgfx_fill_rect(int32_t,int32_t,int32_t,int32_t,uint8_t);
    console_hidden=1;console_registered=0;wm_surface[0]=0;if(wm.count>0u){wm.win[0].flags=1;wm.win[0].frame.x=-2048;wm.win[0].client.x=-2048;}/* hide, keep in list so the background still composites */
    /* The compositor only paints windows, so with no window nothing would be
       drawn and the previous framebuffer content stayed on screen. Paint the
       desktop background explicitly. */
    if(wm_ready)kgfx_fill_rect(0,0,(int32_t)wm.screen_w,(int32_t)wm.screen_h,0u);
    kernel_wm_damage_all();
}
void kernel_wm_console_show(void){console_hidden=0;console_registered=0;}
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
    kconsole_set_grid(80u,18u);   /* dopasowane do okna 480x288: pulpit widoczny */
    if(!kgfx_console_render(kconsole_cells(ksessions_current()),console_surface,&dx,&dy,&dw,&dh))return;
    if(!console_registered){                     /* register once, not per flush */
        wm_init(&wm,640,400);
        wm_surface[0]=console_surface;
        wm_surface_pitch=80;wm_surface_w=640;wm_surface_h=400;
        wm.win[0].frame.x=CONSOLE_WX;wm.win[0].frame.y=CONSOLE_WY;
        wm.win[0].frame.w=CONSOLE_WW;wm.win[0].frame.h=CONSOLE_WH;
        /* Client is the text area only: the title bar must not overlap it. */
        wm.win[0].client.x=wm.win[0].frame.x;
        wm.win[0].client.y=(int16_t)(wm.win[0].frame.y+16);
        wm.win[0].client.w=wm.win[0].frame.w;
        wm.win[0].client.h=(uint16_t)(wm.win[0].frame.h-16);
        wm.win[0].src_x=0;
        /* 8x16 grid is 640x400; the small client shows the bottom slice,
           where the prompt and the last output are. */
        wm.win[0].src_y=(int16_t)(400-(CONSOLE_WH-16));
        wm.win[0].pid=(uint32_t)(uintptr_t)console_surface;wm.win[0].flags=1;
        wm.count=1;
        wm_rebuild_clip(&wm);
        wm_damage_add(&wm,wm.win[0].frame);
        console_registered=1;
    }else{
        r.x=(int16_t)(CONSOLE_WX+dx);r.y=(int16_t)(CONSOLE_WY+dy);r.w=dw;r.h=dh;
        if(r.w&&r.h)wm_damage_add(&wm,r);        /* only the changed band */
    }
    /* No composite here: the frame loop does it once per tick. */
}
