/* PLASMA: timer-driven animation; no input required to advance frames. */
#include "gui_app.h"
#include "app_abi.h"
#include "icons_ids.h"
#define FRAME_COUNTS 246u /* ~50 fps ceiling at 12288 counts/s; skip missed deadlines */
/* The ripple advances with REAL time, not with the frame rate: 15 palette steps
   every 1229 counts is the speed the effect always had, so a higher refresh rate
   makes it smoother instead of faster. */
#define PHASE_COUNTS 1229u
#define PHASE_STEP 15u

#define KKEY 0x4b4b4559u
#define KMOU 0x4b4d4f55u
#define WMOV 0x574d4f56u
#define WCLS 0x57434c53u
#define GFX_RESTORE 6u
#define GFX_DESKTOP 13u
#define GFX_BATCH 18u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GFX_VIDEO_MODE 30u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u
#define GUI_FILL 2u

/* The plasma canvas IS the client area: 26x18 cells of 10x10 pixels = 260x180.
   The canvas size is the ONLY real speed lever: the cost is one write per
   canvas pixel per frame, so a smaller canvas is proportionally faster. */
#define CW 26u
#define CH 18u
#define CELL 10u
#define PW (CW*CELL)        /* 260 */
#define PH (CH*CELL)        /* 180 */
#define WW PW
#define WH (PH+16u)         /* + title bar */
#define CLIENT_H PH

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static gui_draw_t list[80];
static unsigned n;
static uint8_t quit,fullscreen,chunky;
static int16_t wx=100,wy=80;
static uint16_t scrw=640,scrh=400;

static gui_message_t pend[16];static unsigned pend_head,pend_tail;
static void pend_push(const gui_message_t *s)
{unsigned j;if(pend_tail-pend_head<16u){gui_message_t *d=&pend[pend_tail++%16];
 for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}}
static int pend_pop(gui_message_t *d)
{unsigned j;if(pend_head==pend_tail)return 0;
 {gui_message_t *s=&pend[pend_head++%16];for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}
 return 1;}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
static int32_t gui_op(uint32_t op)
{
    if(gui_pid<0)return -1;
    if(send((uint32_t)gui_pid,op,0,0,0))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gui_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
/* BSS clears the unused text/flags once; fills only change geometry/colour. */
static void add_fill(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color)
{
    gui_draw_t *d;
    if(n>=GUI_DRAW_MAX)return;
    d=&list[n++];d->kind=GUI_FILL;d->x=x;d->y=y;
    d->w=w;d->h=h;d->color=color;
}
static void flush(void)
{
    if(!n)return;
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    n=0;
}

static uint8_t base[CH][CW];
static void prepare(void)
{
    unsigned x,y;
    for(y=0;y<CH;++y)for(x=0;x<CW;++x)
        base[y][x]=(uint8_t)(((x*x*3u+y*y*2u)>>4u)%ICON_PAL_COLORS);
}
static uint16_t colour(unsigned x,unsigned y,uint32_t phase)
{
    unsigned v=base[y][x]+phase;
    if(v>=ICON_PAL_COLORS)v-=ICON_PAL_COLORS;
    return chunky?(uint16_t)(ICON_PAL_BASE+v):(uint16_t)((v>>3u)&1u);
}
static void frame(uint32_t phase)
{
    unsigned x,y;
    n=0;
    for(y=0;y<CH;++y)for(x=0;x<CW;){
        unsigned start=x;uint16_t c=colour(x,y,phase);
        while(++x<CW&&colour(x,y,phase)==c){}
        add_fill((int16_t)(start*CELL),(int16_t)(y*CELL),
                 (uint16_t)((x-start)*CELL),CELL,c);
        if(n>=GUI_DRAW_MAX)flush();
    }
    flush();
}

int app_main(void)
{
    uint32_t phase=0,last,phase_acc=0;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    {int32_t v=gfx(GFX_VIDEO_MODE,0,0,0);
     if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
     chunky=(uint8_t)(gfx(35u,0,0,0)==8u);}
    if(gui_op(GUI_BEGIN_KEYS)==0){
        if(gfx(GFX_DESKTOP,0,0,0)==0)fullscreen=1;
        else (void)gui_op(GUI_END_KEYS);
    }
    wx=(int16_t)(((int)scrw-(int)WW)/2);if(wx<8)wx=8;
    wy=(int16_t)(((int)scrh-(int)WH)/2);if(wy<8)wy=8;
    if(!fullscreen){
        (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,((uint32_t)WW<<16)|WH,0u);
        (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"PLASMA",0,0);
    }
    prepare();
    add_fill(0,0,WW,CLIENT_H,0);flush();
    (void)gfx(GFX_CONTENT,CLIENT_H,0,0); /* geometry, not a per-frame operation */
    frame(phase);last=hires();
    while(!quit){
        unsigned events;
        /* Bound input work, so a stream of motion cannot starve animation. */
        for(events=0;events<16u;++events){
            if(!pend_pop(&m)&&receive(&m,0))break;
            if(m.sender)continue;
            if(m.v[0]==KKEY&&((m.v[1]>>24)&0xffu)==1u){
                uint32_t scan=m.v[1]&0x7fu;
                if(scan==1u||scan==0x44u)quit=1;
            }else if(m.v[0]==WCLS)quit=1;
            else if(m.v[0]==WMOV){wx=(int16_t)(m.v[1]>>16);wy=(int16_t)m.v[1];}
            if(quit)break;
        }
        if(quit)break;
        {uint32_t now=hires(),elapsed=now-last;
         if(elapsed>=FRAME_COUNTS){
             uint32_t steps=elapsed/FRAME_COUNTS;
             last+=steps*FRAME_COUNTS;
             phase_acc+=steps*FRAME_COUNTS;
             while(phase_acc>=PHASE_COUNTS){phase_acc-=PHASE_COUNTS;
                 phase=(phase+PHASE_STEP)%ICON_PAL_COLORS;}
             frame(phase);
             (void)gui_call(ATOS_CALL_SLEEP,0u,0u,0,0); /* input and peer apps between frames */
         }else pause_tick();}
    }
    if(fullscreen){(void)gui_op(GUI_END_KEYS);(void)gfx(GFX_RESTORE,0,0,0);}
    return 0;
}
