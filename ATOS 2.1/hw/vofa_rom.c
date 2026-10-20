/* ROM-side bring-up of the VOFA/ET4000 adapter (milestone: boot on VGA).
 *
 * Reuses the guarded register sequence from system/vofa_et4000.c, which the
 * kernel module VOFA.SYS also links. Bus-error-protected accesses come from
 * boot/exception.S (probe_read8/probe_write8); the clock check needs a 50 Hz
 * tick, taken here from MFP timer D instead of the kernel TIMER module.
 */
#include "hardware.h"
#include "system.h"
#include "console.h"
#include "vofa_et4000.h"
#include "config.h"

/* 0x101 = gate declined, 0x102 = gate passed, 0x200 = initialising,
   0x300|vofa_et4000_init() result = finished (0x301 = VOFA_OK). */
/* 0x101 = gate declined, 0x102 = gate passed, 0x200 = initialising,
   0x300|vofa_et4000_init() result = finished (0x301 = VOFA_OK). */
volatile uint32_t vofa_rom_status;
static uint8_t tick_control,tick_data,tick_last;
static uint32_t tick_count;

static int rom_read(uint32_t address,uint8_t *value){return probe_read8(address,value);}
static int rom_write(uint32_t address,uint32_t value){return probe_write8(address,value);}
static void rom_delay(void){volatile unsigned i;for(i=0;i<64u;i++)__asm__ volatile("nop");}
/* Timer D counts DOWN at 2.4576 MHz / 200 = 12288 Hz and reloads from 246,
   so one 50 Hz period is the 246 -> 0 wrap, not every register change. */
static uint32_t rom_ticks(void)
{
    uint8_t now=REG8(MFP_TCDR);
    if(now>tick_last)++tick_count;
    tick_last=now;
    return tick_count;
}
/* Same 2.4576 MHz / 200 / 246 ~= 50 Hz source the boot countdown uses. */
static void ticks_begin(void)
{
    tick_control=REG8(MFP_TCDCR);tick_data=REG8(MFP_TCDR);
    REG8(MFP_TCDCR)=(uint8_t)((tick_control&0x0fu)|0x70u);
    REG8(MFP_TCDR)=246u;
    tick_last=REG8(MFP_TCDR);tick_count=0;
}
static void ticks_end(void)
{
    REG8(MFP_TCDCR)=(uint8_t)(tick_control&0x0fu);
    REG8(MFP_TCDR)=tick_data;
    REG8(MFP_TCDCR)=tick_control;
}

int vofa_rom_init(void)
{
    static const vofa_io_t io={rom_read,rom_write,rom_delay,rom_ticks,0,
                               vofa_et4000_vram_fill};
    int result;uint8_t want;
    /* The setup menu stores the card mode in BIOS.CFG; bring the adapter up in
       it so the WHOLE boot log - the text mode - runs at that resolution.
       The console renders both planar mono and 8 bpp chunky glyphs, so the
       256-colour mode is a valid text mode too. */
    switch(g_config.video_mode){
        case CFG_MODE_800:want=VOFA_MODE_800;break;
        case CFG_MODE_1024:want=VOFA_MODE_1024;break;
        case CFG_MODE_800_8:want=VOFA_MODE_800_8;break;
        /* CFG_MODE_640 is 640x400 MONO, the ST Shifter's high-res geometry.
           On the adapter the native scanout is 800x600 - the same geometry the
           kernel and desktop use - so an ST-high configuration must bring the
           CARD up at 800x600 mono.  Programming 640x400 left the adapter
           scanning one geometry while the system used another, and the entire
           boot screen (POST, logo, SETUP) was invisible: the desktop only
           appeared because the kernel switches to 800x600 itself. */
        case CFG_MODE_640:want=VOFA_MODE_800;break;
        default:want=VOFA_MODE_640;break;
    }
    ticks_begin();
    vofa_rom_status=0x200u;
    result=vofa_et4000_init(&io,want);
    ticks_end();
    vofa_rom_status=0x300u|(uint32_t)result;
    if(result==VOFA_OK){
        /* Tell the console about the card geometry: pitch, frame size and the
           80-column grid all follow the programmed mode from here on. */
        const vofa_mode_info_t *m=vofa_et4000_mode(want);
        uint8_t eight=(uint8_t)(want==VOFA_MODE_800_8);
        if(m)shifter_set_card_mode((uint16_t)(eight?m->width:(m->width+7u)/8u),
                                   m->bytes,(uint16_t)(eight?1u:2u),
                                   (uint16_t)(m->width/8u),16u,eight);
    }
    return result;
}
