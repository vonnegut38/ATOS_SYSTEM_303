/* Phase 2 (v10) regression: machine_classify() must map independent hardware
   facts to the right machine, so ST/Mega ST can never be reported as STE.
   Run: make check-machine  (host build, no emulator needed). */
#include <stdio.h>
#include "machine_classify.h"

static int fails;
static void expect(uint8_t got,uint8_t want,const char *name)
{
    if(got!=want){printf("FAIL: %-34s got %u want %u\n",name,(unsigned)got,(unsigned)want);++fails;}
}

int main(void)
{
    expect(machine_classify(1,1,1,1,1),MACHINE_FALCON,   "videl dominates");
    expect(machine_classify(0,1,1,1,1),MACHINE_TT,       "tt shifter");
    expect(machine_classify(0,0,1,1,1),MACHINE_MEGA_STE, "scu -> mega ste");
    expect(machine_classify(0,0,0,1,1),MACHINE_STE,      "ste shifter beats rtc");
    expect(machine_classify(0,0,0,1,0),MACHINE_STE,      "ste without rtc");
    expect(machine_classify(0,0,0,0,1),MACHINE_MEGA_ST,  "rtc only -> mega st");
    expect(machine_classify(0,0,0,0,0),MACHINE_ST,       "plain st");
    if(fails){printf("MACHINE_DETECT: %d FAIL\n",fails);return 1;}
    printf("MACHINE_DETECT: ALL PASS\n");
    return 0;
}
