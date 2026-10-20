#include "kernel_blitter.h"
#include "hardware.h"

#define KBLIT_WAIT_LIMIT 65535UL
static uint8_t detected,active,copy_ok,copy_fail;

/* Blitter SKEW register: low nibble is the bit skew, bit 7/6 are the FXSR/NFSR
   flags that select which source word pair feeds the shifter. */
#define KBLIT_FXSR 0x80u
#define KBLIT_NFSR 0x40u

static uint16_t irq_lock(void)
{
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0\n\tor.w #0x0700,%%sr":"=d"(sr)::"cc","memory");
    return sr;
}

static void irq_restore(uint16_t sr)
{
    __asm__ volatile("move.w %0,%%sr"::"d"(sr):"cc","memory");
}

void kblit_init(uint8_t present,uint8_t enabled)
{
    detected=present?1u:0u;active=(present&&enabled)?1u:0u;
    copy_ok=active;copy_fail=0u;
}

uint8_t kblit_present(void){return detected;}
uint8_t kblit_enabled(void){return active;}
/* Copy acceleration is a separate capability from fill: a copy self-test
   failure must never disable the already-proven fill path. */
uint8_t kblit_copy_ready(void){return copy_ok;}
uint8_t kblit_copy_fail(void){return copy_fail;}

/* Start the blitter and wait for BUSY to clear.  No-HOG mode, as recommended:
   an interrupt can be serviced between scans.  Returns 0 if it never finished,
   and disables the accelerator so the CPU fallback is used from then on. */
static int kblit_run(uint16_t sr)
{
    uint32_t wait=KBLIT_WAIT_LIMIT;
    REG8(BLITTER_CTRL)=0x80u;
    while((REG8(BLITTER_CTRL)&0x80u)&&--wait){}
    if(!wait){REG8(BLITTER_CTRL)=0;active=0;irq_restore(sr);return 0;}
    return 1;
}

int kblit_fill_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t color)
{
    uint16_t first,last,groups,left,right,plane,dst_xinc,dst_yinc,sr;
    volatile uint16_t *destination;
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
        REG8(BLITTER_SKEW)=0;
        if(!kblit_run(sr))return 0;
    }
    irq_restore(sr);return 1;
}

/* Hardware source-only copy of one planar rectangle with arbitrary bit
   alignment and overlap.  Ported from EmuTOS vdi/vdi_raster.c bit_blt()
   (GPL-2.0-or-later, Copyright 2002 Joachim Hoenig, 2003-2025 The EmuTOS
   development team).  The blitter only runs when kblit_init() saw the chip AND
   kblit_self_test() proved both its fill and its skewed copy correct; otherwise
   this returns 0 and the caller keeps the CPU path. */
