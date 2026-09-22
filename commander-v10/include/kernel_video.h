#ifndef ASF_KERNEL_VIDEO_H
#define ASF_KERNEL_VIDEO_H
#include <stdint.h>
#include "kernel_timer.h"
/* Logical surfaces remain in ST-RAM; the output device owns scanout. */
void kvideo_reset(const volatile uint8_t *screen);
void kvideo_select(const volatile uint8_t *screen);
void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h);
void kvideo_flush(void);
int kvideo_is_vofa(void);
/* Which buffer the output device currently scans out (diagnostics). */
const void *kvideo_front(void);
void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer);
int kvideo_activate_vofa(volatile uint8_t *vram);
/* Guarded byte bus accesses. Only enabled on a baseline 68000 in v8. */
int kvideo_read8(uint32_t address,uint8_t *value);
int kvideo_write8(uint32_t address,uint32_t value);
void kvideo_shifter_base(const volatile uint8_t *base);
#endif
