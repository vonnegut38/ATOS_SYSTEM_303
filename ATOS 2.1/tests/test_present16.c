/* Test konwersji 8 bpp chunky -> 16 bpp HICOLOR (5:6:5) — include/kgfx_present16.h.
 *
 * Ten sam naglowek kompiluje sie do jadra, wiec test sprawdza DOKLADNIE ten kod,
 * ktory pojedzie na Atari (wzorzec z kgfx_planar4.h / kgfx_mono.h).
 * Uklad musi byc zgodny z modelem karty w Hatari (src/vofa.c: bpp=16,
 * stride = width*2, 5:6:5 little-endian) i z tabelami sterownika
 * (system/vofa_et4000.c: gdc16[6] = 0x08). */
#include <stdio.h>
#include <string.h>
#include "kgfx_present16.h"

static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)

int main(void)
{
    uint16_t pal[256]; uint8_t src[32], dst[64];
    unsigned i;

    /* 1. Paleta 12-bit ST -> 5:6:5.  Wartosci wzorcowe VGA. */
    CHECK(kgfx_rgb12_to_565(0x000u)==0x0000u);   /* czarny */
    CHECK(kgfx_rgb12_to_565(0xFFFu)==0xFFFFu);   /* bialy */
    CHECK(kgfx_rgb12_to_565(0xF00u)==0xF800u);   /* czysta czerwien */
    CHECK(kgfx_rgb12_to_565(0x0F0u)==0x07E0u);   /* czysta zielen */
    CHECK(kgfx_rgb12_to_565(0x00Fu)==0x001Fu);   /* czysty blekit */
    /* Rozszerzenie 4 -> 5 bitow to (v<<1)|(v>>3), a nie samo v<<1:
       0x8 -> 0x11, wiec 0x8800.  To standard VGA dla 5:6:5. */
    CHECK(kgfx_rgb12_to_565(0x800u)==0x8800u);

    /* 2. Konwersja pikseli: indeks -> kolor z tabeli, LITTLE-ENDIAN. */
    for(i=0;i<256u;++i)pal[i]=(uint16_t)(0x1000u+i);
    memset(src,0,sizeof src); memset(dst,0xAAu,sizeof dst);
    src[0]=7u; src[1]=8u; src[2]=9u;
    CHECK(kgfx_present_packed16(dst,16u,src,8u,pal,3u,1u)==0);
    /* 0x1007 little-endian = 0x07,0x10 */
    CHECK(dst[0]==0x07u&&dst[1]==0x10u);
    CHECK(dst[2]==0x08u&&dst[3]==0x10u);
    CHECK(dst[4]==0x09u&&dst[5]==0x10u);
    /* Reszta wiersza (pitch 16, szerokosc 3) musi zostac nietknieta. */
    for(i=6u;i<16u;++i)CHECK(dst[i]==0xAAu);

    /* 3. Wiecej wierszy: stride zrodla i celu. */
    memset(src,0,sizeof src); memset(dst,0,sizeof dst);
    src[0]=1u; src[8]=2u;
    CHECK(kgfx_present_packed16(dst,16u,src,8u,pal,3u,2u)==0);
    CHECK(dst[0]==0x01u&&dst[1]==0x10u);         /* wiersz 0 */
    CHECK(dst[16]==0x02u&&dst[17]==0x10u);       /* wiersz 1 */

    /* 4. Bledne argumenty: nic nie piszemy. */
    memset(dst,0x5Au,sizeof dst);
    CHECK(kgfx_present_packed16(NULL,16u,src,8u,pal,3u,1u)==-1);
    CHECK(kgfx_present_packed16(dst,16u,NULL,8u,pal,3u,1u)==-1);
    CHECK(kgfx_present_packed16(dst,16u,src,8u,NULL,3u,1u)==-1);
    CHECK(kgfx_present_packed16(dst,5u,src,8u,pal,3u,1u)==-1);   /* pitch < width*2 */
    CHECK(kgfx_present_packed16(dst,16u,src,2u,pal,3u,1u)==-1);  /* src pitch za maly */
    for(i=0;i<sizeof dst;++i)CHECK(dst[i]==0x5Au);

    printf("PASS present16: %u asercji (uklad zgodny z vofa.c i sterownikiem)\n",checks);
    return 0;
}
