/* Differential coverage for the one-plane copy primitives.
   There was no host test for emutos_vdi_mono_copy_words/_shifted; this exercises
   arbitrary sources/destinations (including overlap) against a snapshot-based
   reference, and guards the words around the surface. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "emutos_vdi_mono.h"

#define LW 40              /* words per scanline = 640 px */
#define LH 80              /* scanlines */
#define PAD 2
static uint16_t actual[PAD+LW*LH+PAD], expect[PAD+LW*LH+PAD], snapshot[PAD+LW*LH+PAD];
static unsigned seed=987654321u;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}

static void putpixel(uint16_t *buf,unsigned x,unsigned y,unsigned v)
{
    uint16_t *w=buf+(uint32_t)y*LW+(x>>4);uint16_t bit=(uint16_t)(0x8000u>>(x&15));
    *w=(uint16_t)((*w&~bit)|(v?bit:0));
}
static unsigned getpixel(const uint16_t *buf,unsigned x,unsigned y)
{return (unsigned)((buf[(uint32_t)y*LW+(x>>4)]>>(15-(x&15)))&1u);}

int main(void)
{
    unsigned n;
    for(n=0;n<20000u;n++){
        unsigned sx,sy,w,h,dx,dy,r,c,i;
        int words=(n&1u)==0u;
        if(words){sx=(rnd()%38u)*16u;dx=(rnd()%38u)*16u;w=(1u+rnd()%3u)*16u;
            if(sx+w>640u||dx+w>640u){continue;}}
        else{sx=rnd()%(640u-320u);dx=rnd()%(640u-320u);w=1u+rnd()%320u;}
        h=1u+rnd()%16u;sy=rnd()%(LH-h);dy=rnd()%(LH-h);
        for(i=0;i<PAD+LW*LH+PAD;i++){
            uint16_t v=(uint16_t)rnd();
            actual[i]=expect[i]=snapshot[i]=v;
        }
        actual[0]=expect[0]=snapshot[0]=0xdeadu;actual[1]=expect[1]=snapshot[1]=0xbeefu;
        for(i=0;i<PAD;i++){actual[PAD+LW*LH+i]=expect[PAD+LW*LH+i]=snapshot[PAD+LW*LH+i]=0xf00du;}
        if(words){
            emutos_vdi_mono_copy_words(actual+PAD,LW,(uint16_t)sx,(uint16_t)sy,
                                       (uint16_t)w,(uint16_t)h,(uint16_t)dx,(uint16_t)dy);
            for(r=0;r<h;r++)for(c=0;c<w/16u;c++)
                expect[PAD+(dy+r)*LW+(dx/16u)+c]=snapshot[PAD+(sy+r)*LW+(sx/16u)+c];
        }else{
            emutos_vdi_mono_copy_shifted(actual+PAD,LW,(uint16_t)sx,(uint16_t)sy,
                                         (uint16_t)w,(uint16_t)h,(uint16_t)dx,(uint16_t)dy);
            for(r=0;r<h;r++)for(c=0;c<w;c++)
                putpixel(expect+PAD,dx+c,dy+r,getpixel(snapshot+PAD,sx+c,sy+r));
        }
        if(actual[0]!=0xdeadu||actual[1]!=0xbeefu||actual[PAD+LW*LH]!=0xf00du||
           actual[PAD+LW*LH+1]!=0xf00du){printf("guard clobbered case %u\n",n);return 1;}
        if(memcmp(actual+PAD,expect+PAD,LW*LH*sizeof(uint16_t))){
            printf("copy mismatch case %u words=%d sx=%u sy=%u w=%u h=%u dx=%u dy=%u\n",
                   n,words,sx,sy,w,h,dx,dy);return 1;}
    }
    puts("EmuTOS mono copy: 20000 differential aligned+shifted cases, guards OK");
    return 0;
}
