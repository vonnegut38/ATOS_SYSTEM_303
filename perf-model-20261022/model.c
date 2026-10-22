/* Host model of the ATOS 2.1 composition pipeline for the 22.10 perf scenario.
 * Uses the REAL wm_core.c (damage, clip lists, wm_recompose), wm_blit.c
 * (wm_copy_rect, wm_fill_rect) and kernel_video.c (kvideo_damage, kvideo_flush
 * with shadow compare). Only the frame-tick ORDER is re-stated here, following
 * kernel_wm_frame_tick(): damage -> kvideo_damage -> wm_recompose -> dock_draw
 * -> kvideo_flush. Chrome and cursor (1.3% + 1.2% in the audit) are omitted.
 *
 * Scenario (AUDYT-WYDAJNOSCI-2026-10-22, 800x600 8 bpp, "4 apps, no input"):
 *   desktop canvas (bottom), SYSINFO, TASKMGR, PLASMA, MODPLAY (top).
 *   Geometry from the apps' sources: MODPLAY covers all other windows.
 * Variants are cumulative switches; each prints work volumes per second. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "wm_core.h"
#include "wm_blit.h"
#include "kernel_video.h"

extern uint32_t kdbg_dmg_px,kdbg_flush_words,kdbg_flush_calls,kdbg_scan_words;

#define SW 800
#define SH 600
enum {W_DESK,W_SYSINFO,W_TASKMGR,W_PLASMA,W_MODPLAY,W_N};
static wm_state_t wm;
static uint8_t *surf[W_N];
static uint8_t *front,*card;

/* switches */
static int clip_app_damage;     /* V1: app damage clipped to the window's visible region */
static int plasma_skip_same;    /* V2: PLASMA redraws only when its phase changed */
static int per_cmd_damage;      /* V3: one damage rect per changed meter bar */
static int dock_when_dirty;     /* V4: dock drawn only when its state changed */
static int coalesce_compose;    /* V5: GUI_FRAME marks damage; ONE compose per 20 ms tick */
static int dock_open=1;
static int occluded_pause;      /* V6: a fully covered window gets no frames (WM tells the app) */

static uint64_t m_gfx_px,m_blit_px,m_dock_px,m_composes;

static void glue(void *ctx,uint32_t pid,const wm_rect_t *dst,uint16_t sx,uint16_t sy)
{
    wm_fb_t d,s;unsigned i;(void)ctx;
    d.data=front;d.pitch=SW;d.w=SW;d.h=SH;d.bpp=8;
    if(!pid)return;                       /* wm_background_canvas=1: desktop app paints bg */
    for(i=0;i<W_N;++i)if((uint32_t)(uintptr_t)surf[i]==pid)break;
    if(i==W_N)return;
    s.data=surf[i];s.pitch=SW;s.w=SW;s.h=SH;s.bpp=8;
    m_blit_px+=(uint64_t)dst->w*dst->h;
    wm_copy_rect(&d,&s,dst,sx,sy);
}
static unsigned slot_of(unsigned w){unsigned i;for(i=0;i<wm.count;++i)if(wm.win[i].pid==(uint32_t)(uintptr_t)surf[w])return i;return 0;}

