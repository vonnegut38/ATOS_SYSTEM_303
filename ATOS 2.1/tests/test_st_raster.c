/* Regression test for the standard ST raster layout (include/st_raster.h).
 *
 * The previous converter treated the four bitplanes of a 320x200x4 screen as
 * four separate 8000-byte blocks and read plane 3 from offset 96000 of a
 * 65536-byte buffer - an AddressSanitizer heap-buffer-overflow.  This test
 * locks in the interleaved-word contract:
 *   - one 16-pixel group = `planes` big-endian words, plane 0 first;
 *   - MSB of a word is the group's leftmost pixel;
 *   - stride = (width/8)*planes bytes;
 *   - total = stride*height = 32000 bytes for every classic ST mode.
 *
 * Run under -fsanitize=address,undefined: the full-screen decode below reads
 * every byte a valid screen can touch, so any out-of-bounds access fails.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "st_raster.h"

static int failures;

#define CHECK(cond, ...) do { \
    if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                   printf(__VA_ARGS__); printf("\n"); } \
} while (0)

/* Raw-byte contract, INDEPENDENT of the accessors.  The earlier version of this
   test only round-tripped setter->getter, so both could be wrong the same way:
   they used 15-(x&15) and mirrored every 16-pixel group (pixel 0 became 0x0001
   instead of 0x8000).  These vectors are written and read as plain bytes. */
static void raw_word_contract(void)
{
    uint8_t screen[8];
    uint16_t word;

    /* Mono, first group: MSB is the leftmost pixel. */
    memset(screen, 0, sizeof screen);
    st_raster_set_pixel(screen, 2, 1, 0, 0, 1);
    CHECK(screen[0] == 0x80u && screen[1] == 0x00u,
          "raw: pixel(0) must be 80 00, got %02x %02x", screen[0], screen[1]);

    memset(screen, 0, sizeof screen);
    st_raster_set_pixel(screen, 2, 1, 15, 0, 1);
    CHECK(screen[0] == 0x00u && screen[1] == 0x01u,
          "raw: pixel(15) must be 00 01, got %02x %02x", screen[0], screen[1]);

    memset(screen, 0, sizeof screen);
    st_raster_set_pixel(screen, 2, 1, 0, 0, 1);
    st_raster_set_pixel(screen, 2, 1, 15, 0, 1);
    CHECK(screen[0] == 0x80u && screen[1] == 0x01u,
          "raw: pixels 0+15 must be 80 01, got %02x %02x", screen[0], screen[1]);

    /* The 16-pixel boundary moves to the NEXT word, not the next byte. */
    memset(screen, 0, sizeof screen);
    st_raster_set_pixel(screen, 4, 1, 16, 0, 1);
    CHECK(screen[0] == 0x00u && screen[1] == 0x00u &&
          screen[2] == 0x80u && screen[3] == 0x00u,
          "raw: pixel(16) must start word 2 with 80 00, got %02x %02x %02x %02x",
          screen[0], screen[1], screen[2], screen[3]);

    /* Getter against a hand-written word, never produced by the setter. */
    memset(screen, 0, sizeof screen);
    screen[0] = 0x80u; screen[1] = 0x01u;
    CHECK(st_raster_pixel(screen, 2, 1, 0, 0) == 1u, "raw: read pixel(0) of 80 01");
    CHECK(st_raster_pixel(screen, 2, 1, 15, 0) == 1u, "raw: read pixel(15) of 80 01");
    CHECK(st_raster_pixel(screen, 2, 1, 1, 0) == 0u, "raw: pixel(1) of 80 01 is 0");
    CHECK(st_raster_pixel(screen, 2, 1, 14, 0) == 0u, "raw: pixel(14) of 80 01 is 0");

    /* Four planes: colour 5 lights plane 0 and plane 2 in the same group. */
    memset(screen, 0, sizeof screen);
    st_raster_set_pixel(screen, 8, 4, 3, 0, 5);
    word = (uint16_t)((screen[0] << 8) | screen[1]);
    CHECK(word == 0x1000u, "raw: plane0 for pixel(3) must be 10 00, got %04x", word);
    word = (uint16_t)((screen[4] << 8) | screen[5]);
    CHECK(word == 0x1000u, "raw: plane2 for pixel(3) must be 10 00, got %04x", word);
    CHECK(st_raster_pixel(screen, 8, 4, 3, 0) == 5u, "raw: 4-plane colour round-trip");
}

/* Fill the whole screen and decode every pixel; the colour must round-trip and
   the decode must never leave the stride*height region. */
static void full_roundtrip(uint16_t width, uint16_t height, uint8_t planes,
                           uint16_t stride, const char *label)
{
    uint32_t bytes = (uint32_t)stride * (uint32_t)height;
    uint8_t *screen = (uint8_t *)malloc(bytes + 64);
    uint32_t x, y;
    uint8_t max_colour = (uint8_t)((1u << planes) - 1u);
    memset(screen, 0, bytes + 64);

    /* A sparse pattern first: distinct pixels across group boundaries. */
    { uint8_t third = (planes >= 2u) ? 3u : 1u;
      st_raster_set_pixel(screen, stride, planes, 0, 0, max_colour);
      st_raster_set_pixel(screen, stride, planes, 15, 0, 1u);
      st_raster_set_pixel(screen, stride, planes, 16, 1, third);
      st_raster_set_pixel(screen, stride, planes, (uint16_t)(width - 1),
                          (uint16_t)(height - 1), max_colour);
      CHECK(st_raster_pixel(screen, stride, planes, 0, 0) == max_colour,
            "%s: pixel(0,0) should be 0x%x", label, max_colour);
      CHECK(st_raster_pixel(screen, stride, planes, 15, 0) == 1u,
            "%s: pixel(15,0) should be 1", label);
      CHECK(st_raster_pixel(screen, stride, planes, 16, 1) == third,
            "%s: pixel(16,1) should be 0x%x", label, third); }
    CHECK(st_raster_pixel(screen, stride, planes, (uint16_t)(width - 1),
                          (uint16_t)(height - 1)) == max_colour,
          "%s: last pixel should be 0x%x", label, max_colour);
    CHECK(st_raster_pixel(screen, stride, planes, 1, 2) == 0u,
          "%s: untouched pixel(1,2) should be 0", label);

    /* Fill everything and decode every pixel (the ASan bounds proof). */
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            st_raster_set_pixel(screen, stride, planes, (uint16_t)x,
                                (uint16_t)y, (uint8_t)((x + y) & max_colour));
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            uint8_t expect = (uint8_t)((x + y) & max_colour);
            if (st_raster_pixel(screen, stride, planes, (uint16_t)x,
                                (uint16_t)y) != expect) {
                ++failures;
                printf("FAIL %s: pixel(%u,%u) mismatch\n", label, x, y);
                free(screen);
                return;
            }
        }
    free(screen);
    printf("%s: OK (stride %u, %u bytes)\n", label, stride, bytes);
}

int main(void)
{
    /* 320x200, 16 colours: 4 planes, stride (320/8)*4 = 160. */
    raw_word_contract();
    full_roundtrip(320, 200, 4, 160, "320x200x4");
    /* 640x200, 4 colours: 2 planes, stride (640/8)*2 = 160. */
    full_roundtrip(640, 200, 2, 160, "640x200x2");
    /* 640x400, mono: 1 plane, stride (640/8)*1 = 80. */
    full_roundtrip(640, 400, 1, 80, "640x400x1");

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("st_raster: ALL PASS\n");
    return 0;
}
