#ifndef ASF_KERNEL_VIDEO_SERVICE_H
#define ASF_KERNEL_VIDEO_SERVICE_H
#include <stdint.h>
#include "vofa_et4000.h"
/* Optional video adapter service, published by VOFA.SYS when loaded. */
#define KERNEL_VIDEO_MAGIC 0x5649444fUL /* "VIDO" */
#define KERNEL_VIDEO_VERSION 4u
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    /* Probe + program the card for one mode (VOFA_MODE_*); <0 = failure. */
    int (*init)(const vofa_io_t *io,uint8_t mode);
    /* Fast live switch: CRTC only, no probe/clear/clock check. */
    int (*set_mode)(const vofa_io_t *io,uint8_t mode);
    /* Geometry lookup, so the kernel never hardcodes the mode list. */
    const vofa_mode_info_t *(*mode_info)(uint8_t mode);
    int (*mode_index)(uint16_t width,uint16_t height);
    /* v3: program one RAMDAC entry (6 bits per gun) - the card's own palette. */
    int (*set_palette)(const vofa_io_t *io,uint8_t index,uint8_t r,uint8_t g,uint8_t b);
    /* v4: RAMDAC family the driver detected/programmed (VOFA_DAC_*), plus a
       printable name for the boot log. */
    uint8_t (*dac_type)(void);
    const char *(*dac_name)(uint8_t);
} kernel_video_service_t;
#endif
