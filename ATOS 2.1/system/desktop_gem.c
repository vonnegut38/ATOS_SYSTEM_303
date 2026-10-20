/* ATOS DESKTOP.PRG - GEM app: top bar via VDI. Run AFTER GEM is up. */
#include <stdint.h>
typedef struct { int16_t contrl[12]; int16_t intin[128]; int16_t ptsin[128]; int16_t intout[128]; int16_t ptsout[128]; } VDIPB;
typedef struct { int16_t contrl[12]; int16_t global[15]; int16_t intin[128]; int16_t intout[128]; int16_t addrin[128]; int16_t addrout[128]; } AESPB;
extern VDIPB vp; extern AESPB ap; extern int16_t handle;
static void vdi_call(void){ __asm__ __volatile__("move.l %0,%%d1\n\tmove.l #0x73,%%d0\n\ttrap #2"::"r"(&vp):"d0","d1","memory"); }
static void aes_call(void){ __asm__ __volatile__("move.l %0,%%d1\n\tmove.l #0xC8,%%d0\n\ttrap #2"::"r"(&ap):"d0","d1","memory"); }
int _tos_main(void)
{
    int i;
    ap.contrl[0]=10; ap.contrl[1]=0; ap.contrl[2]=0; ap.contrl[3]=0; aes_call();  /* appl_init */
    for(i=0;i<10;i++)vp.intin[i]=1;
    vp.intin[10]=2;
    vp.contrl[0]=100; vp.contrl[1]=0; vp.contrl[2]=0; vp.contrl[3]=11; vp.contrl[6]=1;
    vdi_call();                                                                   /* v_opnvwk */
    handle=vp.contrl[6]; if(handle<=0)handle=1;
    vp.contrl[0]=23; vp.contrl[1]=0; vp.contrl[2]=0; vp.contrl[3]=1; vp.contrl[6]=handle; vp.intin[0]=1; vdi_call();
    vp.contrl[0]=11; vp.contrl[1]=2; vp.contrl[2]=0; vp.contrl[3]=0; vp.contrl[6]=handle;
    vp.ptsin[0]=0; vp.ptsin[1]=0; vp.ptsin[2]=639; vp.ptsin[3]=15;
    vdi_call();                                                                   /* v_bar top */
    for(;;){__asm__ __volatile__("nop");}
    return 0;
}
