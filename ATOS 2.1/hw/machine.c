#include "hardware.h"
#include "system.h"
#include "machine_classify.h"

/* Phase 2 (v10): a single register being readable does NOT identify a machine.
   Every family is an independent fact and machine_classify() makes the
   decision.  The STE test is the EmuTOS write/readback of 0xffff820d: a plain
   read succeeds on ST/Mega ST too and used to report them as STE. */
static int detect_ste_shifter(void)
{
    uint8_t old=0u,value=0u;
    if(!probe_read8(SHIFTER_BASE_LO,&old))return 0;
    if(!probe_write8(SHIFTER_BASE_LO,90u)){probe_write8(SHIFTER_BASE_LO,old);return 0;}
    if(!probe_read8(SHIFTER_BASE_LO,&value)||value!=90u){probe_write8(SHIFTER_BASE_LO,old);return 0;}
    if(!probe_write8(SHIFTER_BASE_LO,0u)){probe_write8(SHIFTER_BASE_LO,old);return 0;}
    if(!probe_read8(SHIFTER_BASE_LO,&value)||value!=0u){probe_write8(SHIFTER_BASE_LO,old);return 0;}
    return 1;
}

void machine_detect(system_info_t *info)
{
    uint8_t value;
    uint16_t word;
    int has_videl=probe_read16(FALCON_HHT,&word);
    int has_tt_shifter=probe_read16(TT_PALETTE,&word);
    int has_mste_scu=probe_read8(MSTE_SCU_GPR1,&value);
    int has_ste_shifter=detect_ste_shifter();
    int has_rtc=rtc_mega_present();
    info->machine_type=machine_classify(has_videl,has_tt_shifter,has_mste_scu,has_ste_shifter,has_rtc);
    info->mega_ste = info->machine_type == MACHINE_MEGA_STE;
    if (info->mega_ste && probe_read8(MSTE_CACHE_CTRL, &value)) {
        info->clock_mhz=(value&1u)?16u:8u; info->clock_known=1;
        info->cache_enabled=(value&2u)?1u:0u;
    } else if (info->machine_type==MACHINE_TT) {
        info->clock_mhz=32; info->clock_known=1; info->cache_enabled=0;
    } else if (info->machine_type==MACHINE_FALCON) {
        info->clock_mhz=16; info->clock_known=1; info->cache_enabled=0;
    } else { info->clock_mhz=0; info->clock_known=0; info->cache_enabled=0; }
    info->blitter_present=(uint8_t)probe_read8(BLITTER_CTRL,&value);
    info->blitter_enabled=info->blitter_present;
}

const char *machine_name(uint8_t type)
{
    if (type==MACHINE_FALCON) return "ATARI FALCON";
    if (type==MACHINE_TT) return "ATARI TT";
    if (type==MACHINE_MEGA_STE) return "ATARI MEGA STE";
    if (type==MACHINE_STE) return "ATARI STE";
    if (type==MACHINE_MEGA_ST) return "ATARI MEGA ST";
    return "ATARI ST";
}

int machine_set_clock(uint8_t mhz)
{
    uint8_t control;
    if (!g_system.mega_ste || (mhz != 8u && mhz != 16u))
        return 0;
    control = REG8(MSTE_CACHE_CTRL);
    if (mhz == 16u) control |= 1u;
    else control &= (uint8_t)~1u;
    REG8(MSTE_CACHE_CTRL) = control;
    g_system.clock_mhz = mhz;
    g_system.clock_known = 1;
    return 1;
}
