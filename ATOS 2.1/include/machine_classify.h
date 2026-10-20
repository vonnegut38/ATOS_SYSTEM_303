#ifndef ASF_MACHINE_CLASSIFY_H
#define ASF_MACHINE_CLASSIFY_H
#include "system.h"

/* Phase 2 (v10): pure machine-type decision, separated from the hardware
   probes so it can be host-tested.  Each argument is an independent hardware
   fact; never decide from a single register.  Order is deliberate:
   VIDEL -> TT shifter -> Mega STE (SCU) -> STE shifter -> Mega ST (RTC) -> ST.
   Host regression: tests/test_machine_detect.c (make check-machine). */
static inline uint8_t machine_classify(int has_videl,int has_tt_shifter,
                                       int has_mste_scu,int has_ste_shifter,
                                       int has_rtc)
{
    if (has_videl)      return MACHINE_FALCON;
    if (has_tt_shifter) return MACHINE_TT;
    if (has_mste_scu)   return MACHINE_MEGA_STE;
    if (has_ste_shifter)return MACHINE_STE;
    if (has_rtc)        return MACHINE_MEGA_ST;
    return MACHINE_ST;
}
#endif
