#include "hardware.h"
#include "system.h"

#define NOVA_VME_IO       0xfedc0000UL
#define NOVA_VME_MEM      0xfec00000UL
#define CRAZYDOTS_IO      0xfebf0000UL
#define VOFA_IO           0x00d00000UL
#define VOFA_MEM          0x00c00000UL
#define VGA_VIDSUB        0x03c3UL
#define MACH64_CHIP_ID    0x6eecUL

static int present_at(uint32_t io)
{
    uint8_t value;
    return probe_read8(io+VGA_VIDSUB,&value);
}

static uint8_t identify_chip(uint32_t io,uint32_t mem)
{
    uint8_t a,b;
    if(probe_read8(io+MACH64_CHIP_ID,&a)&&a==0xd7u)return NOVA_CHIP_MACH64;
    if(probe_read8(mem+0x0c0032UL,&a)&&probe_read8(mem+0x0c0034UL,&b)&&a=='6'&&b=='2')return NOVA_CHIP_MACH32;
    /* Distinguishing plain ET4000 from W32 requires indexed-register writes.
     * Keep discovery read-only until the guarded VGA initializer is active. */
    return NOVA_CHIP_ET4000_FAMILY;
}

void nova_detect(system_info_t *info)
{
    uint32_t io=0,mem=0;
    uint8_t dummy;
    info->nova_present=0;info->nova_adapter=NOVA_ADAPTER_NONE;info->nova_chipset=NOVA_CHIP_NONE;
    info->video_preference=VIDEO_PREF_AUTO;info->video_active=VIDEO_ACTIVE_SHIFTER;
    if((info->machine_type==MACHINE_MEGA_STE||info->machine_type==MACHINE_TT)&&present_at(NOVA_VME_IO)){
        io=NOVA_VME_IO;mem=NOVA_VME_MEM;info->nova_adapter=NOVA_ADAPTER_VME;
    } else if((info->machine_type==MACHINE_MEGA_STE||info->machine_type==MACHINE_TT)&&present_at(CRAZYDOTS_IO)){
        io=CRAZYDOTS_IO;mem=NOVA_VME_MEM;info->nova_adapter=NOVA_ADAPTER_CRAZYDOTS;
    } else if(info->ram_bytes<0x00c00000UL&&present_at(VOFA_IO)&&probe_read8(VOFA_MEM,&dummy)){
        io=VOFA_IO;mem=VOFA_MEM;info->nova_adapter=NOVA_ADAPTER_VOFA;
    }
    if(io){info->nova_present=1;info->nova_chipset=identify_chip(io,mem);}
}

const char *nova_adapter_name(uint8_t type)
{
    if(type==NOVA_ADAPTER_VME)return "NOVA VME";
    if(type==NOVA_ADAPTER_CRAZYDOTS)return "CRAZYDOTS VME";
    if(type==NOVA_ADAPTER_VOFA)return "VOFA/VOLKSFARBEN";
    return "NOT DETECTED";
}

const char *nova_chipset_name(uint8_t type)
{
    if(type==NOVA_CHIP_MACH64)return "ATI MACH64";
    if(type==NOVA_CHIP_MACH32)return "ATI MACH32";
    if(type==NOVA_CHIP_ET4000_FAMILY)return "ET4000/W32 FAMILY";
    return "---";
}
