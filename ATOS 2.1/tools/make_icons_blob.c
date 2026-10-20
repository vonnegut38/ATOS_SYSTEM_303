/* Generator \SYSTEM\ICONS.DAT - uruchamiany na hoście przy budowaniu.
 *
 * Włącza wygenerowany nagłówek z tablicami ikon (system/icons_color.h, 1,8 MB
 * źródła / 254 KB danych) i zapisuje je jako jeden bloba big-endian, który
 * jądro wczytuje przy starcie do bufora w TT-RAM.  Dzięki temu te 254 KB nie
 * leżą w obrazie jądra, a co za tym idzie - nie zajmują ST-RAM.
 *
 * Użycie: make_icons_blob <wyjscie.dat>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "icons_color.h"
#include "icons_blob.h"

static void put32(FILE *f, uint32_t v)
{
    fputc((int)((v >> 24) & 0xffu), f); fputc((int)((v >> 16) & 0xffu), f);
    fputc((int)((v >> 8) & 0xffu), f);  fputc((int)(v & 0xffu), f);
}
static void put16(FILE *f, uint16_t v)
{
    fputc((int)((v >> 8) & 0xffu), f); fputc((int)(v & 0xffu), f);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uzycie: %s <wyjscie.dat>\n", argv[0]); return 2; }
    const uint32_t count = (uint32_t)ICON_COLOR_COUNT;
    uint32_t off = (uint32_t)sizeof(icons_blob_header_t)
                 + count * (uint32_t)sizeof(icons_blob_entry_t)
                 + (uint32_t)ICON_PAL_COLORS * 2u;
    icons_blob_entry_t *entries = (icons_blob_entry_t *)calloc(count, sizeof(*entries));
    if (!entries) return 3;
    for (uint32_t i = 0; i < count; ++i) {
        entries[i].mask_off = off;   off += (uint32_t)ICON_MASK_BYTES;
        entries[i].pixels_off = off; off += (uint32_t)ICON_PIXEL_BYTES;
    }
    FILE *f = fopen(argv[1], "wb");
    if (!f) { perror("fopen"); free(entries); return 4; }
    put32(f, ICONS_BLOB_MAGIC);
    put32(f, ICONS_BLOB_VERSION);
    put32(f, count);
    put32(f, (uint32_t)sizeof(icons_blob_header_t) + count * (uint32_t)sizeof(icons_blob_entry_t));
    put32(f, (uint32_t)sizeof(icons_blob_header_t));
    for (uint32_t i = 0; i < count; ++i) { put32(f, entries[i].mask_off); put32(f, entries[i].pixels_off); }
    for (uint32_t i = 0; i < (uint32_t)ICON_PAL_COLORS; ++i) put16(f, icon_color_palette[i]);
    for (uint32_t i = 0; i < count; ++i) {
        fwrite(icon_color_table[i].mask, 1, ICON_MASK_BYTES, f);
        fwrite(icon_color_table[i].pixels, 1, ICON_PIXEL_BYTES, f);
    }
    fclose(f);
    free(entries);
    fprintf(stderr, "ICONS.DAT: %u ikon, %u bajtow\n", (unsigned)count, (unsigned)off);
    return 0;
}
