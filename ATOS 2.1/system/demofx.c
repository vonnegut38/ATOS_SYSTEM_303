/* ATOS DEMOS: szesc efektow demoscenowych w jednym zrodle.
   PLASMA/TUNNEL: 320x200, wzor rysowany raz + animacja palety (40 fps).
   FLAMES/ROTOZOOM/SCALER/WOBBLE: 160x100, per-pixel BLIT8 (~20 fps). */
#include "gui_app.h"
#include "app_abi.h"
#include "icons_ids.h"

#define W 160u
#define H 100u
#if defined(FX_PLASMA) || defined(FX_TUNNEL)
#define PAL_ANIM 1
#define FRAME_COUNTS 246u
#else
#define PAL_ANIM 0
#define FRAME_COUNTS 615u
#endif
#define WW W
#define WH (H+16u)
#define CLIENT_H H

#define KKEY 0x4b4b4559u
#define WMOV 0x574d4f56u
#define WCLS 0x57434c53u
#define WVIS 0x57564953u
#define GFX_RESTORE 6u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GFX_VIDEO_MODE 30u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static uint8_t quit,visible=1,chunky;
static int16_t wx=40,wy=40;
static uint16_t scrw=640,scrh=400;
static const signed char sin_tab[256] __attribute__((unused))= {
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
static uint8_t buf[W*H];
#if defined(FX_ROTOZOOM)||defined(FX_SCALER)||defined(FX_WOBBLE)
static uint8_t tex[64u*64u];
#endif

static void pend_push(const gui_message_t *s);
static int pend_pop(gui_message_t *d);
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
static gui_message_t pend[16];static unsigned pend_head,pend_tail;
static void pend_push(const gui_message_t *s)
{unsigned j;if(pend_tail-pend_head<16u){gui_message_t *d=&pend[pend_tail++%16];for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}}
static int pend_pop(gui_message_t *d)
{unsigned j;if(pend_head==pend_tail)return 0;{gui_message_t *s=&pend[pend_head++%16];for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}return 1;}
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
static void set_pal(const uint16_t *p)
{
    (void)gui_call(ATOS_CALL_PALETTE,236u,16u,(void *)p,0u);
}
static void restore_palette(void)
{
    uint16_t z[16];unsigned i;for(i=0;i<16u;++i)z[i]=0u;
    set_pal(z);
}
static const uint16_t wheel[16] __attribute__((unused))={
    0xf00u,0xf40u,0xf80u,0xfc0u,0xff0u,0xcf0u,0x8f0u,0x0f0u,
    0x0f8u,0x0ffu,0x0cfu,0x08fu,0x00fu,0x40fu,0x80fu,0xf0fu
};
static const uint16_t firepal[16] __attribute__((unused))={
    0x000u,0x200u,0x400u,0x600u,0x800u,0xa00u,0xc00u,0xe00u,
    0xf20u,0xf40u,0xf60u,0xf80u,0xfa0u,0xfc0u,0xfe0u,0xfffu
};
static const uint16_t bluepal[16] __attribute__((unused))={
    0x000u,0x003u,0x005u,0x007u,0x009u,0x00bu,0x00du,0x00fu,
    0x01fu,0x03fu,0x05fu,0x07fu,0x09fu,0x0bfu,0x0dfu,0x0ffu
};

static unsigned __attribute__((unused)) isqrt_u(unsigned n)
{
    unsigned r=0u,bit=1u<<15;
    while(bit>n)bit>>=2;
    while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
    return r;
}
static unsigned __attribute__((unused)) atan2_8(int y,int x)
{
    int ax=x<0?-x:x,ay=y<0?-y:y;unsigned a;
    if(!ax&&!ay)return 0u;
    if(ax>=ay)a=(unsigned)(32*ay/ax);else a=(unsigned)(64-32*ax/ay);
    if(x<0)a=128u-a;
    if(y<0)a=256u-a;
    return a&255u;
}
static uint32_t rng __attribute__((unused))=1234567u;
static unsigned __attribute__((unused)) rnd(void){rng=rng*1103515245u+12345u;return (rng>>16)&0x7fffu;}

static void blit_frame(void)
{
    (void)gui_call(ATOS_CALL_BLIT8,W,H,buf,0u);
}

#if defined(FX_PLASMA) || defined(FX_TUNNEL)
static void draw_static(void)
{
    unsigned x,y;
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        int dx=(int)x-(int)(W/2),dy=(int)y-(int)(H/2);
        unsigned r=isqrt_u((unsigned)(dx*dx+dy*dy));
        unsigned ang=atan2_8(dy,dx);
#if defined(FX_PLASMA)
        int v=(int)(ang*3u+r*3u)+(sin_tab[(ang*2u+r*5u)&255]>>1);
#else
        int v=(int)(r*7u+ang*2u)+(sin_tab[(r*3u+ang*5u)&255]>>1);
#endif
        v&=255;
        buf[y*W+x]=(uint8_t)(236+(v>>4));
    }
    blit_frame();
}
static void apply_frame(unsigned phase)
{
    uint16_t p[16];unsigned i;
    for(i=0;i<16u;++i)p[i]=wheel[(i+phase)&15u];
    set_pal(p);
}
#else
static void init_effect(void)
{
    unsigned x,y;
#if defined(FX_ROTOZOOM)||defined(FX_SCALER)||defined(FX_WOBBLE)
    for(y=0;y<64u;++y)for(x=0;x<64u;++x){
        int v=sin_tab[(x*5+y*3)&255]+sin_tab[(x*2-y*7+32)&255];
        tex[y*64u+x]=(uint8_t)((v+254)*16/508);
    }
#endif
#if defined(FX_FLAMES)
    for(y=0;y<H;++y)for(x=0;x<W;++x)buf[y*W+x]=0;
    for(x=0;x<W;++x)buf[(H-1)*W+x]=(uint8_t)(12u+(rnd()&3u));
#endif
}
static void apply_frame(unsigned phase)
{
    unsigned x,y;(void)phase;
#if defined(FX_FLAMES)
    for(x=0;x<W;++x)buf[(H-1)*W+x]=(uint8_t)(12u+(rnd()&3u));
    for(y=H-1;y>0;--y){
        for(x=0;x<W;++x){
            unsigned a=buf[y*W+x];
            unsigned l=(x?buf[y*W+x-1]:a),r=(x+1<W?buf[y*W+x+1]:a);
            unsigned d=buf[(y+1<H?y+1:y)*W+x];
            unsigned v=(a+l+r+d)>>2;
            buf[(y-1)*W+x]=(uint8_t)(v>1u?v-1u:0u);
        }
    }
    set_pal(firepal);
#elif defined(FX_ROTOZOOM)
    int co=sin_tab[(phase*3+64)&255],si=sin_tab[(phase*3)&255];
    int zoom=150+((sin_tab[(phase*2)&255]+127)>>2);
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        int dx=(int)x-(int)W/2,dy=(int)y-(int)H/2;
        int tx=(((dx*co-dy*si)*zoom)>>12)+32;
        int ty=(((dx*si+dy*co)*zoom)>>12)+32;
        buf[y*W+x]=tex[((unsigned)ty&63u)*64u+((unsigned)tx&63u)];
    }
    set_pal(bluepal);
