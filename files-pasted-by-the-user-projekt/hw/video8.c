#include "hardware.h"
#include "system.h"
#include "console.h"
#include "font.h"
#include "video8.h"

#define V8_IO      0x00d00000UL
#define V8_MEM     0x00c00000UL
/* Tryby VGA karty (8 bpp, liniowo). Odpowiadaja standardowym trybom VGA,
 * ktorych uzywa sterownik RAVEN/ET4000: 0x2F / 0x2E / 0x30 / 0x38. */
typedef struct { uint16_t width,height,stride; uint8_t vdisp,end; } v8_mode_t;
static const v8_mode_t v8_modes[4]={
    { 640, 400, 640, 0x8fu, 0x3eu},   /* 0x2F */
    { 640, 480, 640, 0xdfu, 0x3eu},   /* 0x2E */
    { 800, 600, 800, 0x57u, 0x7cu},   /* 0x30: 599=0x257 -> bit9 (0x07.6), bit8=0 */
    {1024, 768,1024, 0xffu, 0x7cu}    /* 0x38: 767=0x2FF -> bit9 (0x07.6), bit8=0 */
};
#ifndef V8_MODE_DEFAULT
#define V8_MODE_DEFAULT 3   /* test: 640x400 */
#endif
static uint8_t  v8_mode = V8_MODE_DEFAULT;
static uint16_t v8_w = 640, v8_h = 480, v8_stride = 640;

#define V8_W       v8_w
#define V8_H       v8_h
#define V8_STRIDE  v8_stride
#define V8_CELL    16

static volatile uint8_t *const v8_fb = (volatile uint8_t *)(uintptr_t)V8_MEM;

static void v8_reg(uint16_t port,uint8_t value){REG8(V8_IO+port)=value;}
static uint8_t v8_get(uint16_t port){return REG8(V8_IO+port);}
static void v8_seq(uint8_t index,uint8_t value){v8_reg(0x3c4u,index);v8_reg(0x3c5u,value);}
static void v8_crtc(uint8_t index,uint8_t value){v8_reg(0x3d4u,index);v8_reg(0x3d5u,value);}
static uint8_t v8_crtc_get(uint8_t index){v8_reg(0x3d4u,index);return v8_get(0x3d5u);}
static void v8_gc(uint8_t index,uint8_t value){v8_reg(0x3ceu,index);v8_reg(0x3cfu,value);}
static void v8_atc(uint8_t index,uint8_t value){v8_reg(0x3c0u,index);v8_reg(0x3c0u,value);}
static void v8_delay(void){uint32_t i;for(i=0u;i<64u;++i)(void)v8_get(0x3c4u);}

static void v8_palette(void)
{
    /* 16 standard colors, then a gray ramp - enough to see a desktop. */
    static const uint8_t base[16][3]={
        { 0, 0, 0},{ 0, 0,42},{ 0,42, 0},{ 0,42,42},
        {42, 0, 0},{42, 0,42},{42,21, 0},{42,42,42},
        {21,21,21},{21,21,63},{21,63,21},{21,63,63},
        {63,21,21},{63,21,63},{63,63,21},{63,63,63}};
    uint16_t i;
    for(i=0u;i<16u;++i){v8_reg(0x3c8u,(uint8_t)i);v8_reg(0x3c9u,base[i][0]);v8_reg(0x3c9u,base[i][1]);v8_reg(0x3c9u,base[i][2]);}
    v8_reg(0x3c8u,16u);
    for(i=0u;i<240u;++i){uint8_t g=(uint8_t)i;v8_reg(0x3c9u,g);v8_reg(0x3c9u,g);v8_reg(0x3c9u,g);}
}

int video8_is_w32i(void){return g_system.nova_chipset==NOVA_CHIP_ET4000_FAMILY;}

uint8_t video8_mode(void){return v8_mode;}
uint16_t video8_width(void){return v8_w;}
uint16_t video8_height(void){return v8_h;}
uint16_t video8_stride(void){return v8_stride;}