int kblit_copy_planar_to(volatile uint16_t *src,uint16_t src_lw,
                         volatile uint16_t *dst,uint16_t dst_lw,uint8_t planes,
                         uint16_t source_x,uint16_t source_y,uint16_t width,uint16_t height,
                         uint16_t destination_x,uint16_t destination_y)
{
    /* index bit0 = skew<0, bit1 = equal spans, bit2 = left->right. */
    static const uint8_t skew_flags[8]={
        KBLIT_NFSR,KBLIT_FXSR,(KBLIT_NFSR|KBLIT_FXSR),0,
        KBLIT_NFSR,KBLIT_FXSR,0,(KBLIT_NFSR|KBLIT_FXSR)
    };
    uint16_t s_xmin,s_xmax,d_xmin,d_xmax,lendmask,rendmask,end1,end2,end3;
    uint16_t s_xmin_off,s_xmax_off,s_span,d_xmin_off,d_xmax_off,d_span,x_cnt;
    int32_t skew;uint16_t skew_idx;uint8_t skewreg;
    int16_t src_x_inc,src_y_inc,dst_x_inc,dst_y_inc;
    uint32_t s_addr,d_addr,plane,sr;
    if(!active||!copy_ok||!src||!dst||!planes||planes>4u||!width||!height)return 0;
    s_xmin=source_x;s_xmax=(uint16_t)(source_x+width-1u);
    d_xmin=destination_x;d_xmax=(uint16_t)(destination_x+width-1u);
    s_xmin_off=(uint16_t)(s_xmin>>4);s_xmax_off=(uint16_t)(s_xmax>>4);
    s_span=(uint16_t)(s_xmax_off-s_xmin_off);
    d_xmin_off=(uint16_t)(d_xmin>>4);d_xmax_off=(uint16_t)(d_xmax>>4);
    d_span=(uint16_t)(d_xmax_off-d_xmin_off);
    x_cnt=(uint16_t)(d_span+1u);
    lendmask=(uint16_t)(0xffffu>>(d_xmin&15u));
    rendmask=(uint16_t)(~(0x7fffu>>(d_xmax&15u)));
    skew=(int32_t)(d_xmin&15u)-(int32_t)(s_xmin&15u);
    skew_idx=0u;
    if(d_span==s_span)skew_idx|=2u;
    if(skew<0)skew_idx|=1u;
    {
        /* planes are interleaved: one 16-pixel group step is 'planes' words. */
        uint32_t s_first=(uint32_t)source_y*src_lw+(uint32_t)s_xmin_off*planes;
        uint32_t d_first=(uint32_t)destination_y*dst_lw+(uint32_t)d_xmin_off*planes;
        uint32_t s_last=(uint32_t)(source_y+height-1u)*src_lw+(uint32_t)s_xmax_off*planes;
        uint32_t d_last=(uint32_t)(destination_y+height-1u)*dst_lw+(uint32_t)d_xmax_off*planes;
        uint16_t group_step=(uint16_t)(planes*2u);
        /* Overlap only matters inside one bitmap; cross-buffer always forward. */
        if((src==dst)&&(s_first<d_first||(s_first==d_first&&skew>=0))){
            s_addr=s_last;d_addr=d_last;
            src_x_inc=(int16_t)(-(int16_t)group_step);
            dst_x_inc=(int16_t)(-(int16_t)group_step);
            src_y_inc=(int16_t)(-(int16_t)((uint16_t)(src_lw*2u)-(uint16_t)(group_step*s_span)));
            dst_y_inc=(int16_t)(-(int16_t)((uint16_t)(dst_lw*2u)-(uint16_t)(group_step*d_span)));
            end1=rendmask;end2=0xffffu;end3=lendmask;
        }else{
            s_addr=s_first;d_addr=d_first;
            src_x_inc=(int16_t)group_step;
            dst_x_inc=(int16_t)group_step;
            src_y_inc=(int16_t)((uint16_t)(src_lw*2u)-(uint16_t)(group_step*s_span));
            dst_y_inc=(int16_t)((uint16_t)(dst_lw*2u)-(uint16_t)(group_step*d_span));
            end1=lendmask;end2=0xffffu;end3=rendmask;
            skew_idx|=4u;
        }
    }
    if(!d_span)end1=(uint16_t)(end1&end3);   /* XCOUNT==1 ignores endmask 3 */
    if(!s_span&&!d_span){src_x_inc=(int16_t)skew;skewreg=(uint8_t)(skew?(skew&0x0f):KBLIT_FXSR);}
    else skewreg=(uint8_t)((skew&0x0f)|skew_flags[skew_idx]);
    sr=irq_lock();
    for(plane=0;plane<planes;++plane){
        REG16(BLITTER_SRC_XINC)=(uint16_t)src_x_inc;
        REG16(BLITTER_SRC_YINC)=(uint16_t)src_y_inc;
        REG32(BLITTER_SRC_ADDR)=(uint32_t)(uintptr_t)(src+s_addr+plane);
        REG16(BLITTER_ENDMASK1)=end1;
        REG16(BLITTER_ENDMASK2)=end2;
        REG16(BLITTER_ENDMASK3)=end3;
        REG16(BLITTER_DST_XINC)=(uint16_t)dst_x_inc;
        REG16(BLITTER_DST_YINC)=(uint16_t)dst_y_inc;
        REG32(BLITTER_DST_ADDR)=(uint32_t)(uintptr_t)(dst+d_addr+plane);
        REG16(BLITTER_XCOUNT)=x_cnt;
        REG16(BLITTER_YCOUNT)=height;
        REG8(BLITTER_HOP)=2u;        /* HOP source only */
        REG8(BLITTER_OP)=3u;         /* OP replace */
        REG8(BLITTER_SKEW)=skewreg;
        if(!kblit_run(sr))return 0;
    }
    irq_restore(sr);
    return 1;
}
/* Same-bitmap copy (used by kgfx_copy_rect). */
int kblit_copy_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t source_x,uint16_t source_y,uint16_t width,uint16_t height,
                      uint16_t destination_x,uint16_t destination_y)
{
    return kblit_copy_planar_to(words,line_words,words,line_words,planes,
                                source_x,source_y,width,height,destination_x,destination_y);
}

/* ---- boot-time proof that the detected blitter really works -------------- */

#define KBLIT_TEST_GROUPS 12u          /* 192 pixels wide */
#define KBLIT_TEST_LINES  12u          /* the dy=4,h=8 case needs 12 rows */
#define KBLIT_TEST_MAXW   (KBLIT_TEST_GROUPS*4u)   /* 4 planes worst case */
#define KBLIT_TEST_COUNT  (KBLIT_TEST_LINES*KBLIT_TEST_MAXW)
static uint16_t kblit_img[KBLIT_TEST_COUNT+2];
static uint16_t kblit_snap[KBLIT_TEST_COUNT+2];
static uint16_t kblit_ref[KBLIT_TEST_COUNT+2];
static uint32_t kblit_seed=0x12345678u;
static uint32_t kblit_rnd(void){kblit_seed=kblit_seed*1664525u+1013904223u;return kblit_seed;}

