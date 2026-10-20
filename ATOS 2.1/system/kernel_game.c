/* Exclusive full-screen game mode - see include/atos_game.h.
 *
 * One process at a time may own the screen of its session.  The module never
 * touches user memory itself: kernel_exception.c validates the client's frame
 * header, rectangle list and pixel buffer against the client's own image /
 * stack / heap blocks and hands this file a kernel copy of the header plus the
 * validated pixel pointer.
 *
 * Drawing goes straight into the PRESENTED frame (the compositor's desktop
 * framebuffer), then kvideo_damage()/kvideo_flush() move the changed words to
 * the card.  While the game owns the screen the compositor, the task bar and
 * the pointer are gated off (kgame_owns_screen()), so nothing paints over it.
 */
#include <stdint.h>
#include "atos_game.h"
#include "kernel_game.h"
#include "kernel_graphics.h"
#include "kernel_theme.h"
#include "kernel_video.h"
#include "kernel_sessions.h"
#include "kernel_wm.h"

static uint32_t owner_pid;
static unsigned owner_session;
static uint8_t lost;              /* the session left the screen since the last frame */
static uint8_t was_desktop;       /* restore the text console on END when 0 */
static uint16_t prev_w,prev_h;
static uint8_t prev_bpp;
static uint8_t palette[256u*3u];  /* the game's palette, re-applied after a session switch */
static uint8_t palette_valid;
uint32_t kgame_dbg_frames,kgame_dbg_rects,kgame_dbg_pixels;

int kgame_active(void){return owner_pid!=0u;}
uint32_t kgame_owner(void){return owner_pid;}
int kgame_owns_screen(void){return owner_pid&&!lost&&ksessions_current()==owner_session;}

static void system_palette(void)
{
    kgfx_palette_defaults();
    (void)kgfx_theme_apply(kgfx_theme_current());
}

static void game_palette(void)
{
    unsigned i;
    if(!palette_valid)return;
    for(i=0;i<256u;++i)
        (void)kvideo_set_palette_rgb((uint8_t)i,palette[i*3u],palette[i*3u+1u],palette[i*3u+2u]);
}

/* Direct scanout: while the game owns the screen its frames go straight from
   the client buffer into the card aperture.  The old route - double into the
   desktop framebuffer, then let kvideo_flush() compare it word by word with the
   shadow and copy the changes - moved every changed pixel five times over the
   slow ST bus (both buffers live in ST-RAM); now it crosses the bus once.  The
   flush gate keeps the compositor's flush off the card meanwhile and forces a
   full rewrite as soon as the game stops owning the screen. */
static volatile uint8_t *card_target(uint16_t *pitch)
{
    return kvideo_card8(pitch);
}

static void clear_screen(void)
{
    uint16_t pitch;uint8_t bpp;uint32_t n,i;
    const kgfx_info_t *g=kgfx_get_info();
    uint8_t *fb=kgfx_present_target(&pitch,&bpp);
    volatile uint8_t *card;uint16_t cpitch;
    if(!fb)return;
    kvideo_select(fb);
    kvideo_set_flush_gate(kgame_owns_screen);
    card=card_target(&cpitch);
    if(card&&kgame_owns_screen()){
        n=((uint32_t)cpitch*(uint32_t)g->height)/4u;
        {volatile uint32_t *d=(volatile uint32_t *)(void *)card;for(i=0;i<n;++i)d[i]=0u;}
        return;
    }
    n=((uint32_t)pitch*(uint32_t)g->height)/4u;
    {volatile uint32_t *d=(volatile uint32_t *)(void *)fb;for(i=0;i<n;++i)d[i]=0u;}
    kvideo_damage(0,0,(int32_t)g->width,(int32_t)g->height);
    kvideo_flush();
}

/* Diagnostics: exact result of the last ATOS_GAME_BEGIN, so a refusal can be
   traced to the specific guard instead of guessed (readable via the debugger). */
