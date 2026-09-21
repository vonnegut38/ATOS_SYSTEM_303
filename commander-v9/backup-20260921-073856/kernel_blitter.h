#ifndef ASF_KERNEL_BLITTER_H
#define ASF_KERNEL_BLITTER_H
#include <stdint.h>

void kblit_init(uint8_t present,uint8_t enabled);
int kblit_self_test(void);
int kblit_fill_planar(volatile uint16_t *words,uint16_t line_words,uint8_t planes,
                      uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t color);
uint8_t kblit_present(void);
uint8_t kblit_enabled(void);

#endif
