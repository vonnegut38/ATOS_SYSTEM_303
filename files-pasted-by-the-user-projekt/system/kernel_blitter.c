#include "kernel_blitter.h"
#include "hardware.h"

#define KBLIT_WAIT_LIMIT 65535UL
static uint8_t detected,active;

static uint16_t irq_lock(void)
{
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0\n\tori.w #0x0700,%%sr":"=d"(sr)::"cc","memory");
    return sr;
}

static void irq_restore(uint16_t sr)
{
    __asm__ volatile("move.w %0,%%sr"::"d"(sr):"cc","memory");
}

void kblit_init(uint8_t present,uint8_t enabled)
{
    detected=present?1u:0u;active=(present&&enabled)?1u:0u;
}

uint8_t kblit_present(void){return detected;}
uint8_t kblit_enabled(void){return active;}

int kblit_fill_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t color)
{
    uint16_t first,last,groups,left,right,plane,dst_xinc,dst_yinc,sr;
    uint32_t wait;volatile uint16_t *destination;
    if(!active||!words||!planes||planes>4u||!width||!height)return 0;
    first=(uint16_t)(x>>4);last=(uint16_t)((x+width-1u)>>4);
    groups=(uint16_t)(last-first+1u);
    left=(uint16_t)(0xffffu>>(x&15u));
    right=(uint16_t)(0xffffu<<(15u-((x+width-1u)&15u)));
    /* With XCOUNT=1 the Atari Blitter applies ENDMASK1, not an implicit
       combination of the first and last masks. Merge both edges explicitly
       or an aligned one-pixel vertical line becomes a full 16-pixel word. */
    if(groups==1u){left&=right;right=left;}
    dst_xinc=(uint16_t)(planes*2u);
    dst_yinc=(uint16_t)(line_words*2u-(uint16_t)((groups-1u)*dst_xinc));
    destination=words+(uint32_t)y*line_words+(uint32_t)first*planes;
    sr=irq_lock();
    for(plane=0;plane<planes;++plane){
        REG16(BLITTER_SRC_XINC)=0;REG16(BLITTER_SRC_YINC)=0;REG32(BLITTER_SRC_ADDR)=0;
        REG16(BLITTER_ENDMASK1)=left;REG16(BLITTER_ENDMASK2)=0xffffu;
        REG16(BLITTER_ENDMASK3)=right;REG16(BLITTER_DST_XINC)=dst_xinc;
        REG16(BLITTER_DST_YINC)=dst_yinc;
        REG32(BLITTER_DST_ADDR)=(uint32_t)(uintptr_t)(destination+plane);
        REG16(BLITTER_XCOUNT)=groups;REG16(BLITTER_YCOUNT)=height;
        REG8(BLITTER_HOP)=0;REG8(BLITTER_OP)=(color&(uint8_t)(1u<<plane))?3u:0u;
        REG8(BLITTER_SKEW)=0;REG8(BLITTER_CTRL)=0x80u;
        wait=KBLIT_WAIT_LIMIT;while((REG8(BLITTER_CTRL)&0x80u)&&--wait){}
        if(!wait){REG8(BLITTER_CTRL)=0;active=0;irq_restore(sr);return 0;}
    }
    irq_restore(sr);return 1;
}

int kblit_self_test(void)
{
    static volatile uint16_t test[10];uint8_t i;
    if(!active)return 1;
    for(i=0;i<10u;++i)test[i]=0;
    test[0]=0x55aau;test[9]=0xaa55u;
    if(!kblit_fill_planar(test+1,4u,1u,3u,0u,22u,2u,1u))return 0;
    if(test[0]!=0x55aau||test[9]!=0xaa55u||test[1]!=0x1fffu||
       test[2]!=0xff80u||test[5]!=0x1fffu||test[6]!=0xff80u){active=0;return 0;}
    test[1]=0;
    if(!kblit_fill_planar(test+1,4u,1u,0u,0u,1u,1u,1u)||test[1]!=0x8000u){
        active=0;return 0;}
    return 1;
}
