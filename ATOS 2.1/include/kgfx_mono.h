/* Mono blit helpers shared by the kernel and the host tests.
 *
 * The fast mono->8bpp path expands one source byte into eight output bytes and
 * writes them as two 32-bit words.  The 68000 is BIG-endian, so the first byte
 * of a group belongs in the HIGH byte of the word.  Getting that wrong reversed
 * every four bytes on screen and shredded the glyphs once already, and no timing
 * measurement could see it - hence this header plus tests/test_kgfx_mono.c. */
#ifndef KGFX_MONO_H
#define KGFX_MONO_H

#include <stdint.h>

/* Pack the eight expanded bytes e[0..7] into two big-endian 32-bit words. */
static inline void kgfx_mono_pack8(const uint8_t *e, uint8_t *out)
{
    out[0]=e[0];out[1]=e[1];out[2]=e[2];out[3]=e[3];
    out[4]=e[4];out[5]=e[5];out[6]=e[6];out[7]=e[7];
}

/* Same thing as two explicit 32-bit stores, used by the blit's aligned path. */
static inline uint32_t kgfx_mono_word_be(const uint8_t *e)
{
    return ((uint32_t)e[0]<<24)|((uint32_t)e[1]<<16)|((uint32_t)e[2]<<8)|(uint32_t)e[3];
}

#endif
