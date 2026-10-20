#ifndef ASF_KERNEL_BLITTER_H
#define ASF_KERNEL_BLITTER_H
#include <stdint.h>

void kblit_init(uint8_t present,uint8_t enabled);
int kblit_self_test(void);
int kblit_fill_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t color);
/* Source-only copy of a planar rectangle with arbitrary bit alignment and
   overlap.  Returns 0 (caller falls back to the CPU) when the blitter was not
   detected, is disabled, or the copy self-test has not passed. */
/* Copy a planar rectangle from one bitmap to another (window blit).  Returns 0
   when no self-tested blitter is available, so the caller keeps the CPU path. */
int kblit_copy_planar_to(volatile uint16_t *src,uint16_t src_lw,
                         volatile uint16_t *dst,uint16_t dst_lw,uint8_t planes,
                         uint16_t source_x,uint16_t source_y,uint16_t width,uint16_t height,
                         uint16_t destination_x,uint16_t destination_y);
int kblit_copy_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t source_x,uint16_t source_y,uint16_t width,uint16_t height,
                      uint16_t destination_x,uint16_t destination_y);
uint8_t kblit_present(void);
uint8_t kblit_enabled(void);
/* Non-zero when the boot self-test proved the hardware copy correct. */
uint8_t kblit_copy_ready(void);
/* 1-based index of the first failing copy self-test case (0 when none). */
uint8_t kblit_copy_fail(void);

#endif
