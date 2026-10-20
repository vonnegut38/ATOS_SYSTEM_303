/* GDOS ABI probe: does the kernel answer the font calls LDW/GDOS programs make?
 *
 * Calls vst_load_fonts (119), vqt_name (130) and vqt_fontinfo (131) through the
 * real VDI trap and stores the results in a block that the host test reads out
 * of the TPA, so the answer does not depend on a visible screen.
 *
 * VDIPB layout and trap: d1 = &VDIPB, d0 = 0x73, trap #2 (see system/tosvdi.c).
 */
typedef signed long int32_t;
typedef unsigned long uintptr_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;

static int16_t v_contrl[16];
static int16_t v_intin[32];
static int16_t v_ptsin[16];
static int16_t v_intout[64];
static int16_t v_ptsout[16];
static void *v_pb[5]={ v_contrl,v_intin,v_ptsin,v_intout,v_ptsout };

/* Results, read by the host. */
/* Results at a FIXED TPA address: the kernel knows the TPA base (0x04000000),
   so the host never has to compute symbol addresses that shift with every link
   (that mistake cost several measurement cycles).  The program itself is small,
   its BSS is far below this and the stack lives at the top of the TPA. */
#define GDOS_RESULT ((volatile uint32_t *)(uintptr_t)0x04004100UL)
#define gdos_result GDOS_RESULT
volatile int16_t  gdos_intout[64];      /* sentinels live past the declared words */
volatile int16_t  gdos_ptsout[32];
#define SENT 0x5555

static int16_t vdi_call(uint16_t op,int16_t npts_in,int16_t npts_out,
                        int16_t nint_in,int16_t nint_out)
{
    void *pb=(void *)v_pb;
    v_contrl[0]=(int16_t)op;
    v_contrl[1]=npts_in;  v_contrl[2]=npts_out;
    v_contrl[3]=nint_in;  v_contrl[4]=nint_out;
    __asm__ __volatile__("move.l %0,%%d1\n\t"
                         "move.w #0x73,%%d0\n\t"
                         "trap #2"
                         : : "r"(pb)
                         : "d0","d1","d2","a0","a1","a2","memory");
    return v_contrl[0];
}

int _tos_main(void)
{
    int i,handle=0;

    for(i=0;i<48;i++)gdos_result[i]=0;

    /* --- open a workstation the standard way (contrl[0]=100 v_opnvwk) ------ */
    gdos_result[0]=1u;
    for(i=0;i<64;i++){v_intout[i]=SENT;gdos_intout[i]=SENT;}
    v_intin[0]=1;v_intin[1]=1;v_intin[2]=1;v_intin[3]=1;v_intin[4]=1;
    (void)vdi_call(100,0,0,5,45);
    handle=v_contrl[6];
    gdos_result[1]=(uint32_t)(uint16_t)handle;

    /* --- 119 vst_load_fonts: select is ONE intin word -------------------- */
    v_intin[0]=0;
    (void)vdi_call(119,0,0,1,1);
    gdos_result[2]=(uint32_t)(uint16_t)v_intout[0];

    /* --- 130 vqt_name: 1 intin (element), 33 intout, one CHAR PER WORD --- */
    for(i=0;i<33;i++)v_intout[i]=SENT;          /* sentinel right after */
    v_intin[0]=1;
    (void)vdi_call(130,0,0,1,33);
    gdos_result[3]=(uint32_t)(uint16_t)v_intout[0];      /* font ID */
    for(i=0;i<10;i++)gdos_result[4+i]=(uint32_t)(uint16_t)v_intout[1+i];
    gdos_result[14]=(uint32_t)(uint16_t)v_intout[33];    /* must stay SENT */

    /* --- 131 vqt_fontinfo: 2 intout + 10 ptsout, sentinels after both ---- */
    for(i=0;i<64;i++){v_intout[i]=SENT;}
    for(i=0;i<16;i++)v_ptsout[i]=SENT;   /* array is 16 words; index 10 is the sentinel */
    (void)vdi_call(131,0,5,0,2);
    for(i=0;i<2;i++)gdos_result[15+i]=(uint32_t)(uint16_t)v_intout[i];
    gdos_result[17]=(uint32_t)(uint16_t)v_intout[2];     /* must stay SENT */
    for(i=0;i<10;i++)gdos_result[18+i]=(uint32_t)(uint16_t)v_ptsout[i];
    gdos_result[28]=(uint32_t)(uint16_t)v_ptsout[10];    /* must stay SENT */

    gdos_result[40]=(uint32_t)(uintptr_t)v_intout;   /* our own buffer addresses */
    gdos_result[41]=(uint32_t)(uintptr_t)v_ptsout;
    gdos_result[42]=(uint32_t)(uintptr_t)v_contrl;
    gdos_result[47]=0x600d600du;
    for(;;){}
    return 0;
}
