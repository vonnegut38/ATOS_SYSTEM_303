#ifndef ATOS_GRAPHICS_DEVICE_H
#define ATOS_GRAPHICS_DEVICE_H
#include <stdint.h>
/* ATOS graphics device contract.
 *
 * This is the ONLY interface the compositor, the VDI, the desktop and every
 * application may use for output.  No code outside a driver may test for a
 * particular chip (ET4000 / W32i / Mach32 / Mach64 / Shifter): it asks the
 * device for its capabilities and formats instead.
 *
 * Drivers are separate loadable modules in \SYSTEM\DRV, one per card family.
 * The BIOS already detects the adapter and chipset (boot handoff reserved0 /
 * reserved1); the kernel maps that to a driver file through the manifest in
 * system/kernel_video_boot.c and loads the first candidate that probes OK.
 * Adding a new card is therefore: a new \SYSTEM\DRV\<NAME>.SYS module plus
 * one manifest entry - never a change to the desktop, VDI or compositor.
 */
#define GFX_DEVICE_MAGIC   0x47444556UL /* "GDEV" */
#define GFX_DEVICE_VERSION 1u

/* Chipset reported by the BIOS in the boot handoff (reserved1); the same
   numbering as NOVA_CHIP_* in include/system.h. */
typedef enum {
    GFX_CHIP_NONE          = 0,
    GFX_CHIP_ET4000_FAMILY = 1,   /* plain ET4000/ET4000AX or ET4000-W32i */
    GFX_CHIP_MACH32        = 2,
    GFX_CHIP_MACH64        = 3
} gfx_chip_t;

/* Pixel formats a device can scan out. */
typedef enum {
    GFX_FMT_PLANAR_1BPP = 1,
    GFX_FMT_PLANAR_2BPP = 2,
    GFX_FMT_PLANAR_4BPP = 4,
    GFX_FMT_CHUNKY_8BPP = 8
} gfx_format_t;

/* Capabilities: the renderer picks a path from these, never from a chip name. */
#define GFX_CAP_DAMAGE_ROWS   (1u<<0)  /* flush() honours per-scanline damage  */
#define GFX_CAP_HW_BLIT       (1u<<1)  /* hardware 2D blit (W32i, Mach32/64)   */
#define GFX_CAP_HW_CURSOR     (1u<<2)  /* cursor drawn by the card             */
#define GFX_CAP_PALETTE       (1u<<3)  /* set_palette() has an effect          */
#define GFX_CAP_LIVE_MODE     (1u<<4)  /* set_mode() may run while scanning    */

typedef struct {
    uint16_t width,height,pitch;       /* pitch = bytes per line */
    uint8_t  format;                   /* gfx_format_t */
    uint8_t  *pixels;
} gfx_surface_t;

typedef struct {
    uint16_t width,height;
    uint8_t  format;
    uint32_t bytes;                    /* VRAM the mode needs */
} gfx_mode_info_t;

typedef struct gfx_device {
    const char *name;                  /* e.g. "ET4000", "W32I", "MACH64" */
    uint8_t  chip;                     /* gfx_chip_t the driver serves */
    uint32_t caps;                     /* GFX_CAP_* */
    uint16_t mode_count;
    const gfx_mode_info_t *(*mode_info)(uint16_t index);
    int  (*probe)(void);               /* card present + this variant? */
    int  (*init)(void);
    void (*shutdown)(void);
    int  (*set_mode)(uint16_t w,uint16_t h,uint8_t format);
    const gfx_surface_t *(*surface)(void);
    void (*select_front)(const uint8_t *buffer);   /* scanout source */
    void (*damage)(int32_t x,int32_t y,int32_t w,int32_t h);
    void (*flush)(void);                            /* push damaged rows */
    void (*set_palette)(const uint16_t *rgb,uint16_t first,uint16_t count);
    void (*cursor)(int show,int x,int y);
    int  (*blit)(int dx,int dy,const gfx_surface_t *src,int sx,int sy,int w,int h);
} gfx_device_t;

/* Registry: drivers publish themselves; the boot picks by detected chip. */
void gfx_device_register(gfx_device_t *device);
gfx_device_t *gfx_device_active(void);
gfx_device_t *gfx_device_select_chip(uint8_t chip);
#endif
