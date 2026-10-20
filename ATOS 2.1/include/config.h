#ifndef ASF_CONFIG_H
#define ASF_CONFIG_H
#include <stdint.h>

#define CONFIG_MAGIC   0x41534643UL   /* 'ASFC' */
#define CONFIG_VERSION 2u             /* v10: boot_device/video_driver/video_mode/... */
#define CFG_VIDEO_AUTO 0u
#define CFG_VIDEO_SHIFTER 1u
#define CFG_VIDEO_ET4000 2u
/* video_mode values.  0..2 are the ST Shifter modes; 3 is the reserved
   800x600x256 entry the Nova/VOFA driver programs (chain-4), and 4/5 are the
   larger mono card modes the ROM can bring up before the kernel runs. */
#define CFG_MODE_640   2u   /* 640x400 mono (ST high) */
#define CFG_MODE_800_8 3u   /* 800x600, 256 colours */
#define CFG_MODE_800   4u   /* 800x600 mono */
#define CFG_MODE_1024  5u   /* 1024x768 mono */
#define CFG_BOOT_AUTO 0u
#define CFG_BOOT_ACSI 1u
#define CFG_BOOT_IDE 2u

/* Persistent BIOS configuration: \BIOS\BIOS.CFG on the boot volume
 * (fallback \BIOS.CFG), validated by magic/version/size/CRC. */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
    uint8_t  boot_device;   /* 0 auto, 1 acsi, 2 ide */
    uint8_t  video_driver;  /* 0 auto, 1 shifter, 2 et4000 */
    uint8_t  video_mode;    /* 0 low, 1 medium, 2 high, 3 vga8 */
    uint8_t  safe_mode;     /* 0/1 : minimal boot (no GUI) */
    uint8_t  debug_level;   /* 0..3 */
    uint8_t  language[4];   /* "EN", "PL", ... */
    uint8_t  blitter;       /* 0/1 */
    uint8_t  boot_delay;    /* seconds */
    uint8_t  boot_id;       /* specific device id (bit 7 = IDE) */
    uint8_t  boot_part;     /* partition slot 0..3 */
    uint8_t  boot_valid;    /* 1 = use boot_id/boot_part */
    uint8_t  auto_tos;      /* 1 = boot the first ROM TOS/EmuTOS image via MMU */
    uint8_t  video_pending; /* 1 = a card boot is in progress and unconfirmed */
    uint8_t  cpu_clock_mhz; /* user clock for machines with no readable clock (TF536) */
    /* Boot priority: device ids in order (0x00..0x07 ACSI, 0x80..0x81 IDE,
       0xfe floppy, 0xff = end of list).  Empty/valid=0 falls back to the
       boot_device order (ACSI-first or IDE-first) and then the floppy. */
    uint8_t  boot_order[4];
    uint8_t  boot_order_valid;
    uint8_t  reserved[94];
} bios_config_t;

_Static_assert(sizeof(bios_config_t)==128u,"bios_config_t must stay 128 bytes");

extern bios_config_t g_config;
extern uint8_t g_config_loaded;
extern uint8_t g_config_saved;
extern uint8_t g_config_error;

/* Pure helpers live in the header so they are host-testable without storage. */
static inline uint32_t cfg_crc(const bios_config_t *c)
{
    const uint8_t *p=(const uint8_t *)c;uint32_t i,sum=0u;
    for(i=0u;i<sizeof(*c);++i){
        if(i>=8u&&i<12u)continue;             /* CRC field itself is skipped */
        sum=(uint32_t)((sum<<1)|(sum>>31));
        sum^=(uint32_t)p[i];
    }
    return sum;
}
static inline void cfg_defaults(bios_config_t *c)
{
    uint8_t *p=(uint8_t *)c;uint32_t i;
    for(i=0u;i<sizeof(*c);++i)p[i]=0u;
    c->magic=CONFIG_MAGIC;c->version=CONFIG_VERSION;c->size=(uint16_t)sizeof(*c);
    c->boot_device=CFG_BOOT_AUTO;c->video_driver=CFG_VIDEO_AUTO;c->video_mode=2u;
    c->safe_mode=0u;c->debug_level=0u;
    c->language[0]='E';c->language[1]='N';c->language[2]=0u;c->language[3]=0u;
    c->blitter=1u;c->boot_delay=2u;c->boot_valid=0u;c->auto_tos=0u;
}
static inline int cfg_validate(const bios_config_t *c)
{
    return c&&c->magic==CONFIG_MAGIC&&c->version==CONFIG_VERSION&&
           c->size==(uint16_t)sizeof(*c)&&c->crc==cfg_crc(c);
}
int cfg_load(void);
int cfg_save(void);
#endif