void video8_enter(void)
{
    /* Unlock the Tseng extended registers, then program a 640x480x8 chain-4
     * mode.  CRTC 0x13 counts 32-bit units (doubleword, CRTC 0x14 bit 6):
     * 160 units * 4 = 640 bytes per line. */
    v8_w=v8_modes[v8_mode].width;v8_h=v8_modes[v8_mode].height;
    v8_stride=v8_modes[v8_mode].stride;
    v8_reg(0x3bfu,0x03u);
    v8_seq(0x00u,0x01u);v8_delay();
    v8_seq(0x00u,0x03u);
    v8_seq(0x01u,0x01u);v8_seq(0x02u,0x01u);v8_seq(0x03u,0x00u);
    v8_seq(0x04u,0x0cu);          /* chain-4 -> 8 bpp linear */
    v8_seq(0x06u,0x00u);v8_seq(0x07u,0xf4u);
    v8_reg(0x3c2u,0xe3u);         /* misc: color, H-/V- sync (EmuTOS) */

    v8_crtc(0x11u,0x00u);         /* unlock CRTC */
    v8_crtc(0x00u,0x5fu);v8_crtc(0x01u,0x4fu);v8_crtc(0x02u,0x50u);v8_crtc(0x03u,0x82u);
    v8_crtc(0x04u,0x54u);v8_crtc(0x05u,0x80u);v8_crtc(0x06u,0x0bu);v8_crtc(0x07u,v8_modes[v8_mode].end);
    v8_crtc(0x08u,0x00u);v8_crtc(0x09u,0x40u);v8_crtc(0x0au,0x00u);v8_crtc(0x0bu,0x00u);
    v8_crtc(0x0cu,0x00u);v8_crtc(0x0du,0x00u);v8_crtc(0x0eu,0x00u);v8_crtc(0x0fu,0x00u);
    v8_crtc(0x10u,0xeau);v8_crtc(0x11u,0x0cu);v8_crtc(0x12u,v8_modes[v8_mode].vdisp);
    v8_crtc(0x13u,(uint8_t)(V8_STRIDE/8u)); /* offset: stride/8 (RAVEN/ET4000) */
    v8_crtc(0x14u,0x40u);         /* doubleword addressing */
    v8_crtc(0x15u,0xe7u);v8_crtc(0x16u,0x04u);v8_crtc(0x17u,0xc3u);v8_crtc(0x18u,0xffu);
    {uint8_t t=v8_crtc_get(0x11u);v8_crtc(0x11u,(uint8_t)(t|0x80u));} /* lock CRTC */
    v8_crtc(0x36u,0x53u);         /* ET4000: 8-bit IO select */

    v8_atc(0x10u,0x41u);          /* graphics, 8-bit color */
    v8_atc(0x11u,0x00u);
    v8_gc(0x00u,0x00u);v8_gc(0x01u,0x00u);v8_gc(0x02u,0x00u);v8_gc(0x03u,0x00u);
    v8_gc(0x04u,0x00u);v8_gc(0x05u,0x40u);v8_gc(0x06u,0x05u);v8_gc(0x07u,0x0fu);v8_gc(0x08u,0xffu);

    v8_palette();
    v8_reg(0x3bfu,0x00u);         /* lock extended */

    /* The Hatari bridge shows the card through the ST-high raster, so keep the
     * Shifter in mono; the bridge thresholds the 8 bpp image.  On real hardware
     * the ET4000 drives the monitor in 256 colours directly. */
    REG8(SHIFTER_RES)=0x02u;
    video8_clear(0);
}

void video8_leave(void)
{
    /* Restore the ST-high palette/resolution, then the card text mode. */
    REG16(SHIFTER_PALETTE+0)=0x0000u;
    REG16(SHIFTER_PALETTE+2)=0x0777u;
    REG8(SHIFTER_RES)=0x02u;
    {extern void nova_enter_text_vga(void);nova_enter_text_vga();}
}

void video8_clear(uint8_t color)
{
    uint32_t i;
    for(i=0u;i<(uint32_t)V8_STRIDE*(uint32_t)V8_H;++i)v8_fb[i]=color;
}

/* 1bpp -> 8bpp expansion table: one source byte (8 pixels) becomes 8 bytes
 * (2 longwords), so the inner loop uses 32-bit stores. */
static uint8_t v8_mono_lut[256][8];
static uint8_t v8_lut_ready,v8_lut_fg,v8_lut_bg;

static void v8_build_lut(uint8_t fg,uint8_t bg)
{
    int i,j;
    for(i=0;i<256;i++)for(j=0;j<8;j++)v8_mono_lut[i][j]=(i&(0x80>>j))?fg:bg;
    v8_lut_ready=1;v8_lut_fg=fg;v8_lut_bg=bg;
}

/* Expand a 1bpp mono bitmap into the 8bpp framebuffer (T2 compositor). */
void video8_blit_mono(int x,int y,int w,int h,const uint8_t *source,int stride,
                      uint8_t fg,uint8_t bg)
{
    int j;
    if(!source||w<=0||h<=0)return;
    if(!v8_lut_ready||v8_lut_fg!=fg||v8_lut_bg!=bg)v8_build_lut(fg,bg);
    for(j=0;j<h;j++){
        int yy=y+j,i=0;
        const uint8_t *row=source+(long)j*(long)stride;
        volatile uint8_t *dst;
        if(yy<0||yy>=V8_H)continue;
        dst=v8_fb+(long)yy*(long)V8_STRIDE;
        if(x<0)i=-x;
        /* Aligned, fully visible runs: two longword stores per source byte. */
        while(i+8<=w&&x+i>=0&&x+i+8<=V8_W&&((x+i)&3)==0){
            const uint32_t *l=(const uint32_t *)(const void *)&v8_mono_lut[row[i>>3]][0];
            volatile uint32_t *d=(volatile uint32_t *)(void *)(dst+x+i);
            d[0]=l[0];d[1]=l[1];
            i+=8;
        }
        for(;i<w;i++){
            int xx=x+i;
            if(xx<0)continue;
            if(xx>=V8_W)break;
            dst[xx]=v8_mono_lut[row[i>>3]][i&7];
        }
    }
}

