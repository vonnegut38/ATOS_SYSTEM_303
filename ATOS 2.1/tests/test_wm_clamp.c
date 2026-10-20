/* Test wtlocenia okna na ekran po zmniejszeniu rozdzielczosci (wm_clamp.h).
 * Ten sam naglowek kompiluje sie do WM, wiec test sprawdza DOKLADNIE te
 * obliczenia, ktore decyduja o rozmiarze i pozycji okna. */
#include <stdio.h>
#include "wm_clamp.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    wm_clamp_rect_t r;
    /* 1. PRZYPADEK Z AUDYTU: okno calkiem poza prawa krawedzia.
       Stara wersja: w = screen_w - x = 640-900 = -260 -> uint16 = 65276. */
    r=(wm_clamp_rect_t){900,100,200,150};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==1);
    CHECK(r.w==200&&r.h==150);            /* rozmiar NIE zmieniony */
    CHECK(r.x==440);                      /* PRZESUNIETE: 640-200 */
    CHECK(r.x+r.w==640);                  /* dokladnie do krawedzi */
    /* 2. To samo w pionie. */
    r=(wm_clamp_rect_t){100,700,200,150};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==1);
    CHECK(r.y==330&&r.y+r.h==480);
    /* 3. Okno WIEKSZE od ekranu: zmniejszone do ekranu (i nie wieksze). */
    r=(wm_clamp_rect_t){0,0,1200,900};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==1);
    CHECK(r.w==640&&r.h==480);
    CHECK(r.x==0&&r.y==0);
    /* 4. Ujemne wspolrzedne: dosuniete do krawedzi. */
    r=(wm_clamp_rect_t){-50,-30,200,150};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==1);
    CHECK(r.x==0&&r.y==0);
    /* 5. Okno, ktore sie MIESCI: bez zmian (i zgloszone jako bez zmian). */
    r=(wm_clamp_rect_t){10,20,300,200};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==0);
    CHECK(r.x==10&&r.y==20&&r.w==300&&r.h==200);
    /* 6. Minimalny rozmiar okna. */
    r=(wm_clamp_rect_t){600,460,10,10};
    CHECK(wm_clamp_rect(&r,640,480,64,50)==1);
    CHECK(r.w==64&&r.h==50);
    CHECK(r.x+r.w<=640&&r.y+r.h<=480);
    /* 7. WLASNOSC: dla wielu pozycji i rozmiarow wynik ZAWSZE na ekranie.
       To jest test, ktory zlapalby zawiniecie uint16 - żaden wynik nie moze
       byc ogromny ani wystawac za krawedz. */
    {int x,y,w,h;
     for(x=-200;x<=1400;x+=97)for(y=-200;y<=1200;y+=113)
      for(w=8;w<=1400;w+=131)for(h=8;h<=1200;h+=127){
        r=(wm_clamp_rect_t){x,y,w,h};
        (void)wm_clamp_rect(&r,640,480,64,50);
        CHECK(r.x>=0&&r.y>=0);
        CHECK(r.x+r.w<=640&&r.y+r.h<=480);
        CHECK(r.w>=64&&r.h>=50);
        CHECK(r.w<=640&&r.h<=480);
        CHECK(r.x<=640&&r.y<=480);
      }}
    printf("PASS wm-clamp: %u asercji (okno poza krawedzia jest PRZESUWANE, nie rozdymane)\n",checks);
    return 0;
}
