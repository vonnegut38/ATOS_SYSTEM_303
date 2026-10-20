#ifndef KGX_PRESENT16_H
#define KGX_PRESENT16_H
#include <stdint.h>
/* Konwersja 8 bpp CHUNKY -> 16 bpp HICOLOR (5:6:5, 2 bajty na piksel,
   little-endian) dla trybu VOFA_MODE_800_16 (65 536 kolorow).
 *
 * Uklad jest WSPOLNY z modelem karty w Hatari (src/vofa.c: bpp=16, stride =
 * width*2, odczyt 5:6:5 little-endian) i z tabelami sterownika
 * (system/vofa_et4000.c: gdc16[6] = 0x08).
 *
 * UWAGA - to NIE jest powtorzenie bitow jak przy 4 bpp.  W trybie 16 bpp DAC
 * karty nie jest uzywany: kolor jest w pikselu.  Dlatego indeks 8-bitowy jest
 * tlumaczony przez TABELE palety 256 -> 5:6:5, ktora kernel buduje z palety
 * systemowej (kgfx_set_palette).  Bez tej tabeli tryb nie mialby jak pokazac
 * kolorow pulpitu.
 *
 * Zwraca 0 przy powodzeniu, -1 przy blednych argumentach (nic nie pisze).
 */
static inline int kgfx_present_packed16(uint8_t *dst,uint16_t dst_pitch,
                                        const uint8_t *src,uint16_t src_pitch,
                                        const uint16_t *pal565,
                                        uint16_t width,uint16_t height)
{
    uint16_t y;
    if(!dst||!src||!pal565||!width||!height)
        return -1;
    if((uint32_t)dst_pitch<(uint32_t)width*2u)
        return -1;
    if(src_pitch<width)
        return -1;
    for(y=0;y<height;++y){
        const uint8_t *srow=src+(uint32_t)y*src_pitch;
        uint8_t *drow=dst+(uint32_t)y*dst_pitch;
        uint16_t x;
        for(x=0;x<width;++x){
            uint16_t c=pal565[srow[x]];
            drow[x*2u]=(uint8_t)(c&0xffu);
            drow[x*2u+1u]=(uint8_t)(c>>8);
        }
    }
    return 0;
}
/* Paleta systemowa (12-bit ST: 4 bity na dzialke) -> 5:6:5. */
static inline uint16_t kgfx_rgb12_to_565(uint16_t color)
{
    uint16_t r4=(uint16_t)((color>>8)&0x0fu),g4=(uint16_t)((color>>4)&0x0fu),
             b4=(uint16_t)(color&0x0fu);
    uint16_t r5=(uint16_t)((r4<<1)|(r4>>3)),g6=(uint16_t)((g4<<2)|(g4>>2)),
             b5=(uint16_t)((b4<<1)|(b4>>3));
    return (uint16_t)((r5<<11)|(g6<<5)|b5);
}
#endif
