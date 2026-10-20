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
    uint8_t nova_mem_kind;     /* NOVA_MEM_*: aperture the card answers on */
    uint32_t nova_io_base;     /* VGA register window the probe answered on */
    uint32_t nova_mem_base;    /* VRAM window that goes with nova_io_base */
    uint8_t nova_initialized; /* ROM brought the adapter up before the POST */
    uint8_t video_preference;
    uint8_t video_active;
    uint8_t video_force_shifter; /* Alt at power-on or SETUP: stay on the Shifter */
    uint8_t acsi_devices;
    uint8_t ide_units;      /* bit0/1 = IDE master/slave present */
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
/* Detected adapter family.  VME32 is the Nova/Mach position on a TT
   (0xFE900000), VME the Mega STE/TT one (0xFEDC0000) and FALCON the Falcon one
   (0xFFB00000); VOFA covers the Volksfarben/STGA and ST-class mapping the
   emulated card and a Mega ST + VOFA board both answer on (0x00D00000). */
enum { NOVA_ADAPTER_NONE, NOVA_ADAPTER_VME, NOVA_ADAPTER_CRAZYDOTS,
       NOVA_ADAPTER_VOFA, NOVA_ADAPTER_VME32, NOVA_ADAPTER_FALCON };
/* Memory aperture the detected adapter answers on.  A driver that knows only
   one aperture must refuse to program a card found through another one,
   instead of writing to whatever else occupies those addresses. */
enum { NOVA_MEM_NONE, NOVA_MEM_VME32, NOVA_MEM_VME, NOVA_MEM_FALCON,
       NOVA_MEM_STGA };
enum { NOVA_CHIP_NONE, NOVA_CHIP_ET4000_FAMILY, NOVA_CHIP_MACH32,
       NOVA_CHIP_MACH64 };
const char *machine_name(uint8_t type);
extern system_info_t g_system;
void machine_detect(system_info_t *info);
void expansions_detect(system_info_t *info);
void nova_detect(system_info_t *info);
/* 1 when the ROM's VOFA/ET4000 driver may program the detected adapter (it
   speaks the 0x00C00000/0x00D00000 aperture only). */
int nova_mem_base_is_supported(const system_info_t *info);
const char *nova_adapter_name(uint8_t type);
const char *nova_chipset_name(uint8_t type);
uint8_t acsi_detect_devices(void);
uint8_t acsi_detect_first_device(void);
int acsi_probe_device(uint8_t id);
int acsi_inquiry_name(uint8_t id,char *name,uint8_t capacity);
int acsi_inspect_first(system_info_t *info);
int acsi_inspect_device(system_info_t *info,uint8_t id);
int acsi_read_sector(uint8_t id,uint32_t sector,void *buffer);
int acsi_write_sector(uint8_t id,uint32_t sector,const void *buffer);
int machine_set_clock(uint8_t mhz);
uint32_t memory_detect(void);
uint32_t memory_detect_fast(void);
void shifter_init(void);
video_mode_t shifter_detect_mode(void);
const char *shifter_mode_name(video_mode_t mode);
uint16_t shifter_text_columns(void);
uint16_t shifter_line_bytes(void);
/* Active frame size in bytes (Shifter mode or the card's programmed mode). */
uint32_t shifter_frame_bytes(void);
/* Adopt a graphics-card mode in the console geometry (line_bytes 0 restores the
   Shifter geometry). */
void shifter_set_card_mode(uint16_t line_bytes,uint32_t frame_bytes,
                           uint16_t group_bytes,uint16_t columns,uint16_t cell_height,
                           uint8_t chunky);
void shifter_clear_card_mode(void);
/* 1 when the active card mode is 8 bits per pixel (one byte per pixel). */
uint8_t shifter_chunky(void);
uint16_t shifter_group_bytes(void);
void ikbd_init(void);
int ikbd_scancode(void);
int ikbd_mouse_event(int8_t *dy, uint8_t *buttons);
void post_run(void);
int post_tests_failed(void);   /* 1 = a POST self-test failed (emergency screen) */
void post_draw_choices(uint8_t selected);
void setup_run(void);
void diagnostics_run(void);
void hardware_inventory_run(void);
void boot_menu_run(void);
void tos_menu_run(void);
void boot_stage2_run(void);
/* Warm reset: reset peripherals and re-enter the ROM startup. */
void bios_reset(void);
/* Called from the kernel handoff once a card boot reached the shell: clear the
   persistent failover latch using the ROM's own disk write path. */
void bios_video_boot_ok(void);
void post_draw_countdown(uint8_t seconds);
#endif
