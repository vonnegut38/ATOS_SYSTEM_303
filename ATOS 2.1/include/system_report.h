#ifndef SYSTEM_REPORT_H
#define SYSTEM_REPORT_H
#include <stdint.h>
#define SYSREPORT_MAGIC 0x53494e46UL
#define SYSREPORT_VERSION 4u
#define SYSREPORT_VOLUMES 14u
/* All quantities have explicit units; unknown clock/capacity are flagged. */
typedef struct {uint32_t start,sectors,boot;char id[4];} sys_volume_t;
typedef struct {
 uint32_t magic,version,size,machine,cpu,fpu,clock_mhz,clock_source;
 uint32_t st_total,st_free,st_largest,tt_total,tt_free,tt_largest,tt_reserved;
 uint32_t active_video,detected_adapter,detected_chip,hardware_report;
 uint32_t width,height,output_width,output_height,planes,blitter;
 uint32_t bus,device,disk_sectors,sector_bytes,disk_capacity_known;
 uint32_t cache_sectors,cache_tt,volumes,tick_hz,wall_ticks,boot_start,boot_sectors;
 uint32_t netusbee_ethernet,netusbee_usb;
 /* Compositor work counters: the only way to optimise by numbers. */
 uint32_t wm_frames,wm_damage_pixels,wm_console_cells;
 /* Requests-per-frame (v3): total serviced, last composed frame, peak. */
 uint32_t wm_requests_total,wm_requests_last,wm_requests_max;
 /* Work counters (v4): full traversals of the kernel block list, and set/way
    probes in the block cache. These say whether a hot path is doing the work
    once or twice, which is what the allocator and cache changes are about. */
 uint32_t mem_list_walks,blk_searches;
 char disk_name[32];sys_volume_t volume[SYSREPORT_VOLUMES];
} system_report_t;
/* tt_reserved is the TT-RAM tail above 0x04000000 kept for the TOS TPA, so
   usable TT RAM is tt_total-tt_reserved. */
_Static_assert(sizeof(system_report_t)==444,"system report ABI");
int kernel_system_report(system_report_t *out);
#endif
