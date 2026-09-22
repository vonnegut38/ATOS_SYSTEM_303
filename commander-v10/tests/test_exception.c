#include <stdio.h>
#include "exception_format.h"
static int fails;
static void check(int ok,const char*w){if(!ok){printf("FAIL %s\n",w);++fails;}}
int main(void)
{
    exc_frame_t f;
    /* 68000 short frame: SR=0x2700, PC=0x00E01234 */
    {const uint16_t w[3]={0x2700u,0x00E0u,0x1234u};
     exc_parse_frame(w,0,&f);
     check(f.format==0u,"68k format 0");
     check(f.vector==0xFFFFu,"68k vector unknown");
     check(f.sr==0x2700u,"68k sr");
     check(f.pc==0x00E01234u,"68k pc");
     check(!f.fault_valid,"68k no fault addr");}
    /* 68030 bus error, format 7: fmt/vec=0x7002, SR=0x2700, PC=0x00E02000, ..., fault=0x00FF1234 */
    {const uint16_t w[8]={0x7002u,0x2700u,0x00E0u,0x2000u,0x0000u,0x00FFu,0x1234u,0x0000u};
     exc_parse_frame(w,1,&f);
     check(f.format==7u,"030 format 7");
     check(f.vector==2u,"030 vector 2");
     check(f.sr==0x2700u,"030 sr");
     check(f.pc==0x00E02000u,"030 pc");
     check(f.fault_valid&&f.fault==0x00FF1234u,"030 fault addr");}
    /* 68030 illegal instruction, format 3: fmt/vec=0x3004 */
    {const uint16_t w[4]={0x3004u,0x2000u,0x00E0u,0x3000u};
     exc_parse_frame(w,1,&f);
     check(f.format==3u&&f.vector==4u,"030 illegal fmt3 vec4");
     check(f.pc==0x00E03000u,"030 illegal pc");
     check(!f.fault_valid,"030 illegal no fault");}
    check(exc_frame_has_format(0x7002u),"long detected");
    if(fails){printf("EXCEPTION: %d FAIL\n",fails);return 1;}
    printf("EXCEPTION: ALL PASS\n");return 0;
}