int32_t kdbg_game_rc;
int32_t kgame_begin(uint32_t pid,unsigned session,uint16_t width,uint16_t height)
{
#define GAME_RET(v) do{kdbg_game_rc=(int32_t)(v);return (v);}while(0)
    const kgfx_info_t *g;
    if(!pid)GAME_RET(-0x3e0);
    if(owner_pid&&owner_pid!=pid)GAME_RET(-0x3e5);           /* someone else plays */
    if(!kvideo_is_vofa())GAME_RET(-0x3f3);                    /* ET4000 only        */
    g=kgfx_get_info();
    if(!owner_pid){
        prev_w=g->width;prev_h=g->height;prev_bpp=(uint8_t)((g->mode==8u)?8u:1u);
        was_desktop=(uint8_t)kgfx_desktop_active();
        if(!was_desktop&&!kgfx_enter_desktop())GAME_RET(-0x3e4);
    }
    if(width&&height&&(g->width!=width||g->height!=height||g->mode!=8u)){
        int32_t r=kgfx_video_reconfigure_format(width,height,8u);
        if(r<0){
            if(!owner_pid&&!was_desktop)kgfx_restore_console();
            return r;
        }
    }
    g=kgfx_get_info();
    if(g->mode!=8u)GAME_RET(-0x3e3);
    kgfx_cursor_hide();kgfx_overlay_hide();
    owner_pid=pid;owner_session=session;lost=0;
    clear_screen();
    return (int32_t)(((uint32_t)g->width<<16)|(uint32_t)g->height);
}

static void restore(void)
{
    const kgfx_info_t *g=kgfx_get_info();
    owner_pid=0u;lost=0;palette_valid=0;
    if(prev_w&&prev_h&&(g->width!=prev_w||g->height!=prev_h||(g->mode==8u)!=(prev_bpp==8u)))
        (void)kgfx_video_reconfigure_format(prev_w,prev_h,prev_bpp);
    system_palette();
    if(!was_desktop)kgfx_restore_console();
    else{
        kernel_wm_damage_all();
        kernel_wm_refresh_bar();
    }
    kvideo_flush();
}

int32_t kgame_end(uint32_t pid)
{
    if(!owner_pid)return 0;
    if(owner_pid!=pid)return -0x3e5;
    restore();
    return 0;
}

void kgame_release_pid(uint32_t pid)
{
    if(owner_pid&&owner_pid==pid)restore();
}

/* Copy one clipped source rectangle, doubled.  dst points at the destination
   pixel of source (sx,sy); rows advance by two screen lines. */
static void copy2x(uint8_t *dst,uint32_t dpitch,const uint8_t *src,uint32_t spitch,
                   uint32_t w,uint32_t h)
{
    uint32_t y,x;
    if(!(((uintptr_t)dst|dpitch)&1u)){
        for(y=0;y<h;++y){
            volatile uint16_t *d0=(volatile uint16_t *)(void *)dst;
            volatile uint16_t *d1=(volatile uint16_t *)(void *)(dst+dpitch);
            for(x=0;x<w;++x){
                uint16_t v=src[x];v=(uint16_t)(v|(v<<8));
                d0[x]=v;d1[x]=v;
            }
            dst+=dpitch*2u;src+=spitch;
        }
    }else{
        for(y=0;y<h;++y){
            uint8_t *d1=dst+dpitch;
            for(x=0;x<w;++x){uint8_t v=src[x];dst[2u*x]=v;dst[2u*x+1u]=v;d1[2u*x]=v;d1[2u*x+1u]=v;}
            dst+=dpitch*2u;src+=spitch;
        }
    }
}

/* 68k needs only word alignment for long stores; say so (host tests too). */
typedef uint32_t u32_a2 __attribute__((aligned(2)));
/* Doubled copy into the card: two source pixels become one 32-bit store per
   screen row (aabb), i.e. the 68030 issues one long write that the 16-bit bus
   splits into two word cycles.  dst must be even (dst_x is even). */
/* One cached lookup replaces byte-replication shifts in the hot loop.
 * 512 bytes, all 256 palette indices; no allocation and no CPU-specific ISA.
 * Keep this routine out of the large clipping function: inlining previously
 * spilled row pointers and counters to the stack for every pair of pixels. */
