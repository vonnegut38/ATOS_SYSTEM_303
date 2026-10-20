#ifndef FONT_H
#define FONT_H

#include <stdint.h>

const uint8_t *font_glyph(char c, uint8_t tall);
/* Pointer to the 256-glyph Atari 8x8 system font image (the table Line-A's
   V_FNT_AD must point at). */
const uint8_t *font_system_8x8(void);

#endif
