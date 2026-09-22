#ifndef ASF_SYSTEM_H
#define ASF_SYSTEM_H
#include <stdint.h>
#include "hardware.h"
#include "rtc.h"
typedef struct {
    uint8_t machine_type;
    uint8_t cpu_type;
    uint8_t fpu_type;
    uint8_t mega_ste;
    uint8_t clock_mhz;
    uint8_t clock_known;
    uint8_t cache_enabled;
    uint8_t keyboard_present;
    uint8_t video_mode;
    uint8_t blitter_present;
    uint8_t blitter_enabled;
    uint8_t rtc_type;
    uint8_t netusbee_ethernet;
    uint8_t netusbee_usb;
    uint8_t nova_present;
    uint8_t nova_adapter;
    uint8_t nova_chipset;
    uint8_t video_preference;
    uint8_t video_active;
    uint8_t acsi_devices;
    uint8_t acsi_scanned;
    uint8_t acsi_root_read;
    uint8_t acsi_root_id;
    uint8_t acsi_root_bootable;
    uint8_t acsi_partitions;
    char acsi_part_id[4][4];
    uint32_t acsi_part_start[4];
    uint8_t boot_acsi_id;
    uint8_t boot_partition;
    uint8_t boot_selection_valid;
    uint32_t ram_bytes;
    uint32_t tt_ram_bytes;
    rtc_datetime_t datetime;
} system_info_t;
enum { MACHINE_ST, MACHINE_MEGA_ST, MACHINE_STE, MACHINE_MEGA_STE,
       MACHINE_TT, MACHINE_FALCON };
enum { CPU_68000, CPU_68010, CPU_68020, CPU_68030, CPU_68040, CPU_68060 };
enum { VIDEO_PREF_AUTO, VIDEO_PREF_SHIFTER, VIDEO_PREF_NOVA };
enum { VIDEO_ACTIVE_SHIFTER, VIDEO_ACTIVE_NOVA };
enum { NOVA_ADAPTER_NONE, NOVA_ADAPTER_VME, NOVA_ADAPTER_CRAZYDOTS,
       NOVA_ADAPTER_VOFA };
enum { NOVA_CHIP_NONE, NOVA_CHIP_ET4000_FAMILY, NOVA_CHIP_MACH32,
       NOVA_CHIP_MACH64 };
const char *machine_name(uint8_t type);
extern system_info_t g_system;
void machine_detect(system_info_t *info);
void expansions_detect(system_info_t *info);
void nova_detect(system_info_t *info);
const char *nova_adapter_name(uint8_t type);
const char *nova_chipset_name(uint8_t type);
uint8_t acsi_detect_devices(void);
uint8_t acsi_detect_first_device(void);
int acsi_probe_device(uint8_t id);
int acsi_inquiry_name(uint8_t id,char *name,uint8_t capacity);
int acsi_inspect_first(system_info_t *info);
int acsi_inspect_device(system_info_t *info,uint8_t id);
int acsi_read_sector(uint8_t id,uint32_t sector,void *buffer);
int machine_set_clock(uint8_t mhz);
uint32_t memory_detect(void);
uint32_t memory_detect_fast(void);
void shifter_init(void);
video_mode_t shifter_detect_mode(void);
const char *shifter_mode_name(video_mode_t mode);
uint16_t shifter_text_columns(void);
uint16_t shifter_line_bytes(void);
uint16_t shifter_group_bytes(void);
void ikbd_init(void);
int ikbd_scancode(void);
int ikbd_mouse_event(int8_t *dy, uint8_t *buttons);
void post_run(void);
void post_draw_choices(uint8_t selected);
void setup_run(void);
void diagnostics_run(void);
void hardware_inventory_run(void);
void boot_menu_run(void);
void tos_menu_run(void);
void boot_stage2_run(void);
void post_draw_countdown(uint8_t seconds);
#endif