#elif defined(FX_SCALER)
    int zoom=90+((sin_tab[(phase*2)&255]+127)>>1);
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        int dx=(int)x-(int)W/2,dy=(int)y-(int)H/2;
        int tx=((dx*zoom)>>8)+32,ty=((dy*zoom)>>8)+32;
        buf[y*W+x]=tex[((unsigned)ty&63u)*64u+((unsigned)tx&63u)];
    }
    set_pal(bluepal);
#else /* WOBBLE */
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        int ox=sin_tab[(y*2+phase)&255]>>4;
        int oy=sin_tab[(x*2-phase)&255]>>4;
        int tx=((int)x+ox)&63,ty=((int)y+oy)&63;
        buf[y*W+x]=tex[(unsigned)ty*64u+(unsigned)tx];
    }
    set_pal(bluepal);
#endif
    blit_frame();
}
#endif

static const char *effect_name(void)
{
#if defined(FX_PLASMA)
    return "PLASMA";
#elif defined(FX_TUNNEL)
    return "TUNNEL";
#elif defined(FX_FLAMES)
    return "FLAMES";
#elif defined(FX_ROTOZOOM)
    return "ROTOZOOM";
#elif defined(FX_SCALER)
    return "SCALER";
#else
    return "WOBBLE";
#endif
}

int app_main(void)
{
    uint32_t phase=0,last;int32_t v;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    (void)gui_op(GUI_BEGIN_KEYS);
    v=gfx(GFX_VIDEO_MODE,0,0,0);
    if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
    chunky=(uint8_t)(gfx(35u,0,0,0)==8u);
    if(!chunky)return 1;
    wx=(int16_t)(((int)scrw-(int)WW)/2);if(wx<8)wx=8;
    wy=(int16_t)(((int)scrh-(int)WH)/2);if(wy<8)wy=8;
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,((uint32_t)WW<<16)|WH,0u);
    (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)effect_name(),0,0);
    (void)gfx(GFX_CONTENT,CLIENT_H,0,0);
#if defined(FX_PLASMA) || defined(FX_TUNNEL)
    draw_static();apply_frame(0);
#else
    init_effect();apply_frame(0);
#endif
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
                if(nv&&!visible){visible=1;last=hires();
#if defined(FX_PLASMA)||defined(FX_TUNNEL)
                    apply_frame(phase);
#else
                    apply_frame(phase);
#endif
                }else if(!nv){visible=0;restore_palette();}
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
             apply_frame(phase);
             (void)gui_call(ATOS_CALL_SLEEP,0u,0u,0u,0u);
         }else pause_tick();}
    }
    restore_palette();
    (void)gui_op(GUI_END_KEYS);
    return 0;
}
