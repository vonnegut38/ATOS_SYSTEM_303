#include "hardware.h"
#include "system.h"

void machine_detect(system_info_t *info)
{
    uint8_t value;
    uint16_t word;
    if (probe_read16(FALCON_HHT, &word)) info->machine_type=MACHINE_FALCON;
    else if (probe_read16(TT_PALETTE, &word)) info->machine_type=MACHINE_TT;
    else if (probe_read8(MSTE_SCU_GPR1, &value)) info->machine_type=MACHINE_MEGA_STE;
    else if (probe_read8(SHIFTER_BASE_LO, &value)) info->machine_type=MACHINE_STE;
    else if (probe_read8(MEGA_RTC_SECONDS, &value)) info->machine_type=MACHINE_MEGA_ST;
    else info->machine_type=MACHINE_ST;
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
