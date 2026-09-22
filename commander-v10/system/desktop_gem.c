/* ATOS DESKTOP.PRG - minimal marker test (no GEM yet). */
#include <stdint.h>
static uint32_t screen_base(void)
{
    uint32_t r;
    __asm__ __volatile__("move.w #3,%%d0\n\ttrap #14\n\tmove.l %%d0,%0"
                         : "=d"(r) : : "d1","memory");
    return r;
}
int _tos_main(void)
{
    uint32_t pb=screen_base(),i;
    if((pb&1u)==0u && pb>=0x1000u){
        uint8_t *scr=(uint8_t *)(uintptr_t)pb;
        for(i=0;i<32000u;++i)scr[i]=0xAAu;
        for(i=0;i<80u*16u;++i)scr[i]=0x00u;
    }
    for(;;){__asm__ __volatile__("nop");}
    return 0;
}