static const uint16_t pixel2x[256] = {
    0x0000u,0x0101u,0x0202u,0x0303u,0x0404u,0x0505u,0x0606u,0x0707u,
    0x0808u,0x0909u,0x0a0au,0x0b0bu,0x0c0cu,0x0d0du,0x0e0eu,0x0f0fu,
    0x1010u,0x1111u,0x1212u,0x1313u,0x1414u,0x1515u,0x1616u,0x1717u,
    0x1818u,0x1919u,0x1a1au,0x1b1bu,0x1c1cu,0x1d1du,0x1e1eu,0x1f1fu,
    0x2020u,0x2121u,0x2222u,0x2323u,0x2424u,0x2525u,0x2626u,0x2727u,
    0x2828u,0x2929u,0x2a2au,0x2b2bu,0x2c2cu,0x2d2du,0x2e2eu,0x2f2fu,
    0x3030u,0x3131u,0x3232u,0x3333u,0x3434u,0x3535u,0x3636u,0x3737u,
    0x3838u,0x3939u,0x3a3au,0x3b3bu,0x3c3cu,0x3d3du,0x3e3eu,0x3f3fu,
    0x4040u,0x4141u,0x4242u,0x4343u,0x4444u,0x4545u,0x4646u,0x4747u,
    0x4848u,0x4949u,0x4a4au,0x4b4bu,0x4c4cu,0x4d4du,0x4e4eu,0x4f4fu,
    0x5050u,0x5151u,0x5252u,0x5353u,0x5454u,0x5555u,0x5656u,0x5757u,
    0x5858u,0x5959u,0x5a5au,0x5b5bu,0x5c5cu,0x5d5du,0x5e5eu,0x5f5fu,
    0x6060u,0x6161u,0x6262u,0x6363u,0x6464u,0x6565u,0x6666u,0x6767u,
    0x6868u,0x6969u,0x6a6au,0x6b6bu,0x6c6cu,0x6d6du,0x6e6eu,0x6f6fu,
    0x7070u,0x7171u,0x7272u,0x7373u,0x7474u,0x7575u,0x7676u,0x7777u,
    0x7878u,0x7979u,0x7a7au,0x7b7bu,0x7c7cu,0x7d7du,0x7e7eu,0x7f7fu,
    0x8080u,0x8181u,0x8282u,0x8383u,0x8484u,0x8585u,0x8686u,0x8787u,
    0x8888u,0x8989u,0x8a8au,0x8b8bu,0x8c8cu,0x8d8du,0x8e8eu,0x8f8fu,
    0x9090u,0x9191u,0x9292u,0x9393u,0x9494u,0x9595u,0x9696u,0x9797u,
    0x9898u,0x9999u,0x9a9au,0x9b9bu,0x9c9cu,0x9d9du,0x9e9eu,0x9f9fu,
    0xa0a0u,0xa1a1u,0xa2a2u,0xa3a3u,0xa4a4u,0xa5a5u,0xa6a6u,0xa7a7u,
    0xa8a8u,0xa9a9u,0xaaaau,0xababu,0xacacu,0xadadu,0xaeaeu,0xafafu,
    0xb0b0u,0xb1b1u,0xb2b2u,0xb3b3u,0xb4b4u,0xb5b5u,0xb6b6u,0xb7b7u,
    0xb8b8u,0xb9b9u,0xbabau,0xbbbbu,0xbcbcu,0xbdbdu,0xbebeu,0xbfbfu,
    0xc0c0u,0xc1c1u,0xc2c2u,0xc3c3u,0xc4c4u,0xc5c5u,0xc6c6u,0xc7c7u,
    0xc8c8u,0xc9c9u,0xcacau,0xcbcbu,0xccccu,0xcdcdu,0xceceu,0xcfcfu,
    0xd0d0u,0xd1d1u,0xd2d2u,0xd3d3u,0xd4d4u,0xd5d5u,0xd6d6u,0xd7d7u,
    0xd8d8u,0xd9d9u,0xdadau,0xdbdbu,0xdcdcu,0xddddu,0xdedeu,0xdfdfu,
    0xe0e0u,0xe1e1u,0xe2e2u,0xe3e3u,0xe4e4u,0xe5e5u,0xe6e6u,0xe7e7u,
    0xe8e8u,0xe9e9u,0xeaeau,0xebebu,0xececu,0xededu,0xeeeeu,0xefefu,
    0xf0f0u,0xf1f1u,0xf2f2u,0xf3f3u,0xf4f4u,0xf5f5u,0xf6f6u,0xf7f7u,
    0xf8f8u,0xf9f9u,0xfafau,0xfbfbu,0xfcfcu,0xfdfdu,0xfefeu,0xffffu
};
static inline __attribute__((always_inline)) uint32_t doubled_pair(const uint8_t *s)
{
#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
    return ((uint32_t)pixel2x[s[1]]<<16)|pixel2x[s[0]];
#else
    return ((uint32_t)pixel2x[s[0]]<<16)|pixel2x[s[1]];
#endif
}
static __attribute__((noinline)) void card2x(volatile uint8_t *dst,uint32_t dpitch,
                   const uint8_t *src,uint32_t spitch,uint32_t w,uint32_t h)
{
    for(uint32_t y=0;y<h;++y){
        volatile u32_a2 *d0=(volatile u32_a2 *)(volatile void *)dst;
        volatile u32_a2 *d1=(volatile u32_a2 *)(volatile void *)(dst+dpitch);
        const uint8_t *s=src;uint32_t n=w;
#if defined(__m68k__)
        /* Explicit registers avoid -Os spilling pointers in this bus-bound
         * loop. Word table indexes and byte source reads work on 68000 too,
         * including odd source addresses. Destination is word aligned. */
        if(n>=2u){
            uint32_t pairs=n/2u, count=pairs-1u, index, value;
            __asm__ volatile (
                "1: moveq #0,%[index]\n\t"
                "move.b (%[src])+,%[index]\n\t"
                "add.w %[index],%[index]\n\t"
                "move.w (%[table],%[index].w),%[value]\n\t"
                "swap %[value]\n\t"
                "moveq #0,%[index]\n\t"
                "move.b (%[src])+,%[index]\n\t"
                "add.w %[index],%[index]\n\t"
                "move.w (%[table],%[index].w),%[value]\n\t"
                "move.l %[value],(%[dst0])+\n\t"
                "move.l %[value],(%[dst1])+\n\t"
                "dbra %[count],1b"
                : [src] "+a" (s), [dst0] "+a" (d0), [dst1] "+a" (d1),
                  [count] "+d" (count), [index] "=&d" (index), [value] "=&d" (value)
                : [table] "a" (pixel2x)
                : "cc", "memory");
            n-=pairs*2u;
        }
#else
        while(n>=8u){
            uint32_t v=doubled_pair(s); d0[0]=v;d1[0]=v;
            v=doubled_pair(s+2); d0[1]=v;d1[1]=v;
            v=doubled_pair(s+4); d0[2]=v;d1[2]=v;
            v=doubled_pair(s+6); d0[3]=v;d1[3]=v;
            s+=8;d0+=4;d1+=4;n-=8u;
        }
        while(n>=2u){uint32_t v=doubled_pair(s);*d0++=v;*d1++=v;s+=2;n-=2u;}
#endif
        if(n){uint16_t v=pixel2x[*s];
            *(volatile uint16_t *)(volatile void *)d0=v;
            *(volatile uint16_t *)(volatile void *)d1=v;}
        dst+=dpitch*2u;src+=spitch;
    }
}

