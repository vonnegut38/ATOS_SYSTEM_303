#ifndef KGX_PLANAR4_H
#define KGX_PLANAR4_H
#include <stdint.h>
/* Konwersja 8 bpp CHUNKY -> VGA 4 bpp PLANARNY (tryb VOFA_MODE_640_4).
 *
 * Uklad jest WSPOLNY z trzema miejscami i musi pozostac zgodny:
 *   - modelem karty w Hatari: atari-030-port/hatari-src/src/vofa.c,
 *   - tabelami sterownika:   system/vofa_et4000.c (seq4/gdc4/attr4),
 *   - tym konwerterem.
 * Grupa 8 pikseli to 4 bajty, po jednym na plan, bit 7-(x&7):
 *   adres = base + (x/8)*4 + p,  piksel = b0 | b1<<1 | b2<<2 | b3<<3.
 *
 * Mapowanie koloru: TABELA 256 -> 16 (argument map16), a NIE stale ">>4".
 * Powod: pulpit uzywa indeksow z calego zakresu 0..255, wiec ">>4" zlewalo
 * np. tekst i tlo w jeden kolor i ekran stawal sie nieczytelny.  Tablice
 * buduje warstwa wideo tak, ze indeksy 0..15 zostaja BEZ ZMIAN (dzieki temu
 * kolory systemowe, tekst i tlo sa dokladne), a wyzsze trafiaja na najblizszy
 * z 16 wpisow palety karty.
 *
 * W naglowku celowo (jak vofa_et4000_vram_fill): dzieki temu ta sama funkcja
 * jest kompilowana do jadra i do testu hostowego, wiec test sprawdza DOKLADNIE
 * ten kod, ktory jedzie na Atari.
 *
 * Zwraca 0 przy powodzeniu, -1 przy blednych argumentach (wtedy nic nie pisze).
 */
static inline int kgfx_present_planar4(uint8_t *dst,uint16_t dst_pitch,
                                       const uint8_t *src,uint16_t src_pitch,
                                       const uint8_t *map16,
                                       uint16_t width,uint16_t height)
{
    uint16_t y;
    if(!dst||!src||!map16||!width||!height)
        return -1;
    if((uint32_t)dst_pitch<(((uint32_t)width+7u)/8u)*4u)
        return -1;
    if(src_pitch<width)
        return -1;
    for(y=0;y<height;++y){
        const uint8_t *srow=src+(uint32_t)y*src_pitch;
        uint8_t *drow=dst+(uint32_t)y*dst_pitch;
        uint16_t x;
        for(x=0;x<width;x+=8u){
            uint8_t b0=0,b1=0,b2=0,b3=0,k;
            for(k=0;k<8u;++k){
                uint16_t px=(uint16_t)(x+k);
                uint8_t idx=0;
                uint8_t bit=(uint8_t)(0x80u>>k);
                if(px<width)
                    idx=map16[srow[px]];
                if(idx&1u)
                    b0|=bit;
                if(idx&2u)
                    b1|=bit;
                if(idx&4u)
                    b2|=bit;
                if(idx&8u)
                    b3|=bit;
            }
            drow[(x>>3)*4u+0u]=b0;
            drow[(x>>3)*4u+1u]=b1;
            drow[(x>>3)*4u+2u]=b2;
            drow[(x>>3)*4u+3u]=b3;
        }
    }
    return 0;
}
#endif