/* Per-pixel reference over the interleaved plane layout: plane q of a 16-pixel
   group lives at word (x>>4)*planes+q. */
static void kblit_ref_copy(const uint16_t *src,uint16_t *dst,unsigned planes,
                           unsigned line_words,int sx,int sy,int w,int h,int dx,int dy)
{
    int r,c;unsigned q;
    for(r=0;r<h;r++)for(c=0;c<w;c++){
        int x=sx+c,y=sy+r,X=dx+c,Y=dy+r;
        const uint16_t *sp=src+1u+(uint32_t)y*line_words+(uint32_t)(x>>4)*planes;
        uint16_t *dp=dst+1u+(uint32_t)Y*line_words+(uint32_t)(X>>4)*planes;
        uint16_t dmask=(uint16_t)(0x8000u>>(X&15));
        for(q=0;q<planes;q++){
            unsigned v=(sp[q]>>(15-(x&15)))&1u;
            if(v)dp[q]|=dmask;else dp[q]=(uint16_t)(dp[q]&(uint16_t)~dmask);
        }
    }
}

static int kblit_copy_case(unsigned planes,unsigned line_words,
                           int sx,int sy,int w,int h,int dx,int dy)
{
    unsigned i;
    for(i=0;i<KBLIT_TEST_COUNT+2u;i++){uint16_t v=(uint16_t)kblit_rnd();
        kblit_img[i]=v;kblit_snap[i]=v;kblit_ref[i]=v;}
    kblit_img[0]=0xa5a5u;kblit_img[KBLIT_TEST_COUNT+1u]=0x5a5au;
    if(!kblit_copy_planar(kblit_img+1,line_words,(uint8_t)planes,
                          (uint16_t)sx,(uint16_t)sy,(uint16_t)w,(uint16_t)h,
                          (uint16_t)dx,(uint16_t)dy))return 0;
    kblit_ref_copy(kblit_snap,kblit_ref,planes,line_words,sx,sy,w,h,dx,dy);
    if(kblit_img[0]!=0xa5a5u||kblit_img[KBLIT_TEST_COUNT+1u]!=0x5a5au)return 0;
    for(i=1u;i<=KBLIT_TEST_COUNT;i++)if(kblit_img[i]!=kblit_ref[i])return 0;
    return 1;
}

int kblit_self_test(void)
{
    static const struct {int sx,sy,w,h,dx,dy;} cases[]={
        {0,0,32,4,16,0},   /* aligned, forward */
        {0,0,32,4,64,0},   /* source before dest: right-to-left */
        {0,0,48,4,5,1},    /* skew > 0 */
        {5,0,32,4,0,2},    /* skew < 0 */
        {0,0,20,4,20,0},   /* different word spans */
        {0,0,64,6,2,3},    /* overlapping, skew != 0 */
        {8,2,40,6,0,4},    /* overlapping, source after dest */
        {0,0,16,4,144,0},  /* single destination word at the right edge */
        {0,0,8,4,152,0},
        {0,0,4,4,156,0},
        {32,0,32,4,0,0},   /* source after dest: left-to-right */
        {0,0,32,8,0,4}     /* vertical move, destination below source */
    };
    static volatile uint16_t test[10];
    unsigned i,pi;uint8_t k;
    if(!active)return 1;
    for(k=0;k<10u;++k)test[k]=0;
    test[0]=0x55aau;test[9]=0xaa55u;
    if(!kblit_fill_planar(test+1,4u,1u,3u,0u,22u,2u,1u))return 0;
    if(test[0]!=0x55aau||test[9]!=0xaa55u||test[1]!=0x1fffu||
       test[2]!=0xff80u||test[5]!=0x1fffu||test[6]!=0xff80u){active=0;return 0;}
    test[1]=0;
    if(!kblit_fill_planar(test+1,4u,1u,0u,0u,1u,1u,1u)||test[1]!=0x8000u){
        active=0;return 0;}
    /* The compositor supports 1/2/4bpp; prove the skewed copy on each. */
    if(copy_ok)for(pi=0;pi<3u&&copy_ok;++pi){
        unsigned planes=(pi==0u)?1u:((pi==1u)?2u:4u);
        unsigned line_words=KBLIT_TEST_GROUPS*planes;
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
            if(!kblit_copy_case(planes,line_words,cases[i].sx,cases[i].sy,
                                cases[i].w,cases[i].h,cases[i].dx,cases[i].dy)){
                /* Keep fill acceleration; only the copy path falls back. */
                copy_ok=0u;copy_fail=(uint8_t)(pi*sizeof(cases)/sizeof(cases[0])+i+1u);return 1;
            }
    }
    return 1;
}
