#ifndef KVIDEO_GUARD_H
#define KVIDEO_GUARD_H
#include <stdint.h>
/* Zabezpieczenie zmiany trybu graficznego - w JADRZE, nie w aplikacji.
 *
 * Po kazdej zmianie trybu jadro pamieta tryb POPRZEDNI i uzbraja licznik.
 * Aplikacja (SETTINGS) musi zmiane POTWIERDZIC; jesli w 4 sekundy tego nie
 * zrobi - bo tryb jest nieczytelny, bo aplikacja sie zawiesila, bo cokolwiek -
 * jadro SAMO wraca do poprzedniego trybu.  Dzieki temu zabezpieczenie dziala
 * nawet wtedy, gdy SETTINGS zginie.
 *
 * Stan jest tu, w naglowku, celowo (wzorzec kgfx_planar4.h): te sama logike
 * kompiluje jadro i sprawdza test hostowy, wiec nie ma dwoch roznych prawd.
 *
 * Jednostka czasu: tykniecia liczone w PRZERWANIU Timer-C (kernel_timer_ticks,
 * 50 Hz) - nie tykniecia scheduler-a ani polling licznika.  Polling gubi
 * okresy, gdy petla jadra zwalnia (zmierzone 23 Hz zamiast 50 Hz w trybie
 * 4 bpp), a wtedy "4 s" wychodzi 8-14 s i bezpieczenstwo przestaje byc
 * przewidywalne.  Wszystkie wywolania (uzbrojenie, petla jadra, WM, op 39)
 * podaja ten sam zegar: kernel_user_wall_clock().
 */
#define KVIDEO_GUARD_TICKS 200u   /* 4 s przy 50 Hz */

typedef struct {
    uint8_t  pending;        /* 1 = zmiana czeka na potwierdzenie */
    uint16_t prev_w,prev_h;  /* tryb, do ktorego wracamy */
    uint8_t  prev_bpp;
    uint32_t armed;          /* tykniecie, w ktorym uzbrojono */
} kvideo_guard_t;

static inline void kvideo_guard_init(kvideo_guard_t *g)
{g->pending=0u;g->prev_w=0u;g->prev_h=0u;g->prev_bpp=0u;g->armed=0u;}

/* Uzbroj po zastosowaniu nowego trybu.  WOLAJ Z POPRZEDNIM trybem. */
static inline void kvideo_guard_arm(kvideo_guard_t *g,uint16_t w,uint16_t h,
                                    uint8_t bpp,uint32_t now)
{
    /* Gdy zmiana JUZ czeka na potwierdzenie, NIE nadpisujemy trybu powrotu:
       inaczej lancuch tymczasowych zmian cofalby sie do poprzedniej tymczasowej
       (moze rownie nieczytelnej), a nie do ostatniej POTWIERDZONEJ. */
    if(!g->pending){g->prev_w=w;g->prev_h=h;g->prev_bpp=bpp;}
    g->armed=now;g->pending=1u;
}

/* Potwierdzenie aplikacji: zmiana zostaje na stale. */
static inline void kvideo_guard_confirm(kvideo_guard_t *g){g->pending=0u;}

/* Czy minąl czas bez potwierdzenia?  1 = trzeba wrocic do poprzedniego trybu. */
static inline int kvideo_guard_expired(kvideo_guard_t *g,uint32_t now)
{
    if(!g->pending)return 0;
    if((uint32_t)(now-g->armed)<KVIDEO_GUARD_TICKS)return 0;
    g->pending=0u;
    return 1;
}
#endif
