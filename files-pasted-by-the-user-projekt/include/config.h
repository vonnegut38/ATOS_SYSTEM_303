#ifndef ASF_CONFIG_H
#define ASF_CONFIG_H
#include <stdint.h>

#define CONFIG_MAGIC   0x41534643UL   /* 'ASFC' */
#define CONFIG_VERSION 1u
#define CONFIG_FILE_SIZE 64u

/* Persistent BIOS configuration.  Stored as a small file on the boot volume
 * (\BIOS.CFG on FAT16) and validated with a magic/version/CRC triple. */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
    uint8_t  video_pref;   /* 0 auto, 1 shifter, 2 nova */
    uint8_t  blitter;      /* 0/1 */
    uint8_t  boot_delay;   /* seconds */
    uint8_t  boot_device;  /* 0 auto, 1 acsi, 2 ide */
    uint8_t  fast_boot;    /* 0/1 */
    uint8_t  beep;         /* 0/1 */
    uint8_t  boot_id;      /* specific boot device id (bit 7 = IDE) */
    uint8_t  boot_part;    /* partition slot 0..3 */
    uint8_t  boot_valid;   /* 1 = use boot_id/boot_part */
    uint8_t  reserved[43];
} bios_config_t;

extern bios_config_t g_config;
extern uint8_t g_config_loaded;   /* 1 = valid file found and used */
extern uint8_t g_config_saved;    /* 1 = defaults persisted to disk */
extern uint8_t g_config_error;    /* 0 none, else the failing cfg_load step */
extern uint8_t g_config_device;   /* boot device id used by the store */
extern uint8_t g_config_location; /* 0 = root \BIOS.CFG, 1 = \BIOS\BIOS.CFG */

uint32_t cfg_crc(const bios_config_t *config);
void cfg_defaults(void);
int  cfg_load(void);   /* 1 when a valid file was loaded, 0 for defaults */
int  cfg_save(void);   /* 1 when written to disk */

#endif
