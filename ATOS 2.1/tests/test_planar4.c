/* Test konwersji 8 bpp chunky -> VGA 4 bpp planarnej (include/kgfx_planar4.h).
 *
 * Ten sam naglowek kompiluje sie do jadra, wiec test sprawdza DOKLADNIE ten
 * kod, ktory pojedzie na Atari.  Uklad musi byc zgodny z modelem karty w
 * Hatari (src/vofa.c) i tabelami sterownika (seq4/gdc4/attr4) - jesli ktos
 * zmieni jedna strone bez drugiej, ten test to wykryje. */
#include <stdio.h>
#include <string.h>
#include "kgfx_planar4.h"

static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)

int main(void)
{
    uint8_t src[64], dst[64], map[256];

    /* Mapa uzywana w wiekszosci przypadkow: "stare" zachowanie (indeks & 15),
       zeby dotychczasowe wzorce pozostaly czytelne. */
    /* entry testu podaja indeks jako (v<<4), a mapa ma go zamienic na v -
       czyli odpowiednik dawnego ">>4", ale TERAZ przez tabele, ktora kernel
       buduje z palety. */
    {unsigned mi;for(mi=0;mi<256u;++mi)map[mi]=(uint8_t)(mi>>4);}

    /* 1. Pojedyncza grupa 8 pikseli, szesc kolorow o roznych bitach. */
    memset(src,0,sizeof src); memset(dst,0xAA,sizeof dst);
    src[0]=(uint8_t)(0x01u<<4);   /* idx 1 -> tylko plan 0 */
    src[1]=(uint8_t)(0x02u<<4);   /* idx 2 -> plan 1 */
    src[2]=(uint8_t)(0x04u<<4);   /* idx 4 -> plan 2 */
    src[3]=(uint8_t)(0x08u<<4);   /* idx 8 -> plan 3 */
    src[4]=(uint8_t)(0x0Fu<<4);   /* idx 15 -> wszystkie plany */
    src[5]=0x00;                  /* idx 0 -> zadnego */
    CHECK(kgfx_present_planar4(dst,4u,src,8u,map,8u,1u)==0);
    /* Po JEDNEJ konwersji dst zawiera bity WSZYSTKICH 8 pikseli:
       idx 1 -> plan0 bit7; idx 2 -> plan1 bit6; idx 4 -> plan2 bit5;
       idx 8 -> plan3 bit4; idx 15 -> wszystkie plany bit3; 5..7 puste. */
    CHECK(dst[0]==0x88u);   /* plan 0: piksel 0 (bit7) + piksel 4 (bit3) */
    CHECK(dst[1]==0x48u);   /* plan 1: piksel 1 (bit6) + piksel 4 */
    CHECK(dst[2]==0x28u);   /* plan 2: piksel 2 (bit5) + piksel 4 */
    CHECK(dst[3]==0x18u);   /* plan 3: piksel 3 (bit4) + piksel 4 */
    /* Piksele 5..7 zerowe we wszystkich planach (mlodsze 3 bity). */
    CHECK((dst[0]&0x07u)==0u&&(dst[1]&0x07u)==0u&&(dst[2]&0x07u)==0u&&(dst[3]&0x07u)==0u);

    /* 2. Wyczerpujaco: kazdy z 16 indeksow daje wlasny wzorzec bitow planow. */
    {
        unsigned v;
        for(v=0;v<16u;++v){
            memset(src,0,sizeof src); memset(dst,0,sizeof dst);
            src[0]=(uint8_t)(v<<4);
            CHECK(kgfx_present_planar4(dst,4u,src,8u,map,8u,1u)==0);
            CHECK(dst[0]==((v&1u)?0x80u:0u));
            CHECK(dst[1]==((v&2u)?0x80u:0u));
            CHECK(dst[2]==((v&4u)?0x80u:0u));
            CHECK(dst[3]==((v&8u)?0x80u:0u));
        }
    }

    /* 3. Gorny polowek indeksu: 8-bitowy indeks >> 4.  0x7F -> 7. */
    memset(src,0,sizeof src); memset(dst,0,sizeof dst);
    src[0]=0x7Fu;
    CHECK(kgfx_present_planar4(dst,4u,src,8u,map,8u,1u)==0);
    CHECK(dst[0]==0x80u&&dst[1]==0x80u&&dst[2]==0x80u&&dst[3]==0x00u);

    /* 4. Szerokosc nie bedaca wielokrotnoscia 8: ogon musi byc wyzerowany,
          a wiersz nie moze wyjsc poza przeznaczony obszar. */
    {
        uint8_t small[8];
        memset(src,0xFFu,sizeof src); memset(small,0xAAu,sizeof small);
        CHECK(kgfx_present_planar4(small,8u,src,16u,map,9u,1u)==0);
        /* 9 px -> 2 grupy = 8 bajtow; piksel 8 (bit 7 drugiej grupy) ustawiony. */
        CHECK(small[4]==0x80u&&small[5]==0x80u&&small[6]==0x80u&&small[7]==0x80u);
        /* Piksele 9..15 nie istnieja, wiec ich bity musza byc zerowe. */
        CHECK((small[4]&0x7Fu)==0u&&small[5]==0x80u);
    }

    /* 5. Bledne argumenty: zaden zapis nie moze powstac. */
    memset(dst,0x5Au,sizeof dst);
    CHECK(kgfx_present_planar4(NULL,4u,src,16u,map,8u,1u)==-1);
    CHECK(kgfx_present_planar4(dst,4u,NULL,16u,map,8u,1u)==-1);
    CHECK(kgfx_present_planar4(dst,3u,src,16u,map,8u,1u)==-1);   /* dst pitch za maly */
    CHECK(kgfx_present_planar4(dst,8u,src,7u,map,8u,1u)==-1);    /* src pitch za maly */
    CHECK(kgfx_present_planar4(dst,4u,src,16u,map,0u,1u)==-1);
    for(unsigned i=0;i<sizeof dst;++i)CHECK(dst[i]==0x5Au);

    /* 6. Mapa DOWOLNA: indeks 200 ma odpowiadac kolorowi 3.  To jest wlasnie
          naprawa z audytu: wczesniej uzywano stale ">>4", przez co indeksy
          pulpitu (np. tekst i tlo) zlewaly sie w jeden kolor. */
    {unsigned mi;for(mi=0;mi<256u;++mi)map[mi]=0u;
     for(mi=0;mi<16u;++mi)map[mi]=(uint8_t)mi;}   /* systemowe bez zmian */
    map[200]=3u;
    memset(src,0,sizeof src); memset(dst,0,sizeof dst);
    src[0]=200u;
    CHECK(kgfx_present_planar4(dst,4u,src,8u,map,8u,1u)==0);
    /* kolor 3 = plany 0 i 1 -> bit 7 w bajtach 0 i 1 */
    CHECK(dst[0]==0x80u&&dst[1]==0x80u&&dst[2]==0x00u&&dst[3]==0x00u);

    printf("PASS planar4-present: %u asercji (uklad zgodny z vofa.c i sterownikiem)\n",checks);
    return 0;
}
