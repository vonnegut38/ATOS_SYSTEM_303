#include "hardware.h"
#include "system.h"
#include "console.h"

#define NOVA_VME_IO       0xfedc0000UL
#define NOVA_VME_MEM      0xfec00000UL
#define CRAZYDOTS_IO      0xfebf0000UL
#define VOFA_IO           0x00d00000UL
#define VOFA_MEM          0x00c00000UL
/* STGA (Till Harbaum): VRAM 0x00C00000, VGA registers 0x00CF0000 + port,
 * 16-bit I/O. The adapter decodes only A0-A23 because it sits on the ST's
 * 24-bit bus, so the documented 0xFEC00000/0xFECF0000 forms are the same
 * windows with the top eight address bits ignored; software uses the 24-bit
 * form so that both a 68000 and a 68030 reach the card. */
#define STGA_IO           0x00cf0000UL
#define STGA_MEM          0x00c00000UL
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


/* ---------------------------------------------------------------------------
 * ET4000: wejscie w tryb VGA 640x480x256 i wzorzec kontrolny.
 *
 * Uklad VRAM jest liniowy: 640 bajtow na linie, 1 bajt = 1 piksel. Rejestry
 * ustawiamy tak, by odczytywaly sie spojnie (offset w slowach 32-bit,
 * 8 bpp) - nasz system i emulator musza zgadzac sie co do geometrii.
 * ------------------------------------------------------------------------ */
#define ET4000_IO   0x00d00000UL
#define ET4000_MEM  0x00c00000UL

static void et4000_reg(uint16_t port, uint8_t value)
{
    REG8(ET4000_IO + port) = value;
}

static void et4000_palette(void)
{
    uint16_t i;
    et4000_reg(0x3c8u, 0u);
    for(i=0u;i<256u;++i)
    {
        /* 8 palet: RGBA w stylu 6-bit DAC */
        et4000_reg(0x3c9u,(uint8_t)(((i>>5)&7u)*9u));
        et4000_reg(0x3c9u,(uint8_t)(((i>>2)&7u)*9u));
        et4000_reg(0x3c9u,(uint8_t)((i&3u)*21u));
    }
}

static void et4000_pattern(void)
{
    /* 8 pionowych pasow + poziomy pasek na gorze - latwo rozpoznac na zrzucie */
    uint16_t y, x;
    for(y=0u;y<480u;++y)
        for(x=0u;x<80u;++x)
            REG8(ET4000_MEM + (uint32_t)y*80u + x) = (uint8_t)((y<32u)?0xffu:((x&1u)?0xffu:0x00u));
}

static void et4000_enter_vga(void)
{
    /* sekwencer */
    et4000_reg(0x3c4u,0x00u); et4000_reg(0x3c5u,0x01u);
    et4000_reg(0x3c4u,0x01u); et4000_reg(0x3c5u,0x01u);
    et4000_reg(0x3c4u,0x02u); et4000_reg(0x3c5u,0x0fu);
    et4000_reg(0x3c4u,0x03u); et4000_reg(0x3c5u,0x00u);
    et4000_reg(0x3c4u,0x04u); et4000_reg(0x3c5u,0x06u);  /* 1 bpp, 80 B/linia - jak w EmuTOS */
    /* CRTC: offset 0xA0 slow 32-bit = 640 bajtow, 480 linii */
    et4000_reg(0x3d4u,0x13u); et4000_reg(0x3d5u,0x28u);  /* 40 slow = 80 bajtow */
    et4000_reg(0x3d4u,0x17u); et4000_reg(0x3d5u,0xa3u);  /* bez trybu slow 32-bit */
    /* CRTC 0x14: byte addressing.  video8_enter() sets bit 6 (doubleword) for
       the 8bpp desktop; leaving it set here makes the text mode count 160-byte
       lines and the console font end up half width ("smaller characters"). */
    et4000_reg(0x3d4u,0x14u); et4000_reg(0x3d5u,0x00u);
    et4000_reg(0x3d4u,0x12u); et4000_reg(0x3d5u,0xdfu);  /* 480 linii: 0x1DF */
    et4000_reg(0x3d4u,0x07u); et4000_reg(0x3d5u,0x02u);  /* overflow -> bit 8 */
    /* kontroler graficzny: tryb 256 kolorow */
    et4000_reg(0x3ceu,0x05u); et4000_reg(0x3cfu,0x40u);
    et4000_reg(0x3ceu,0x06u); et4000_reg(0x3cfu,0x05u);
    et4000_palette();
    et4000_pattern();
    REG8(SHIFTER_RES) = 0x02u;    /* Shifter: tryb High 640x400 - zgodny z karta 1:1 */
    console_set_vga(1u);          /* od tego miejsca POST i shell pisza na karte */
}

void nova_enter_text_vga(void){et4000_enter_vga();}

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
        /* VOFA first: its register window 0x00D00000 is outside the 1 MB VRAM,
         * whereas 0x00CF0000 (the STGA register window) still lies inside the
         * VOFA VRAM and would make every VOFA card look like an STGA. */
        io=VOFA_IO;mem=VOFA_MEM;info->nova_adapter=NOVA_ADAPTER_VOFA;
    } else if(info->ram_bytes<0x00c00000UL&&present_at(STGA_IO)){
        io=STGA_IO;mem=STGA_MEM;info->nova_adapter=NOVA_ADAPTER_STGA;
    }
    if(io){info->nova_present=1;info->nova_chipset=identify_chip(io,mem);et4000_enter_vga();}
}

const char *nova_adapter_name(uint8_t type)
{
    if(type==NOVA_ADAPTER_VME)return "NOVA VME";
    if(type==NOVA_ADAPTER_CRAZYDOTS)return "CRAZYDOTS VME";
    if(type==NOVA_ADAPTER_VOFA)return "VOFA/VOLKSFARBEN";
    if(type==NOVA_ADAPTER_STGA)return "STGA/ET4000";
    return "NOT DETECTED";
}

const char *nova_chipset_name(uint8_t type)
{
    if(type==NOVA_CHIP_MACH64)return "ATI MACH64";
    if(type==NOVA_CHIP_MACH32)return "ATI MACH32";
    if(type==NOVA_CHIP_ET4000_FAMILY)return "ET4000/W32 FAMILY";
    return "---";
}
