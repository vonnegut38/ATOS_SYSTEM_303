/* Super() probe: does GEMDOS 0x20 really switch the processor mode?
 *
 * The mode is observed through Super(1) itself (0 = user, -1 = supervisor),
 * so the test needs no privileged instruction of its own.  Results go to the
 * fixed TPA address 0x04004100.
 *
 * Call convention matches TOS and LDW: push the argument, push the function
 * word, trap #1, then the caller removes its own six bytes.
 */
typedef signed long int32_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned long uintptr_t;

#define RES ((volatile uint32_t *)(uint32_t)0x04004100UL)

static int32_t super_call(int32_t arg)
{
    register int32_t r __asm__("d0");
    __asm__ __volatile__("move.l %1,-(%%sp)\n\t"
                         "move.w #0x20,-(%%sp)\n\t"
                         "trap #1\n\t"
                         "addq.l #6,%%sp"
                         : "=r"(r)
                         : "r"(arg)
                         : "d1","a0","a1","memory");
    return r;
}

static uint32_t read_usp(void)
{
    void *v;
    /* USP can only be moved through an address register, and reading it is
       privileged - so this is used only while in supervisor mode. */
    __asm__ __volatile__("move.l %%usp,%0" : "=a"(v));
    return (uint32_t)(uintptr_t)v;
}

int _tos_main(void)
{
    int32_t r1,r2,r3,r5;
    uint32_t usp;

    for(usp=0u;usp<8u;usp++)RES[usp]=0u;

    RES[0]=1u;
    r1=super_call(1);                 /* inquiry while in USER mode  -> 0  */
    RES[1]=(uint32_t)r1;

    RES[2]=1u;
    (void)super_call(0);              /* switch to SUPERVISOR on our own stack */
    r2=super_call(1);                 /* inquiry in SUPERVISOR mode  -> -1 */
    RES[3]=(uint32_t)r2;

    usp=read_usp();                   /* our own user stack, read in supervisor */
    RES[4]=usp;
    r3=super_call((int32_t)usp);      /* back to USER mode on that stack */
    RES[5]=(uint32_t)r3;
    r5=super_call(1);                 /* inquiry in USER mode again  -> 0 */
    RES[6]=(uint32_t)r5;

    RES[7]=0x600d600du;
    for(;;){}
    return 0;
}
