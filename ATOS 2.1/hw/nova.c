#include "hardware.h"
#include "system.h"

/* VGA register windows of the graphics-card families ATOS knows about.  The
   order is deliberate: the ST-class window first, because that is the one a
   Mega ST (24-bit bus, VOFA/STGA board) and the emulated card answer on, then
   the VME positions, then the Falcon.  Every probe is bus-error protected, so a
   missing window costs one caught fault and nothing else. */
#define NOVA_STGA_IO      0x00d00000UL
#define NOVA_STGA_MEM     0x00c00000UL
#define NOVA_VME_IO       0xfedc0000UL
#define NOVA_VME_MEM      0xfec00000UL
#define NOVA_VME32_IO     0xfe900000UL
#define NOVA_VME32_MEM    0xfe800000UL
#define CRAZYDOTS_IO      0xfebf0000UL
#define FALCON_VME_IO     0xffb00000UL
#define FALCON_VME_MEM    0xffa00000UL
#define VGA_VIDSUB        0x03c3UL
#define MACH64_CHIP_ID    0x6eecUL

typedef struct {
    uint32_t io,mem;
    uint8_t adapter,mem_kind;
    uint8_t needs_vme;      /* only meaningful on a machine with a VME bus */
    uint8_t needs_videl;    /* only meaningful on a Falcon */
} nova_candidate_t;

static const nova_candidate_t candidates[]={
    {NOVA_STGA_IO,   NOVA_STGA_MEM,   NOVA_ADAPTER_VOFA,      NOVA_MEM_STGA,   0u,0u},
    {NOVA_VME_IO,    NOVA_VME_MEM,    NOVA_ADAPTER_VME,       NOVA_MEM_VME,    1u,0u},
    {NOVA_VME32_IO,  NOVA_VME32_MEM,  NOVA_ADAPTER_VME32,     NOVA_MEM_VME32,  1u,0u},
    {CRAZYDOTS_IO,   NOVA_VME_MEM,    NOVA_ADAPTER_CRAZYDOTS, NOVA_MEM_VME,    1u,0u},
    {FALCON_VME_IO,  FALCON_VME_MEM,  NOVA_ADAPTER_FALCON,    NOVA_MEM_FALCON, 0u,1u}
};

/* A read-only presence probe: the video-subsystem register answers on every
   VGA-compatible adapter and no Atari motherboard register decodes there. */
static int present_at(uint32_t io)
{
    uint8_t value;
    return probe_read8(io+VGA_VIDSUB,&value);
}

/* Which machine classes each candidate may be probed on.  Probing a VME window
   on an ST would only ever fault, and the Falcon window is decoded by Videl
   hardware, so neither is attempted outside its machine family. */
static int candidate_allowed(const nova_candidate_t *c,const system_info_t *info)
{
    if(c->needs_vme&&info->machine_type!=MACHINE_MEGA_STE&&
       info->machine_type!=MACHINE_TT)return 0;
    if(c->needs_videl&&info->machine_type!=MACHINE_FALCON)return 0;
    if(!c->needs_vme&&!c->needs_videl&&info->machine_type==MACHINE_FALCON)return 0;
    return 1;
}

static uint8_t identify_chip(uint32_t io,uint32_t mem)
{
    uint8_t a,b;
    if(probe_read8(io+MACH64_CHIP_ID,&a)&&a==0xd7u)return NOVA_CHIP_MACH64;
    /* Mach32 cards carry the ATI magic "62" in the video BIOS mapped through
       the card's MEMORY aperture - not through the register window. */
    if(probe_read8(mem+0x0c0032UL,&a)&&probe_read8(mem+0x0c0034UL,&b)&&
       a=='6'&&b=='2')return NOVA_CHIP_MACH32;
    /* Distinguishing plain ET4000 from W32 requires indexed-register writes.
     * Keep discovery read-only until the guarded VGA initializer is active. */
    return NOVA_CHIP_ET4000_FAMILY;
}

/* Find the adapter and remember WHERE it answered.  The memory aperture is part
   of the result: with 12 MB or more of ST-RAM the 0x00C00000 window is ordinary
   RAM and the card simply is not there, so the card must not be programmed
   through it (see nova_mem_base_is_supported()). */
void nova_detect(system_info_t *info)
{
    unsigned i;
    info->nova_present=0;info->nova_adapter=NOVA_ADAPTER_NONE;
    info->nova_chipset=NOVA_CHIP_NONE;info->nova_mem_kind=NOVA_MEM_NONE;
    info->nova_io_base=0;info->nova_mem_base=0;
    info->video_preference=VIDEO_PREF_AUTO;info->video_active=VIDEO_ACTIVE_SHIFTER;
    for(i=0;i<sizeof(candidates)/sizeof(candidates[0]);++i){
        const nova_candidate_t *c=&candidates[i];
        if(!candidate_allowed(c,info))continue;
        if(!present_at(c->io))continue;
        info->nova_present=1;info->nova_adapter=c->adapter;
        info->nova_mem_kind=c->mem_kind;
        info->nova_io_base=c->io;info->nova_mem_base=c->mem;
        /* The Mach32 ID lives in the video BIOS mapped through the memory
           aperture; it says nothing on the ST-class window. */
        info->nova_chipset=(c->mem_kind==NOVA_MEM_STGA)?NOVA_CHIP_ET4000_FAMILY
                                                      :identify_chip(c->io,c->mem);
        return;
    }
}

/* True when the ROM's VOFA/ET4000 driver may program the detected adapter: it
   speaks one aperture only.  A card found through a different window is
   reported, but left alone. */
int nova_mem_base_is_supported(const system_info_t *info)
{
    return info->nova_present&&info->nova_mem_base==NOVA_STGA_MEM;
}

const char *nova_adapter_name(uint8_t type)
{
    if(type==NOVA_ADAPTER_VME)return "NOVA VME";
    if(type==NOVA_ADAPTER_CRAZYDOTS)return "CRAZYDOTS VME";
    if(type==NOVA_ADAPTER_VOFA)return "VOFA/VOLKSFARBEN";
    if(type==NOVA_ADAPTER_VME32)return "NOVA VME (TT)";
    if(type==NOVA_ADAPTER_FALCON)return "NOVA VME (FALCON)";
    return "NOT DETECTED";
}

const char *nova_chipset_name(uint8_t type)
{
    if(type==NOVA_CHIP_MACH64)return "ATI MACH64";
    if(type==NOVA_CHIP_MACH32)return "ATI MACH32";
    if(type==NOVA_CHIP_ET4000_FAMILY)return "ET4000/W32 FAMILY";
    return "---";
}