static void card1x(volatile uint8_t *dst,uint32_t dpitch,const uint8_t *src,uint32_t spitch,
                   uint32_t w,uint32_t h)
{
    uint32_t y,x;
    for(y=0;y<h;++y){for(x=0;x<w;++x)dst[x]=src[x];dst+=dpitch;src+=spitch;}
}

static void copy1x(uint8_t *dst,uint32_t dpitch,const uint8_t *src,uint32_t spitch,
                   uint32_t w,uint32_t h)
{
    uint32_t y,x;
    for(y=0;y<h;++y){for(x=0;x<w;++x)dst[x]=src[x];dst+=dpitch;src+=spitch;}
}

/* Clip [*a,*b) (source units) so that origin+scale*v stays inside [0,limit). */
static void clip_axis(int32_t *a,int32_t *b,int32_t origin,int32_t scale,int32_t limit)
{
    int32_t lo=0,hi;
    if(origin<0)lo=(-origin+scale-1)/scale;
    hi=(limit-origin)/scale;
    if(limit-origin<0)hi=-1;
    if(*a<lo)*a=lo;
    if(*b>hi)*b=hi;
}

static int32_t frame_to_card(const atos_game_frame_t *f,const uint8_t *pixels,
                             volatile uint8_t *card,uint16_t pitch,const kgfx_info_t *g,int32_t result)
{
    unsigned i,count=f->count;int32_t scale=(f->scale==2u)?2:1;
    if(count>ATOS_GAME_RECTS)return -0x3e0;
    kvideo_set_flush_gate(kgame_owns_screen);
    for(i=0;i<(count?count:1u);++i){
        int32_t x0,y0,x1,y1;
        if(count){x0=f->rect[i].x;y0=f->rect[i].y;x1=x0+(int32_t)f->rect[i].w;y1=y0+(int32_t)f->rect[i].h;}
        else{x0=0;y0=0;x1=(int32_t)f->width;y1=(int32_t)f->height;}
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>(int32_t)f->width)x1=(int32_t)f->width;
        if(y1>(int32_t)f->height)y1=(int32_t)f->height;
        clip_axis(&x0,&x1,f->dst_x,scale,(int32_t)g->width);
        clip_axis(&y0,&y1,f->dst_y,scale,(int32_t)g->height);
        if(x1<=x0||y1<=y0)continue;
        {
            int32_t dx=f->dst_x+x0*scale,dy=f->dst_y+y0*scale;
            volatile uint8_t *d=card+(uint32_t)dy*pitch+(uint32_t)dx;
            const uint8_t *s=pixels+(uint32_t)y0*f->pitch+(uint32_t)x0;
            if(scale==2&&!(dx&1))card2x(d,pitch,s,f->pitch,(uint32_t)(x1-x0),(uint32_t)(y1-y0));
            else if(scale==2){
                /* odd column: byte path (never used by FLASHBK, dst_x is even) */
                uint32_t y,x,w=(uint32_t)(x1-x0),h=(uint32_t)(y1-y0);
                for(y=0;y<h;++y){volatile uint8_t *r=d+(uint32_t)(2u*y)*pitch;
                    for(x=0;x<w;++x){uint8_t v=s[y*f->pitch+x];r[2u*x]=v;r[2u*x+1u]=v;r[pitch+2u*x]=v;r[pitch+2u*x+1u]=v;}}
            }else card1x(d,pitch,s,f->pitch,(uint32_t)(x1-x0),(uint32_t)(y1-y0));
            ++kgame_dbg_rects;kgame_dbg_pixels+=(uint32_t)((x1-x0)*(y1-y0));
        }
    }
    ++kgame_dbg_frames;
    return result;
}

