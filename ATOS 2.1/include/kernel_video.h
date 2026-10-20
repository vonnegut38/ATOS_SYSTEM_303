#ifndef ASF_KERNEL_VIDEO_H
#define ASF_KERNEL_VIDEO_H
#include <stdint.h>
#include "kernel_timer.h"
/* Logical surfaces remain in ST-RAM; the output device owns scanout. */
void kvideo_reset(const volatile uint8_t *screen);
void kvideo_select(const volatile uint8_t *screen);
/* Adopt a mode geometry (pitch = width/8 bytes, one byte per 8 mono pixels).
   Safe to call before or after the adapter is activated. */
void kvideo_set_mode(uint16_t width,uint16_t height);
/* Same, with an explicit depth: 1 = planar mono, 8 = chunky (256 colours). */
void kvideo_set_mode_format(uint16_t width,uint16_t height,uint8_t bpp);
uint16_t kvideo_mode_width(void);
/* Bits per pixel of the active video mode (1 = planar, 8 = chunky). */
uint8_t kvideo_mode_bpp(void);
uint16_t kvideo_mode_height(void);
void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h);
void kvideo_flush(void);
/* Paleta 256 -> 5:6:5 dla trybu 16 bpp (65 536 kolorow).  W tym trybie DAC
   karty nie jest uzywany, wiec warstwa wideo tlumaczy indeksy na kolory. */
void kvideo_set_palette565(uint8_t index,uint16_t color12);
/* Zabezpieczenie zmiany trybu (include/kvideo_guard.h): uzbrojenie po zmianie,
   potwierdzenie przez aplikacje i powrot po 4 s bez potwierdzenia. */
void kvideo_guard_watch(uint16_t w,uint16_t h,uint8_t bpp,uint32_t now,uint32_t owner);
int kvideo_guard_active(void);
int kvideo_guard_control(uint32_t owner,uint32_t op,uint32_t now);
int  kvideo_guard_rollback(uint32_t now);
int kvideo_is_vofa(void);
/* Which buffer the output device currently scans out (diagnostics). */
const void *kvideo_front(void);
/* Card framebuffer the adapter scans out, or NULL when the Shifter is active. */
const void *kvideo_scanout(void);
void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer);
/* Live mode switch through the resident VOFA.SYS service: 0 = ok, <0 = refused.
   The caller reconfigures the compositor afterwards. */
int kvideo_program_mode(uint16_t width,uint16_t height);
/* Mode switch with an explicit pixel depth (8 = 800x600x256 chain-4). */
int kvideo_program_mode_format(uint16_t width,uint16_t height,uint8_t bpp);
/* Publish the VOFA.SYS service without reprogramming the card (ROM boot). */
int kvideo_module_attach(const kernel_timer_service_t *timer);
/* Adapter/chipset reported by the BIOS (boot handoff reserved0/reserved1); the
   attach picks the matching driver from \SYSTEM\DRV. */
void kvideo_set_detected(uint8_t adapter_field,uint8_t chip);
int kvideo_mode_supported(uint16_t width,uint16_t height);
/* Program one system-palette entry (ST $0RGB layout, 4 bits per gun).  On the
   card this reaches the RAMDAC; on the Shifter the hardware palette register. */
int kvideo_set_palette(uint8_t index,uint16_t color);
int kvideo_set_palette_rgb(uint8_t index,uint8_t r,uint8_t g,uint8_t b);
/* Direct scanout access for the exclusive game mode: the card aperture and its
   pitch while an 8 bpp adapter mode is active, else NULL. */
volatile uint8_t *kvideo_card8(uint16_t *pitch);
/* While gate() returns non-zero, kvideo_flush() leaves the card alone (the
   owner writes the scanout itself); when the gate opens again the next flush
   rewrites the whole frame, because the shadow no longer matches the card. */
void kvideo_set_flush_gate(int (*gate)(void));
int kvideo_activate_vofa(volatile uint8_t *vram);
/* Give the screen back to the Shifter (a failed adapter initialisation). */
void kvideo_deactivate_vofa(void);
/* Guarded byte bus accesses. Only enabled on a baseline 68000 in v8. */
int kvideo_read8(uint32_t address,uint8_t *value);
int kvideo_write8(uint32_t address,uint32_t value);
void kvideo_shifter_base(const volatile uint8_t *base);
#endif
