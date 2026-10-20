#ifndef WM_CLAMP_H
#define WM_CLAMP_H
#include <stdint.h>
/* Wtloczenie okna z powrotem na ekran po ZMNIEJSZENIU rozdzielczosci.
 *
 * Bylo: gdy okno stalo poza nowa krawedzia, rozmiar liczono jako
 * "screen_w - x".  Dla x=900 i screen_w=640 daje to -260, a rzutowanie na
 * uint16_t daje 65 276 - okno dostawalo OGROMNY, bledny rozmiar zamiast
 * zostac przesuniete (audyt r1).  Warunek "w<64 => w=64" tego nie lapal,
 * bo 65 276 jest wieksze od 64.
 *
 * Teraz: najpierw ograniczamy ROZMIAR do ekranu, potem PRZESUWAMY okno tak,
 * zeby jego prawa/dolna krawedz zmiescila sie na ekranie, i dopiero na koncu
 * pilnujemy minimalnego rozmiaru.  Cala arytmetyka jest na int (bez zawijania).
 *
 * Zwraca 1, gdy cokolwiek sie zmienilo (wtedy wola sie notify_geometry).
 */
typedef struct {int32_t x,y;int32_t w,h;} wm_clamp_rect_t;

static inline int wm_clamp_rect(wm_clamp_rect_t *r,int32_t screen_w,
                                int32_t screen_h,int32_t min_w,int32_t min_h)
{
    int32_t ox=r->x,oy=r->y,ow=r->w,oh=r->h;
    if(screen_w<1)screen_w=1;
    if(screen_h<1)screen_h=1;
    /* 1. Okno wieksze od ekranu: zmniejsz do ekranu. */
    if(r->w>screen_w)r->w=screen_w;
    if(r->h>screen_h)r->h=screen_h;
    /* 2. Ujemne wspolrzedne: dosun do krawedzi. */
    if(r->x<0)r->x=0;
    if(r->y<0)r->y=0;
    /* 3. Wystawanie za prawa/dolna krawedz: PRZESUN okno. */
    if(r->x+r->w>screen_w)r->x=screen_w-r->w;
    if(r->y+r->h>screen_h)r->y=screen_h-r->h;
    if(r->x<0)r->x=0;
    if(r->y<0)r->y=0;
    /* 4. Minimalny sensowny rozmiar (i nie wiekszy niz ekran). */
    if(r->w<min_w)r->w=min_w;
    if(r->h<min_h)r->h=min_h;
    if(r->w>screen_w)r->w=screen_w;
    if(r->h>screen_h)r->h=screen_h;
    if(r->x+r->w>screen_w)r->x=(screen_w-r->w>0)?(screen_w-r->w):0;
    if(r->y+r->h>screen_h)r->y=(screen_h-r->h>0)?(screen_h-r->h):0;
    return (r->x!=ox||r->y!=oy||r->w!=ow||r->h!=oh)?1:0;
}
#endif