/* kernel_wm_app_dirty(): real code adds the screen rect unclipped (V0). */
static void app_dirty(unsigned w,int l,int t,int r,int b)
{
    unsigned i=slot_of(w),j;wm_rect_t d;
    d.x=(int16_t)(wm.win[i].client.x+l);d.y=(int16_t)(wm.win[i].client.y+t);
    d.w=(uint16_t)(r-l);d.h=(uint16_t)(b-t);
    if(!clip_app_damage){wm_damage_add(&wm,d);return;}
    if(!(wm.win[i].flags&1u)||(wm.win[i].flags&8u))return;
    for(j=0;j<wm.clip_count[i];++j){wm_rect_t c;if(wm_rect_intersect(&d,&wm.clip[i][j],&c))wm_damage_add(&wm,c);}
}
static void surf_fill(unsigned w,int x,int y,int ww,int hh,uint8_t c)
{
    wm_fb_t f;wm_rect_t r;f.data=surf[w];f.pitch=SW;f.w=SW;f.h=SH;f.bpp=8;
    r.x=(int16_t)x;r.y=(int16_t)y;r.w=(uint16_t)ww;r.h=(uint16_t)hh;
    wm_fill_rect(&f,&r,c);m_gfx_px+=(uint64_t)ww*hh;
}
static void dock_draw(void)
{
    wm_fb_t d;wm_rect_t r;int k;
    d.data=front;d.pitch=SW;d.w=SW;d.h=SH;d.bpp=8;
    r.x=8;r.y=SH-24-2;r.w=dock_open?SW-16:24;r.h=24;
    wm_fill_rect(&d,&r,15);m_dock_px+=(uint64_t)r.w*r.h;
    if(dock_open)for(k=0;k<8;++k){wm_rect_t s={(int16_t)(8+22+k*24+4),(int16_t)(r.y+4),16,16};wm_fill_rect(&d,&s,(uint8_t)(40+k));}
    kvideo_damage(r.x,r.y,r.w,r.h);
}
static int dock_dirty=1;
static void compose(void)
{
    unsigned d;
    if(!wm.damage_count){kvideo_flush();return;}
    ++m_composes;
    for(d=0;d<wm.damage_count;++d)kvideo_damage(wm.damage[d].x,wm.damage[d].y,wm.damage[d].w,wm.damage[d].h);
    wm_recompose(&wm,glue,0);
    if(!dock_when_dirty||dock_dirty){dock_draw();dock_dirty=0;}
    kvideo_flush();
}

/* PLASMA: 26x18 cells of 10x10, colour = 20 + (base+phase) % 216 (plasma.c). */
static uint8_t pbase[18][26];
static void plasma_frame(unsigned phase)
{
    unsigned x,y;
    for(y=0;y<18;++y)for(x=0;x<26;){
        unsigned st=x,c=(pbase[y][x]+phase)%216u;
        while(++x<26&&(pbase[y][x]+phase)%216u==c){}
        surf_fill(W_PLASMA,(int)st*10,(int)y*10,(int)(x-st)*10,10,(uint8_t)(20+c));
    }
    app_dirty(W_PLASMA,0,0,260,180);   /* batch bounds = whole canvas */
}
/* MODPLAY meters (modplay.c meters()): bar i at x=20+20i, 14 wide, baseline 406,
   height 0..60; 4 channels play, so bars 0..3 change; one damage per 8-bar group. */
static uint8_t mh[32];
static unsigned rng=12345;static unsigned rnd(void){rng=rng*1103515245u+12345u;return (rng>>16)&0x7fff;}
static void modplay_paint(unsigned second_tick)
{
    unsigned i,g;
    for(g=0;g<4;++g){
        int l=10000,r=-1;
        for(i=g*8;i<g*8+8;++i){
            unsigned h=(i<4)?(rnd()%61):0,old=mh[i];int x=20+20*(int)i;
            if(h==old)continue;
            if(h<old)surf_fill(W_MODPLAY,x,406-(int)old,14,(int)(old-h),0);
            else surf_fill(W_MODPLAY,x,406-(int)h,14,(int)(h-old),19);
            mh[i]=(uint8_t)h;
            if(per_cmd_damage)app_dirty(W_MODPLAY,x,346,x+14,406);
            if(x<l)l=x;
            if(x+14>r)r=x+14;
        }
        if(!per_cmd_damage&&r>l)app_dirty(W_MODPLAY,l,346,r,406);
    }
    if(second_tick){   /* time text "0:06 / 4:13" (19 chars) and progress step */
        surf_fill(W_MODPLAY,24,280,152,16,(uint8_t)(1+(rng&1)));app_dirty(W_MODPLAY,24,280,176,296);
        surf_fill(W_MODPLAY,24+(int)(rng%600),248,3,22,19);app_dirty(W_MODPLAY,24,248,656,270);
    }
}
static void setup(void)
{
    static const wm_rect_t fr[W_N]={{0,0,SW,SH},{100,65,600,470},{160,140,480,320},{270,202,260,196},{50,42,680,496}};
    unsigned i,x,y;
    wm_init(&wm,SW,SH);
    for(i=0;i<W_N;++i){
        if(!surf[i])surf[i]=malloc(SW*SH);
        memset(surf[i],(int)(i*30+5),SW*SH);
        wm.win[i].frame=fr[i];wm.win[i].client=fr[i];
        if(i){wm.win[i].client.y=(int16_t)(fr[i].y+16);wm.win[i].client.h=(uint16_t)(fr[i].h-16);wm.win[i].flags=1;}
        else wm.win[i].flags=7;
        wm.win[i].pid=(uint32_t)(uintptr_t)surf[i];wm.win[i].session=0;wm.win[i].src_x=0;wm.win[i].src_y=0;
    }
    wm.count=W_N;wm_rebuild_clip(&wm);
    for(y=0;y<18;++y)for(x=0;x<26;++x)pbase[y][x]=(uint8_t)(((x*x*3u+y*y*2u)>>4u)%216u);
    memset(mh,0,sizeof mh);rng=12345;
    if(!front)front=malloc(SW*SH);
    if(!card)card=malloc(SW*SH);
    kvideo_set_mode_format(SW,SH,8);kvideo_activate_vofa(card);kvideo_select(front);
    wm_damage_add(&wm,fr[0]);compose();   /* first full frame */
    m_gfx_px=m_blit_px=m_dock_px=m_composes=0;kdbg_dmg_px=kdbg_flush_words=kdbg_scan_words=0;dock_dirty=0;
}
/* 5 s at 20 ms ticks. PLASMA at the measured composite rate (~18 Hz, CPU bound),
   phase step every 100 ms (PHASE_COUNTS 1229 of 12288/s); MODPLAY paints at 25 Hz
   (paint_deadline = 1/25 s) and each paint is a GUI_FRAME -> kernel_wm_compose_now(). */
