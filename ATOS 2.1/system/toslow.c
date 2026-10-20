/* Does a TOS program see what real TOS gives it?
 *
 * LDW Power bus-errors before its first GEM call while reading address $10 (the
 * 68k vector table).  This program answers the two questions that decide where
 * the fault belongs:
 *   1. can a guest READ the system page (vectors at $10, _hz_200 at $4ba)?
 *   2. can a guest INSTALL an exception vector, and does our persona actually
 *      deliver the exception to it (the forwarding added to tos_entry.S)?
 *
 * Output goes through GEMDOS Cconws (function 9), so the kernel console - and
 * therefore a screenshot - shows the result even when something faults.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef short int16_t;

static int32_t gdos1(int16_t fn,int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
static void puts_(const char *s){ (void)gdos1(9,(int32_t)(uint32_t)s); }
static int32_t xbios1(int16_t fn,int32_t a)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w %2,-(%%sp)\n\t"
                         "trap #14\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(a), "r"((int32_t)fn)
                         : "d1","d2","a0","a1","a2","memory");
    return r;
}
/* Results read by the host. */
volatile uint32_t toslow_ptr[8];
/* Low memory is reached through inline asm: a constant address cast trips
   -Werror=array-bounds and is undefined behaviour in C anyway. */
static uint32_t lm_rd32(uint32_t addr)
{
    uint32_t v;
    __asm__ __volatile__("move.l %1@,%0" : "=d"(v) : "a"(addr) : "memory");
    return v;
}
static void lm_wr32(uint32_t addr,uint32_t v)
{
    __asm__ __volatile__("move.l %0,%1@" : : "d"(v),"a"(addr) : "memory");
}
static void hex32(uint32_t v) __attribute__((unused));
static void hex32(uint32_t v)
{
    static const char hx[]="0123456789ABCDEF";
    char b[11];int i;
    b[0]='0';b[1]='x';
    for(i=0;i<8;i++)b[2+i]=hx[(v>>((7-i)*4))&15u];
    b[10]=0;
    puts_(b);
}

/* Stage marker, exported so the host test can read it out of the TPA.  The
   handler sets the top bit: then the last stage tells exactly which operation
   faulted, without needing a visible screen. */
volatile uint32_t toslow_stage;

/* Installed into vector 2 by the guest.  If our persona forwards the exception
   this runs with the CPU's own frame on the stack. */
static void bus_handler(void)
{
    toslow_stage=0x80000000u|(toslow_stage&0xffu);
    for(;;){}
}

int _tos_main(void)
{
    uint32_t v10,v4ba,wback;

    puts_("TOSLOW: low-memory read/write probe\n");
    toslow_stage=1;
    v10=lm_rd32(0x10u);                 /* vector table */
    toslow_stage=2;
    v4ba=lm_rd32(0x4bau);               /* _hz_200 */
    toslow_stage=3;
    lm_wr32(2u*4u,(uint32_t)(uint32_t)bus_handler);   /* install vector 2 */
    toslow_stage=4;
    wback=lm_rd32(2u*4u);
    if(wback!=(uint32_t)(uint32_t)bus_handler){toslow_stage=0x40u;for(;;){}}
    toslow_stage=5;
    lm_wr32(0x400u,0x12345678u);        /* long write, user vector page */
    toslow_stage=6;
    wback=lm_rd32(0x400u);
    if(wback!=0x12345678u){toslow_stage=0x50u;for(;;){}}
    /* The long write at $4 that LDW used to make is GONE from this probe: the
       real cause was the wrong start stack (fixed), and the first 8 bytes are
       protected by the machine model, so testing them only hides later stages. */
    toslow_stage=8;
    lm_wr32(0x500u,0xa5u);              /* byte write in the system area */
    /* keep the reads alive so the compiler cannot drop them */
    if((v10|v4ba)==0u)toslow_stage=0x60u;
    /* XBIOS 34 Kbdvbase() and XBIOS 14 Iorec(1): LDW needs both, and a zero
       return is what made it read address $10. */
    toslow_stage=10;
    toslow_ptr[0]=(uint32_t)xbios1(34,0);
    toslow_stage=11;
    toslow_ptr[1]=(uint32_t)xbios1(14,1);
    toslow_stage=12;
    toslow_ptr[2]=toslow_ptr[0]?lm_rd32(toslow_ptr[0]+16u):0xffffffffu;
    toslow_stage=0;                     /* everything survived */
    toslow_ptr[3]=0x600d600du;          /* completion marker for the host */
    puts_("TOSLOW: DONE\n");
    for(;;){}
    return 0;
}