int32_t kgame_frame(uint32_t pid,const atos_game_frame_t *f,const uint8_t *pixels)
{
    const kgfx_info_t *g;uint16_t pitch;uint8_t bpp;uint8_t *fb;
    unsigned i,count;int32_t result=ATOS_GAME_FRAME_OK;int32_t scale;
    if(!owner_pid||pid!=owner_pid)return -0x3e5;
    if(ksessions_current()!=owner_session){
        if(!lost){lost=1;system_palette();}
        return ATOS_GAME_FRAME_HIDDEN;
    }
    if(lost){
        /* Back on screen: our palette and a full repaint are needed. */
        lost=0;kgfx_cursor_hide();kgfx_overlay_hide();game_palette();
        clear_screen();result=ATOS_GAME_FRAME_REPAINT;
    }
    g=kgfx_get_info();
    fb=kgfx_present_target(&pitch,&bpp);
    if(!fb||bpp!=8u)return -0x3e3;
    {uint16_t cpitch;volatile uint8_t *card=card_target(&cpitch);
     if(card)return frame_to_card(f,pixels,card,cpitch,g,result);}
    scale=(f->scale==2u)?2:1;
    count=f->count;
    if(count>ATOS_GAME_RECTS)return -0x3e0;
    for(i=0;i<(count?count:1u);++i){
        int32_t x0,y0,x1,y1;
        if(count){x0=f->rect[i].x;y0=f->rect[i].y;x1=x0+(int32_t)f->rect[i].w;y1=y0+(int32_t)f->rect[i].h;}
        else{x0=0;y0=0;x1=(int32_t)f->width;y1=(int32_t)f->height;}
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>(int32_t)f->width)x1=(int32_t)f->width;
        if(y1>(int32_t)f->height)y1=(int32_t)f->height;
        clip_axis(&x0,&x1,f->dst_x,scale,(int32_t)g->width);
        clip_axis(&y0,&y1,f->dst_y,scale,(int32_t)g->height);
        if(x1<=x0||y1<=y0)continue;
        {
            int32_t dx=f->dst_x+x0*scale,dy=f->dst_y+y0*scale;
            uint8_t *d=fb+(uint32_t)dy*pitch+(uint32_t)dx;
            const uint8_t *s=pixels+(uint32_t)y0*f->pitch+(uint32_t)x0;
            if(scale==2)copy2x(d,pitch,s,f->pitch,(uint32_t)(x1-x0),(uint32_t)(y1-y0));
            else copy1x(d,pitch,s,f->pitch,(uint32_t)(x1-x0),(uint32_t)(y1-y0));
            kvideo_damage(dx,dy,(x1-x0)*scale,(y1-y0)*scale);
            ++kgame_dbg_rects;kgame_dbg_pixels+=(uint32_t)((x1-x0)*(y1-y0));
        }
    }
    kvideo_flush();
    ++kgame_dbg_frames;
    return result;
}

int32_t kgame_palette(uint32_t pid,const atos_game_palette_t *p)
{
    unsigned i;
    if(!owner_pid||pid!=owner_pid)return -0x3e5;
    if(p->first>255u||p->count>256u-p->first)return -0x3e0;
    for(i=0;i<p->count;++i){
        unsigned e=p->first+i;
        palette[e*3u]=p->rgb[i*3u];palette[e*3u+1u]=p->rgb[i*3u+1u];palette[e*3u+2u]=p->rgb[i*3u+2u];
    }
    palette_valid=1;
    if(!kgame_owns_screen())return 0;
    for(i=0;i<p->count;++i){
        unsigned e=p->first+i;
        (void)kvideo_set_palette_rgb((uint8_t)e,palette[e*3u],palette[e*3u+1u],palette[e*3u+2u]);
    }
    return 0;
}
