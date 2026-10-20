#ifndef ASF_ST_RASTER_H
#define ASF_ST_RASTER_H
#include <stdint.h>

/* Standard Atari ST screen layout — the single contract both the TOS screen
   converter and any program drawing into Logbase must agree on:
     * bitplanes are INTERLEAVED at 16-bit WORD granularity: one 16-pixel group
       holds `planes` big-endian words, plane 0 first;
     * the leftmost pixel of a group is bit 15, the rightmost is bit 0;
     * stride is bytes per row = (width/8) * planes;
     * the whole screen is therefore stride * height bytes (32000 for every
       classic ST mode: 320x200x4, 640x200x2, 640x400x1).

   The getter is kept as a pure inline so a host test (tests/test_st_raster.c)
   can exercise it under AddressSanitizer: that is what proved the previous
   "planes as separate blocks" model read 96000 bytes into a 64 KB buffer. */

/* Decode the pixel at (x, y) into a colour index (0..2^planes-1).  Never
   reads beyond stride*height when x < width and y < height. */
static inline uint8_t st_raster_pixel(const uint8_t *screen,uint16_t stride,
                                      uint8_t planes,uint16_t x,uint16_t y)
{
    const uint8_t *wp=screen+(uint32_t)y*(uint32_t)stride+
                      (uint32_t)(x>>4u)*(uint32_t)(2u*planes);
    /* MSB-first within the word: pixel x=0 is 0x8000, x=15 is 0x0001.  The old
       15-(x&15) form produced 0x0001 at x=0, i.e. it mirrored the image inside
       every 16-pixel group; a setter/getter round-trip could not see it because
       both were wrong the same way. */
    uint16_t mask=(uint16_t)(0x8000u>>(x&15u));
    uint8_t value=0u,plane;
    for(plane=0u;plane<planes;++plane){
        uint16_t word=(uint16_t)(((uint16_t)wp[2u*plane]<<8)|wp[2u*plane+1u]);
        if(word&mask)value|=(uint8_t)(1u<<plane);
    }
    return value;
}

/* Set the pixel at (x, y) to `colour` (mirror of the getter, used to build a
   known pattern in tests). */
static inline void st_raster_set_pixel(uint8_t *screen,uint16_t stride,uint8_t planes,
                                       uint16_t x,uint16_t y,uint8_t colour)
{
    uint8_t *wp=screen+(uint32_t)y*(uint32_t)stride+
                (uint32_t)(x>>4u)*(uint32_t)(2u*planes);
    uint16_t mask=(uint16_t)(0x8000u>>(x&15u));   /* same MSB-first mask */

    uint8_t plane;
    for(plane=0u;plane<planes;++plane){
        uint16_t word=(uint16_t)(((uint16_t)wp[2u*plane]<<8)|wp[2u*plane+1u]);
        if(colour&(uint8_t)(1u<<plane))word=(uint16_t)(word|mask);
        else word=(uint16_t)(word&(uint16_t)~mask);
        wp[2u*plane]=(uint8_t)(word>>8);
        wp[2u*plane+1u]=(uint8_t)word;
    }
}

#endif
