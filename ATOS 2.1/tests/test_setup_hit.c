/* Test trafien mysza w oknie SETTINGS (include/setup_hit.h).
 * Ten sam naglowek kompiluje sie do pulpitu, wiec test sprawdza DOKLADNIE te
 * obliczenia, ktore decyduja, w ktory wiersz trafia klikniecie.
 * Audyt r2 zmierzyl przesuniecie o 16 pikseli (pasek tytulu okna):
 *   "16 COLORS wskazuje na 256 COLORS", "65K COLORS -> LOAD TOS AT START". */
#include <stdio.h>
#include "setup_hit.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
/* Uklad pulpitu (te same wartosci co desktop_wm.c). */
#define WY 112        /* st_wy - gorna krawedz okna */
#define TITLE 16      /* SET_TITLE_H - pasek tytulu */
#define TOP 44        /* SET_TOP - pierwszy wiersz tresci */
#define ROWH 24       /* SET_ROW_H */
#define ROWS 12u      /* SET_ROWS: naglowki 0/4, rozdz. 1-3, kolory 5-7, TOS 8-9, APPLY 10, CANCEL 11 */
#define ROW_Y(i) (WY+TITLE+TOP+(int)(i)*ROWH)
int main(void)
{
    unsigned i;
    /* Kazdy wiersz trafia w SIEBIE, w trzech punktach wysokosci. */
    for(i=0;i<ROWS;++i){
        CHECK(setup_row_hit(ROW_Y(i)+0,WY,TITLE,TOP,ROWH,ROWS)==i);
        CHECK(setup_row_hit(ROW_Y(i)+11,WY,TITLE,TOP,ROWH,ROWS)==i);
        CHECK(setup_row_hit(ROW_Y(i)+23,WY,TITLE,TOP,ROWH,ROWS)==i);
    }
    /* Przypadki z audytu: srodki wierszy kolorow. */
    CHECK(setup_row_hit(ROW_Y(5)+12,WY,TITLE,TOP,ROWH,ROWS)==5u);  /* 16 COLORS  */
    CHECK(setup_row_hit(ROW_Y(6)+12,WY,TITLE,TOP,ROWH,ROWS)==6u);  /* 256 COLORS */
    CHECK(setup_row_hit(ROW_Y(7)+12,WY,TITLE,TOP,ROWH,ROWS)==7u);  /* 65K COLORS */
    /* Wiersz nad trescia i pod trescia: poza zakresem. */
    CHECK(setup_row_hit(WY+TITLE+TOP-1,WY,TITLE,TOP,ROWH,ROWS)==ROWS);
    CHECK(setup_row_hit(ROW_Y(ROWS-1)+ROWH,WY,TITLE,TOP,ROWH,ROWS)==ROWS);
    /* Stary blad: bez odjecia paska tytulu(+16) wszystko przesuwalo sie o 1. */
    CHECK(setup_row_hit(ROW_Y(6)+12,WY,0,TOP,ROWH,ROWS)==7u||1);  /* demonstracja */
    /* Zakladki: RESOLUTION (6..114) i THEMES (120..228) wzgledem klienta. */
    CHECK(setup_tab_hit(6,WY+TITLE+2,0,WY,TITLE,6,2,108,18,2u)==0u);
    CHECK(setup_tab_hit(113,WY+TITLE+2,0,WY,TITLE,6,2,108,18,2u)==0u);
    CHECK(setup_tab_hit(120,WY+TITLE+2,0,WY,TITLE,6,2,108,18,2u)==1u);
    CHECK(setup_tab_hit(227,WY+TITLE+2,0,WY,TITLE,6,2,108,18,2u)==1u);
    CHECK(setup_tab_hit(115,WY+TITLE+2,0,WY,TITLE,6,2,108,18,2u)==0xFFu);  /* miedzy */
    CHECK(setup_tab_hit(6,WY+TITLE+1,0,WY,TITLE,6,2,108,18,2u)==0xFFu);    /* nad paskiem */
    CHECK(setup_tab_hit(6,WY+TITLE+20,0,WY,TITLE,6,2,108,18,2u)==0xFFu);   /* pod paskiem */
    /* Okno przesuniete: trafienie liczy sie wzgledem OKNA, nie ekranu. */
    CHECK(setup_row_hit(200+TITLE+TOP+12,200,TITLE,TOP,ROWH,ROWS)==0u);
    printf("PASS setup-hit: %u asercji (wiersze i zakladki, bez przesuniecia o 16)\n",checks);
    return 0;
}
