#ifndef ASF_VOFA_ET4000_H
#define ASF_VOFA_ET4000_H
#include <stdint.h>
#define VOFA_IO 0x00d00000UL
#define VOFA_MEM 0x00c00000UL
/* Legacy 640x400 aperture (32000 B).  The widest mode (1024x768 mono) needs
   98,304 B; 1024x768x256 needs 786,432 B.  VOFA_MAX_BYTES documents the largest
   shadow/clear the driver has to handle. */
#define VOFA_BYTES 32000UL
/* 800x600x16 (65 536 kolorow) to 960 000 B, wiec najwiekszy tryb to wlasnie
   on - 1024x768x256 (786 432 B) jest mniejszy. */
#define VOFA_MAX_BYTES 960000UL
/* Mode indices.  The kernel and the OPTIONS menu speak these, and every table
   below is selected by one of them. */
enum {VOFA_MODE_640=0u,VOFA_MODE_800=1u,VOFA_MODE_1024=2u,VOFA_MODE_800_8=3u,
      VOFA_MODE_1024_8=4u,VOFA_MODE_640_8=5u,VOFA_MODE_640_4=6u,
      VOFA_MODE_800_16=7u,VOFA_MODE_1024_4=8u,VOFA_MODE_COUNT=9u};
/* RAMDAC families.  The Nova drivers ship one build per family (T0/T1/T3 =
   8-bit, T4/T6 = 15-bit, T8 = 24-bit) because the palette path differs; the
   driver probes the part and picks the matching sequence. */
enum {VOFA_DAC_UNKNOWN=0u,VOFA_DAC_8BIT=1u,VOFA_DAC_15BIT=2u,VOFA_DAC_24BIT=3u};
/* VoFa cards in the field mostly carry a CIC10487 (15-bit, Nova's T4), so that
   is what the driver uses when the probe cannot tell them apart. */
#define VOFA_DAC_DEFAULT VOFA_DAC_15BIT
/* Geometry of one mode, read back by the kernel after programming. */
typedef struct {uint16_t width,height;uint32_t bytes;} vofa_mode_info_t;
/* Every bus operation may fail; no card access bypasses these callbacks. */
typedef struct {
 int (*read)(uint32_t,uint8_t *);
 int (*write)(uint32_t,uint32_t);
 void (*delay)(void);
 uint32_t (*ticks)(void); /* polling clock, 50 Hz */
 /* Called once the adapter is scanning out but before the (slow) clock
    check, so the kernel can paint its console into the fresh VRAM and the
    mode takeover reveals text instead of a black gap. Optional. */
 void (*render)(void);
 /* Bulk aperture fill, appended last.  Programming the card clears up to
    480000 bytes (800x600x256) and one indirect write() per byte dominated the
    card boot; the hardware implementation (vofa_et4000_vram_fill) uses wide
    stores instead.  When NULL - the host test - the driver falls back to the
    byte-wise write() loop, so BOTH the fast and the generic path stay
    reachable and testable.  The direct-store decision stays in the bus
    implementation, never in the driver. Optional. */
 void (*fill)(uintptr_t,uint32_t,uint8_t);
} vofa_io_t;
/* Fast aperture fill: 32-bit stores plus a byte tail.  It lives in the header
   because the three link units that install it are separate: the ROM
   (hw/vofa_rom.c), the kernel (which hands the io struct to VOFA.SYS and does
   NOT link the driver object) and the host test.  Whoever offers the fast path
   owns the direct store; the driver only ever calls it through io->fill. */
static inline void vofa_et4000_vram_fill(uintptr_t address,uint32_t bytes,uint8_t value)
{
 volatile uint32_t *p=(volatile uint32_t *)(uintptr_t)address;
 uint32_t v=(uint32_t)value*0x01010101u,longs=bytes/4u,at;
 for(at=0;at<longs;++at)p[at]=v;
 for(at=longs*4u;at<bytes;++at)*(volatile uint8_t *)(uintptr_t)(address+at)=value;
}
enum {VOFA_OK=1,VOFA_ABSENT=0,VOFA_IO_ERROR=-1,VOFA_CLOCK_ERROR=-2,VOFA_MEMORY_ERROR=-3};
/* Name of a DAC family, for the boot log ("CIC10487 15-BIT"). */
const char *vofa_et4000_dac_name(uint8_t type);
/* Detect the RAMDAC family (override != VOFA_DAC_UNKNOWN forces it). */
int vofa_et4000_dac_detect(const vofa_io_t *io,uint8_t override);
uint8_t vofa_et4000_dac(void);
/* Probe and program the adapter for the selected mode (0=640x480 view 640x400,
   1=800x600, 2=1024x768).  Out-of-range modes fall back to 640. */
int vofa_et4000_init(const vofa_io_t *io,uint8_t mode);
/* Live switch: rewrite only the CRTC timing/offset for an already-programmed
   adapter (no presence probe, VRAM clear or clock check).  The kernel repaints
   the whole frame afterwards, so the display comes back at the new geometry. */
int vofa_et4000_set_mode(const vofa_io_t *io,uint8_t mode);
/* Program one RAMDAC entry (6 bits per gun).  This is the palette the card
   actually scans out; the ST Shifter palette is unrelated on this adapter. */
int vofa_et4000_set_palette(const vofa_io_t *io,uint8_t index,uint8_t r,uint8_t g,uint8_t b);
/* Visible geometry of a mode; never NULL for an index below VOFA_MODE_COUNT. */
const vofa_mode_info_t *vofa_et4000_mode(uint8_t mode);
/* Index of an exact width/height pair, or -1 when the mode is not provided. */
int vofa_et4000_mode_index(uint16_t width,uint16_t height);
/* ROM-side bring-up (BIOS, before the first console message): same guarded
   sequence with bus-error-protected accesses and an MFP tick for the clock
   check. Returns VOFA_OK when the adapter is programmed and scanning. */
int vofa_rom_init(void);
/* Diagnostic state of the ROM bring-up (read from the running system). */
extern volatile uint32_t vofa_rom_status;
#endif
