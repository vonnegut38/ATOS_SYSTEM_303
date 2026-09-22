/* ATOS DESKTOP.PRG - first milestone: a GEM application on the EmuTOS layer.
 * Draws a top applet bar and a clock with VDI, and refreshes ONLY the clock
 * rectangle when the second changes (EmuTOS-style damage, no full redraw). */
#include <stdint.h>

typedef struct { int16_t contrl[12]; int16_t intin[128]; int16_t ptsin[128]; int16_t intout[128]; int16_t ptsout[128]; } VDIPB;
typedef struct { int16_t contrl[12]; int16_t global[15]; int16_t intin[128]; int16_t intout[128]; int16_t addrin[128]; int16_t addrout[128]; } AESPB;

static VDIPB vp;
static AESPB ap;
static int16_t vdi_handle;

/* TOS GEM trap #2: d0 = 0x73 (VDI) / 0xC8 (AES), d1 = parameter block. */
static void vdi_call(void){ __asm__ volatile("move.l %0,%%d1\n\tmove.l #0x73,%%d0\n\ttrap #2"::"r"(&vp):"d0","d1","memory"); }
static void aes_call(void){ __asm__ volatile("move.l %0,%%d1\n\tmove.l #0xC8,%%d0\n\ttrap #2"::"r"(&ap):"d0","d1","memory"); }

static void gtext(int x,int y,const char *s)
{
    int i;
    for(i=0;i<10&&s[i];++i) vp.intin[i]=(int16_t)s[i];
    vp.contrl[0]=8; vp.contrl[1]=1; vp.contrl[3]=i; vp.contrl[6]=vdi_handle;
    vp.ptsin[0]=(int16_t)x; vp.ptsin[1]=(int16_t)y;
    vdi_call();
}
static void bar(int x0,int y0,int x1,int y1)
{
    vp.contrl[0]=11; vp.contrl[1]=2; vp.contrl[2]=0; vp.contrl[6]=vdi_handle;
    vp.ptsin[0]=(int16_t)x0; vp.ptsin[1]=(int16_t)y0;
    vp.ptsin[2]=(int16_t)x1; vp.ptsin[3]=(int16_t)y1;
    vdi_call();
}
static void vdi_set_fill(int c){ vp.contrl[0]=23; vp.contrl[1]=0; vp.contrl[3]=1; vp.contrl[6]=vdi_handle; vp.intin[0]=(int16_t)c; vdi_call(); }
static void vdi_set_text_color(int c){ vp.contrl[0]=22; vp.contrl[1]=0; vp.contrl[3]=1; vp.contrl[6]=vdi_handle; vp.intin[0]=(int16_t)c; vdi_call(); }

static uint16_t tgettime(void)
{
    uint16_t t;
    __asm__ volatile("move.w #0x2c,%%d0\n\ttrap #1\n\tmove.w %%d0,%0":"=r"(t)::"d0","memory");
    return t;
}

static void clock_text(char out[9])
{
    uint16_t t=tgettime();
    uint16_t sec=((t>>5)&0x3f),min=((t>>11)&0x3f),hour=(uint16_t)(t&0x1f);
    out[0]=(char)('0'+hour/10u);out[1]=(char)('0'+hour%10u);out[2]=':';
    out[3]=(char)('0'+min/10u);out[4]=(char)('0'+min%10u);out[5]=':';
    out[6]=(char)('0'+sec/10u);out[7]=(char)('0'+sec%10u);out[8]=0;
}

int _tos_main(void)
{
    char clock[9];uint16_t last=0xffffu;int16_t w=640;
    /* ATOS diagnostic: if AUTO runs at all, paint a marker straight into the
       Shifter screen via XBIOS Physbase - no AES/VDI involvement. */
    {uint32_t pb;uint32_t i;
     __asm__ __volatile__("move.w #2,%%d0\n\ttrap #14\n\tmove.l %%d0,%0":"=r"(pb)::"d0","memory");
     if(pb){uint8_t *scr=(uint8_t *)(uintptr_t)pb;for(i=0;i<32000u;++i)scr[i]=0xAAu;}}
    ap.contrl[0]=10; ap.contrl[3]=0; aes_call();      /* appl_init */
    vp.contrl[0]=1; vp.contrl[1]=0; vp.contrl[2]=0; vp.contrl[3]=0;
    vp.contrl[5]=1; vp.intin[0]=1;                     /* v_opnwk(device 1) */
    vdi_call();
    vdi_handle=vp.contrl[6];
    if(vp.intout[0])w=vp.intout[0];
    vdi_set_fill(1); bar(0,0,w-1,15);                  /* top applet bar */
    for(;;){
        uint16_t t=tgettime();
        if(t!=last){
            last=t;
            clock_text(clock);
            vdi_set_fill(1); bar(w-80,1,w-2,14);       /* clear only the clock cell */
            vdi_set_text_color(0); gtext(w-74,11,clock);
        }
    }
    return 0;
}
