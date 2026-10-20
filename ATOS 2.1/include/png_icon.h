#ifndef PNG_ICON_H
#define PNG_ICON_H
#include <stdint.h>
/* work: >= 65536 bytes scratch (16KB idat + 48KB inflate output). */
int png_decode_rgba(const uint8_t *data,uint32_t len,uint8_t *rgba,uint32_t *w,uint32_t *h,uint8_t *work);
int png_icon_convert(const uint8_t *rgba,uint32_t w,uint32_t h,
                     const uint16_t *palette,uint8_t npalette,
                     uint8_t *pixels,uint8_t *mask,uint32_t out_size);
#endif
