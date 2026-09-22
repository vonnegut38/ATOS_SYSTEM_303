#ifndef ASF_HARDWARE_H
#define ASF_HARDWARE_H
#include <stdint.h>

#define REG8(a)  (*(volatile uint8_t *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define SHIFTER_BASE_HI  0xffff8201UL
#define SHIFTER_BASE_MID 0xffff8203UL
#define SHIFTER_BASE_LO  0xffff820dUL
#define SHIFTER_PALETTE  0xffff8240UL
#define SHIFTER_RES      0xffff8260UL
#define MMU_MEMCONF      0xffff8001UL
#define IKBD_ACIA_CTRL   0xfffffc00UL
#define IKBD_ACIA_DATA   0xfffffc02UL
#define MIDI_ACIA_CTRL   0xfffffc04UL
#define MIDI_ACIA_DATA   0xfffffc06UL
#define MFP_GPIP         0xfffffa01UL
#define MFP_IERA         0xfffffa07UL
#define MFP_TCDCR        0xfffffa1dUL
#define MFP_TCDR         0xfffffa23UL
#define MSTE_SCU_GPR1    0xffff8e09UL
#define MSTE_CACHE_CTRL  0xffff8e21UL
#define TT_PALETTE       0xffff8400UL
#define FALCON_HHT       0xffff8282UL
#define FALCON_BUS_CTRL  0xffff8007UL
#define BLITTER_SRC_XINC 0xffff8a20UL
#define BLITTER_SRC_YINC 0xffff8a22UL
#define BLITTER_SRC_ADDR 0xffff8a24UL
#define BLITTER_ENDMASK1 0xffff8a28UL
#define BLITTER_ENDMASK2 0xffff8a2aUL
#define BLITTER_ENDMASK3 0xffff8a2cUL
#define BLITTER_DST_XINC 0xffff8a2eUL
#define BLITTER_DST_YINC 0xffff8a30UL
#define BLITTER_DST_ADDR 0xffff8a32UL
#define BLITTER_XCOUNT   0xffff8a36UL
#define BLITTER_YCOUNT   0xffff8a38UL
#define BLITTER_HOP      0xffff8a3aUL
#define BLITTER_OP       0xffff8a3bUL
#define BLITTER_CTRL     0xffff8a3cUL
#define BLITTER_SKEW     0xffff8a3dUL
#define MEGA_RTC_SECONDS 0xfffffc21UL
#define MEGA_RTC_BASE    0xfffffc20UL
#define NVRAM_SELECT     0xffff8961UL
#define NVRAM_DATA       0xffff8963UL
#define DMA_DATA         0xffff8604UL
#define DMA_CONTROL      0xffff8606UL
#define DMA_ADDR_HIGH    0xffff8609UL
#define DMA_ADDR_MID     0xffff860bUL
#define DMA_ADDR_LOW     0xffff860dUL

#define ROM_ISA_READ_BASE   0xfffa0000UL
#define ROM_ISA_WRITE_BASE  0xfffb0000UL
#define ROM_ISA_ADDR(port)  (ROM_ISA_READ_BASE+(((uint32_t)(port)&0x7fUL)<<9))

enum { SCREEN_BASE = 0x00010000UL, SCREEN_BYTES = 32000UL };

typedef enum {
    VIDEO_ST_LOW = 0,
    VIDEO_ST_MEDIUM = 1,
    VIDEO_ST_MONO = 2
} video_mode_t;

int probe_read8(uint32_t address, uint8_t *value);
int probe_read16(uint32_t address, uint16_t *value);
int probe_read32(uint32_t address, uint32_t *value);
int probe_rw32(uint32_t address);
int cpu_probe_vbr(void);
int cpu_probe_extb(void);
int cpu_probe_tc(void);
int cpu_probe_urp(void);
int cpu_probe_pcr(void);
int cpu_probe_030_cacr(void);
void exception_install(void);
void system_image_cache_sync(uint32_t cpu_type);
#endif
