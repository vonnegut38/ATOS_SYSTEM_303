/* PLASMA (demoscene): pelnoekranowa 320x200, 8bpp, klasyczny interferencyjny
   wzor + cykliczna paleta.  Wzór liczymy raz; każda klatka to tylko
   base[i]+phase -> paleta i JEDEN blit do powierzchni okna (ATOS_CALL_BLIT8). */
#include "gui_app.h"
#include "app_abi.h"
#include "icons_ids.h"

#define FRAME_COUNTS 492u      /* ~25 fps (12288/492) */
#define W 320u
#define H 200u
#define WW W
#define WH (H+16u)
#define CLIENT_H H

#define KKEY 0x4b4b4559u
#define KMOU 0x4b4d4f55u
#define WMOV 0x574d4f56u
#define WCLS 0x57434c53u
#define WVIS 0x57564953u
#define GFX_RESTORE 6u
#define GFX_DESKTOP 13u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GFX_VIDEO_MODE 30u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static uint8_t quit,fullscreen,chunky;
static int16_t wx=100,wy=80;
static uint16_t scrw=640,scrh=400;

static uint8_t *base,*buf;
static uint8_t pal[256];
static const signed char sin_tab[256]={
    0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46,
    49, 51, 54, 57, 60, 63, 65, 68, 71, 73, 76, 78, 81, 83, 85, 88,
    90, 92, 94, 96, 98, 100, 102, 104, 106, 107, 109, 111, 112, 113, 115, 116,
    117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127,
    127, 127, 127, 127, 126, 126, 126, 125, 125, 124, 123, 122, 122, 121, 120, 118,
    117, 116, 115, 113, 112, 111, 109, 107, 106, 104, 102, 100, 98, 96, 94, 92,
    90, 88, 85, 83, 81, 78, 76, 73, 71, 68, 65, 63, 60, 57, 54, 51,
    49, 46, 43, 40, 37, 34, 31, 28, 25, 22, 19, 16, 12, 9, 6, 3,
    0, -3, -6, -9, -12, -16, -19, -22, -25, -28, -31, -34, -37, -40, -43, -46,
    -49, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
    -90, -92, -94, -96, -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
    -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
    -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
    -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
    -49, -46, -43, -40, -37, -34, -31, -28, -25, -22, -19, -16, -12, -9, -6, -3,
};

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
static void prepare(void)
{
    unsigned x,y;
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        /* Klasyczne interferencje + skladowa radialna: gladkie kontury
           (fale + pierscienie), bez szumu. */
        int dx=(int)x-160,dy=(int)y-100;
        int a=sin_tab[((int)x*2+(int)y*3)&255];
        int b=sin_tab[((int)x*3-(int)y*2+64)&255];
        int c=sin_tab[(((int)x+(int)y)*2)&255];
        int d=sin_tab[((dx*dx+dy*dy)>>7)&255];
        int v=a+b+c+d;                    /* -508..508 */
        v=(v+508)*255/1016;               /* 0..255 */
        base[y*W+x]=(uint8_t)v;
    }
}
/* 16-stopniowa rampa z palety systemowej (0..19): czern -> granat -> blue ->
   jasny blue -> szarosc -> biel -> teal.  Ikony maja wlasna, przypadkowa
   kolejnosc kolorow, wiec uzycie ICON_PAL_BASE dawalo szum zamiast gradientu. */
static const uint8_t ramp[16]={0u,16u,16u,17u,17u,18u,18u,19u,
                               19u,15u,14u,13u,3u,1u,2u,0u};
static void frame(unsigned phase)
{
    unsigned x,y,i;
    for(i=0;i<256u;++i)pal[i]=ramp[(i>>4)&15u];
    for(y=0;y<H;++y){
        unsigned p=(phase+(y*2u))&255u;
        const uint8_t *bp=base+y*W;uint8_t *dp=buf+y*W;
        for(x=0;x<W;++x)dp[x]=pal[(bp[x]+p)&255u];
    }
    (void)gui_call(ATOS_CALL_BLIT8,W,H,buf,0u);
}

int app_main(void)
{
    uint32_t phase=0,last;uint8_t visible=1;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    {int32_t v=gfx(GFX_VIDEO_MODE,0,0,0);
     if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
     chunky=(uint8_t)(gfx(35u,0,0,0)==8u);}
    if(!chunky)return 1;                  /* efekt 8bpp */
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
    base=(uint8_t *)(uintptr_t)gui_call(ATOS_CALL_ALLOC,W*H,1u,0,0);
    buf=(uint8_t *)(uintptr_t)gui_call(ATOS_CALL_ALLOC,W*H,1u,0,0);
    if(!base||!buf)return 1;
    prepare();
    frame(0);
    (void)gfx(GFX_CONTENT,CLIENT_H,0,0);
    last=hires();
    while(!quit){
        unsigned events;
        for(events=0;events<16u;++events){
            if(!pend_pop(&m)&&receive(&m,0))break;
            if(m.sender)continue;
            if(m.v[0]==KKEY&&((m.v[1]>>24)&0xffu)==1u){
                uint32_t scan=m.v[1]&0x7fu;
                if(scan==1u||scan==0x44u)quit=1;
            }else if(m.v[0]==WCLS)quit=1;
            else if(m.v[0]==WVIS){
                unsigned nv=m.v[1]&1u;
                if(nv&&!visible){visible=1;last=hires();frame(phase);}
                else if(!nv)visible=0;
            }else if(m.v[0]==WMOV){wx=(int16_t)(m.v[1]>>16);wy=(int16_t)m.v[1];}
            if(quit)break;
        }
        if(quit)break;
        if(!visible){pause_tick();continue;}
        {uint32_t now=hires(),elapsed=now-last;
         if(elapsed>=FRAME_COUNTS){
             uint32_t steps=elapsed/FRAME_COUNTS;
             last+=steps*FRAME_COUNTS;
             phase=(phase+steps)&255u;
             frame(phase);
             (void)gui_call(ATOS_CALL_SLEEP,0u,0u,0,0);
         }else pause_tick();}
    }
    if(fullscreen){(void)gui_op(GUI_END_KEYS);(void)gfx(GFX_RESTORE,0,0,0);}
    return 0;
}