void video8_pixel(int x,int y,uint8_t color)
{
    if(x<0||y<0||x>=V8_W||y>=V8_H)return;
    v8_fb[(uint32_t)y*V8_STRIDE+(uint32_t)x]=color;
}

void video8_fill(int x,int y,int w,int h,uint8_t color)
{
    int j,x0=x,x1=x+w;
    uint32_t word;
    /* Clip once instead of testing the bounds for every single pixel: the
       desktop and console window do hundreds of thousands of these writes. */
    if(x0<0)x0=0;
    if(x1>V8_W)x1=V8_W;
    if(x0>=x1)return;
    /* NOTE: 32-bit wide stores were tried here to cut MegaBus traffic; they
       hung the machine on the ET4000 path, so fills go out as bytes again.
       (A 16-bit variant deserves its own careful test.) */
    (void)word;
    for(j=0;j<h;++j){
        int yy=y+j,i;
        volatile uint8_t *row;
        if(yy<0||yy>=V8_H)continue;
        row=v8_fb+(uint32_t)yy*V8_STRIDE;
        for(i=x0;i<x1;++i)row[i]=(uint8_t)color;
    }
}

void video8_hline(int x,int y,int w,uint8_t color){video8_fill(x,y,w,1,color);}
void video8_vline(int x,int y,int h,uint8_t color){video8_fill(x,y,1,h,color);}

void video8_rect(int x,int y,int w,int h,uint8_t color)
{
    video8_hline(x,y,w,color);
    video8_hline(x,y+h-1,w,color);
    video8_vline(x,y,h,color);
    video8_vline(x+w-1,y,h,color);
}

void video8_text(int x,int y,const char *s,uint8_t fg,uint8_t bg)
{
    int cx=x;
    while(*s){
        const uint8_t *glyph=font_glyph(*s,1u);
        int gy,gx;
        for(gy=0;gy<V8_CELL;++gy){
            int yy=y+gy; if(yy<0||yy>=V8_H)continue;
            volatile uint8_t *row=v8_fb+(uint32_t)yy*V8_STRIDE;
            for(gx=0;gx<8;++gx){
                int xx=cx+gx; if(xx<0||xx>=V8_W)continue;
                row[xx]=(uint8_t)((glyph[gy]&(0x80u>>gx))?fg:bg);
            }
        }
        cx+=8; ++s;
    }
}

void video8_blit(int sx,int sy,int w,int h,int dx,int dy)
{
    /* Simple CPU copy; handles the common non-overlapping case. */
    int i,j;
    for(j=0;j<h;++j){
        int yy=dy+j; if(yy<0||yy>=V8_H)continue;
        volatile uint8_t *dst=v8_fb+(uint32_t)yy*V8_STRIDE;
        int syy=sy+j; if(syy<0||syy>=V8_H)continue;
        volatile uint8_t *src=v8_fb+(uint32_t)syy*V8_STRIDE;
        for(i=0;i<w;++i){int xx=dx+i;if(xx<0||xx>=V8_W)continue;dst[xx]=src[sx+i];}
    }
}

static void v8_wait_vbl(void)
{
    uint32_t guard=100000ul;
    while((REG8(MFP_GPIP)&0x10u)&&guard)--guard;
    guard=100000ul;
    while(!(REG8(MFP_GPIP)&0x10u)&&guard)--guard;
}

static void v8_window(int x,int y)
{
    video8_fill(x,y,300,170,7u);              /* light gray body   */
    video8_fill(x,y,300,18,1u);               /* blue title bar    */
    video8_text(x+6,y+1,"Window 1",15u,1u);
    video8_fill(x+282,y+2,12,14,7u);          /* close box         */
    video8_rect(x,y,300,170,0u);              /* black frame       */
    video8_text(x+10,y+28,"GEM-style window on ET4000",0u,7u);
    video8_text(x+10,y+48,"640x480 x 256 colors",0u,7u);
    video8_text(x+10,y+68,"W32i blitter / CPU fallback",0u,7u);
    video8_text(x+10,y+88,"desktop layer E1",0u,7u);
}

static void v8_cursor(int x,int y)
{
    int i;
    for(i=0;i<12;++i){video8_pixel(x+i,y+i,0u);video8_pixel(x+i+1,y+i,15u);}
}

void video8_demo(void)
{
    video8_enter();
    video8_clear(1u);                              /* blue desktop */
    video8_fill(0,0,V8_W,20,7u);                   /* menu bar     */
    video8_text(6,2,"Desk   File   Edit   View   Options   Help",0u,7u);
    video8_fill(0,20,V8_W,1,0u);
    v8_window(70,80);
    v8_window(360,240);
    v8_cursor(600,440);
    video8_fill(0,460,V8_W,20,1u);
    video8_text(6,462,"E1: ET4000 640x480x8  -  PRESS A KEY",15u,1u);
    while(ikbd_scancode()<0)v8_wait_vbl();
    video8_leave();
}