static void run(const char *name,double plasma_hz)
{
    unsigned tick,phase=0,last_phase_drawn=0xffff;double tp=0,tm=0,t=0;
    setup();
    for(tick=0;tick<250;++tick,t+=0.020){
        unsigned want_phase=(unsigned)(t/0.100)*15u%216u;
        while(tm<=t){modplay_paint(((unsigned)(tm*25.0))%25u==0);if(!coalesce_compose)compose();tm+=1.0/25.0;}
        while(plasma_hz>0&&tp<=t){
            phase=want_phase;
            if(occluded_pause&&wm.clip_count[slot_of(W_PLASMA)]==0){tp+=1.0/plasma_hz;continue;}
            if(!plasma_skip_same||phase!=last_phase_drawn){plasma_frame(phase);last_phase_drawn=phase;}
            tp+=1.0/plasma_hz;
        }
        compose();   /* scheduled frame tick */
    }
    {double wm_b=(2.0*m_blit_px+m_dock_px+2.0*kdbg_scan_words*2.0+kdbg_flush_words*2.0)/5.0; /* WM side: copy r+w, dock w, flush reads front+shadow, shadow w */
     printf("%-38s %5.1f | %7.0f | %8.0f | %8.0f | %7.0f | %8.0f | %6.0f | %8.0f\n",
        name,m_composes/5.0,m_gfx_px/5.0,kdbg_dmg_px/5.0,m_blit_px/5.0,m_dock_px/5.0,kdbg_scan_words*2/5.0,kdbg_flush_words*2/5.0,wm_b);}
}
int main(void)
{
    int d;
    for(d=1;d>=0;--d){
        dock_open=d;printf("== dock %s   [per second: composes | app GFX px | damage px | recompose px | dock px | flush scan B | card B | WM RAM traffic B]\n",d?"OPEN (perf-4apps.png)":"CLOSED (perf-dockclosed.png)");
        clip_app_damage=plasma_skip_same=per_cmd_damage=dock_when_dirty=coalesce_compose=occluded_pause=0;
        run("V0 current code",18);
        clip_app_damage=1;                 run("V1 +clip app damage to visible",18);
        plasma_skip_same=1;                run("V2 +PLASMA skips identical frames",18);
        per_cmd_damage=1;                  run("V3 +one damage rect per command",18);
        dock_when_dirty=1;                 run("V4 +dock drawn only when dirty",18);
        coalesce_compose=1;                run("V5 +one compose per tick",18);
        occluded_pause=1;                  run("V6 +covered window stops animating",18);
        clip_app_damage=plasma_skip_same=per_cmd_damage=dock_when_dirty=coalesce_compose=occluded_pause=0;
        run("ref: MODPLAY alone, current code",0);
    }
    return 0;
}
