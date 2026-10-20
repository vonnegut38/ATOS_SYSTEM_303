#ifndef ASF_EMUTOS_VDI_MONO_H
#define ASF_EMUTOS_VDI_MONO_H
#include <stdint.h>

void emutos_vdi_mono_fill(volatile uint16_t *words,uint16_t line_words,
                          uint16_t x,uint16_t y,uint16_t width,uint16_t height,
                          uint8_t color);
void emutos_vdi_mono_copy_words(volatile uint16_t *words,uint16_t line_words,
                                uint16_t source_x,uint16_t source_y,
                                uint16_t width,uint16_t height,
                                uint16_t destination_x,uint16_t destination_y);
void emutos_vdi_mono_copy_shifted(volatile uint16_t *words,uint16_t line_words,
                                  uint16_t source_x,uint16_t source_y,
                                  uint16_t width,uint16_t height,
                                  uint16_t destination_x,uint16_t destination_y);

void emutos_vdi_planar_fill(volatile uint16_t *,uint16_t,uint8_t,
    uint16_t,uint16_t,uint16_t,uint16_t,uint8_t,uint8_t,const uint16_t *,uint8_t);
void emutos_vdi_vertical(volatile uint16_t *,uint16_t,uint8_t,
    uint16_t,uint16_t,uint16_t,uint8_t);
void mono_glyph(volatile uint16_t *,uint16_t,uint16_t,uint16_t,const uint8_t [16],uint8_t,uint8_t,int);
/* Same word-mask renderer for the 8-pixel-tall font (the TOS VDI uses it:
   SysInfo draws its text with vst_height 8). */
void mono_glyph_h(volatile uint16_t *,uint16_t,uint16_t,uint16_t,const uint8_t [16],uint16_t,uint8_t,uint8_t,int);
#endif
