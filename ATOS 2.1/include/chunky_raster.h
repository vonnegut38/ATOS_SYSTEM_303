#ifndef CHUNKY_RASTER_H
#define CHUNKY_RASTER_H
#include <stdint.h>
/* Native byte order table: no assumption about host versus m68k endianness. */
typedef union { uint32_t word; uint8_t bytes[4]; } chunky_row_t;
static inline void chunky_span(volatile uint8_t *p,uint32_t n,uint8_t color)
{
    uint32_t v=(uint32_t)color*0x01010101u;
    while(n&&((uintptr_t)p&3u)){*p++=color;--n;}
    while(n>=4u){*(volatile uint32_t *)(void *)p=v;p+=4;n-=4;}
    while(n--)*p++=color;
}
static inline void chunky_fill(volatile uint8_t *p,uint32_t pitch,uint32_t w,uint32_t h,uint8_t color)
{while(h--){chunky_span(p,w,color);p+=pitch;}}
static inline void chunky_glyph(volatile uint8_t *p,uint32_t pitch,const uint8_t *glyph,uint8_t fg,uint8_t bg,int opaque)
{
    static chunky_row_t table[16];static uint8_t oldfg,oldbg,valid;
    unsigned r,b,i;
    if(opaque&&(!valid||fg!=oldfg||bg!=oldbg)){
        for(i=0;i<16u;++i)for(b=0;b<4u;++b)table[i].bytes[b]=(i&(0x8u>>b))?fg:bg;
        oldfg=fg;oldbg=bg;valid=1;
    }
    for(r=0;r<16u;++r,p+=pitch){
        if(opaque&&!((uintptr_t)p&3u)){
            ((volatile uint32_t *)(void *)p)[0]=table[glyph[r]>>4].word;
            ((volatile uint32_t *)(void *)p)[1]=table[glyph[r]&15u].word;
        }else for(b=0;b<8u;++b){
            if(glyph[r]&(0x80u>>b))p[b]=fg;else if(opaque)p[b]=bg;
        }
    }
}
/* Coordinates have been clipped by caller. Traverse like memmove. */
static inline void chunky_copy(volatile uint8_t *base,uint32_t pitch,int sx,int sy,int w,int h,int dx,int dy)
{
    int row,start=dy>sy?h-1:0,end=dy>sy?-1:h,step=dy>sy?-1:1,x;
    for(row=start;row!=end;row+=step){
        volatile uint8_t *s=base+(sy+row)*pitch+sx,*d=base+(dy+row)*pitch+dx;
        if(dx>sx)for(x=w;x-- >0;)d[x]=s[x];
        else for(x=0;x<w;++x)d[x]=s[x];
    }
}
#endif
