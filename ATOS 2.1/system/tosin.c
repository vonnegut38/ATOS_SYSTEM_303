/* AES input probe: run the real event calls through the real trap.
 *
 * The host audit checked cut-out copies of the event code; this checks the
 * kernel itself.  Results go to a FIXED TPA address (0x04004100) so the host
 * never has to recompute symbol addresses after a link.
 *
 * AESPB layout (see system/tos_entry.S): {contrl,global,intin,intout,addrin,addrout}
 * Call convention: d0 = 0xC8 (AES), d1 = &AESPB, trap #2; result also in d0.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned long uintptr_t;

#define RES ((volatile uint32_t *)(uintptr_t)0x04004100UL)

static int16_t ctrl[8];
static int16_t in[32];
static int16_t out[40];
static void *global;
static void *addrin[2];
static void *addrout[2];
static void *pb[6]={ctrl,&global,in,out,addrin,addrout};

static int32_t aes_call(int16_t op,int16_t nintin,int16_t nintout,
                        int16_t naddrin,int16_t naddrout)
{
    int32_t r;
    unsigned i;
    for(i=0;i<8u;i++)ctrl[i]=0;
    for(i=0;i<40u;i++)out[i]=0x5555;          /* sentinel: shows stray writes */
    ctrl[0]=op;ctrl[1]=0;ctrl[2]=0;
    ctrl[3]=nintin;ctrl[4]=nintout;ctrl[5]=naddrin;ctrl[6]=naddrout;
    r=0;
    __asm__ __volatile__("move.l %2,%%d1\n\t"
                         "move.w #0xC8,%%d0\n\t"
                         "trap #2\n\t"
                         "move.l %%d0,%0"
                         : "=r"(r)
                         : "r"((void *)pb), "r"((void *)pb)
                         : "d0","d1","d2","a0","a1","a2","memory");
    return r;
}

int _tos_main(void)
{
    unsigned i;
    int32_t r;

    for(i=0;i<48u;i++)RES[i]=0;

    /* ---- evnt_button(1 click, left button, released) ---------------------- */
    RES[0]=1u;
    in[0]=1;in[1]=1;in[2]=0;                  /* clicks, mask, state */
    r=aes_call(21,3,5,0,0);
    RES[1]=(uint32_t)r;
    for(i=0;i<5u;i++)RES[2+i]=(uint32_t)(uint16_t)out[i];

    /* ---- evnt_mouse(flags=0 -> wait for ENTER, rectangle) ----------------- */
    RES[8]=1u;
    in[0]=0;in[1]=0;in[2]=0;in[3]=640;in[4]=400;
    r=aes_call(22,5,5,0,0);
    RES[9]=(uint32_t)r;
    for(i=0;i<5u;i++)RES[10+i]=(uint32_t)(uint16_t)out[i];

    /* ---- evnt_keybd: the host injects the Up arrow ------------------------ */
    RES[16]=1u;
    r=aes_call(20,0,1,0,0);
    RES[17]=(uint32_t)(uint16_t)r;
    RES[18]=(uint32_t)(uint16_t)out[0];

    RES[47]=0x600d600du;
    for(;;){}
    return 0;
}
