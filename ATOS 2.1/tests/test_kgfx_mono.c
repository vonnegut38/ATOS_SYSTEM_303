/* Byte-order contract for the fast mono blit (see include/kgfx_mono.h).
 *
 * The original bug reversed every four output bytes on screen (little-endian
 * packing on a big-endian CPU) and no timing measurement noticed; this test
 * checks the raw bytes, independent of the blit loop. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "kgfx_mono.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    uint8_t e[8], out[8];
    uint32_t w;
    /* ink 0xFF, paper 0x00, MSB of the source byte = leftmost pixel. */
    e[0]=0xFF;e[1]=0x00;e[2]=0x00;e[3]=0x00;e[4]=0x00;e[5]=0x00;e[6]=0x00;e[7]=0x00;
    memset(out,0xAA,sizeof out);
    kgfx_mono_pack8(e,out);
    CHECK(out[0]==0xFF&&out[1]==0x00&&out[2]==0x00&&out[3]==0x00,
          "pack8 must copy bytes straight, got %02x %02x %02x %02x", out[0],out[1],out[2],out[3]);
    w=kgfx_mono_word_be(e);
    CHECK(w==0xFF000000u, "word_be for FF,00,00,00 must be FF000000, got %08x", w);
    e[0]=0x00;e[1]=0x00;e[2]=0x00;e[3]=0xFF;
    w=kgfx_mono_word_be(e);
    CHECK(w==0x000000FFu, "word_be for 00,00,00,FF must be 000000FF, got %08x", w);
    /* A whole expansion of 0x80 (leftmost pixel lit) with ink=0xFF paper=0x00. */
    { uint8_t ex[8]; unsigned i; for(i=0;i<8u;i++)ex[i]=(i==0)?0xFFu:0x00u;
      w=kgfx_mono_word_be(ex);
      CHECK(w==0xFF000000u, "pixel 0 of 0x80 must start with FF, got %08x", w); }
    puts(failures?"kgfx_mono: FAIL":"kgfx_mono: ALL PASS");
    return failures?1:0;
}
