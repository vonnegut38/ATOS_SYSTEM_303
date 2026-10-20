/* Test zabezpieczenia zmiany trybu (include/kvideo_guard.h).
 * Ta sama logika chodzi w jądrze, więc test sprawdza DOKŁADNIE to, co decyduje
 * o powrocie do poprzedniego trybu, gdy aplikacja nie potwierdzi w 4 s. */
#include <stdio.h>
#include "kvideo_guard.h"
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    kvideo_guard_t g;
    /* 1. Bez uzbrojenia nic sie nie dzieje - nawet po bardzo dlugim czasie. */
    kvideo_guard_init(&g);
    CHECK(kvideo_guard_expired(&g,100000u)==0);
    /* 2. Uzbrojone i BEZ potwierdzenia: przed 200 tyknieciami nie wracamy... */
    kvideo_guard_init(&g);
    kvideo_guard_arm(&g,800u,600u,8u,1000u);
    CHECK(g.pending==1u);
    CHECK(kvideo_guard_expired(&g,1000u+199u)==0);   /* 199 tykniec: jeszcze nie */
    CHECK(g.pending==1u);
    /* ...a w 200. tyknieciu (4 s przy 50 Hz) wracamy DOKLADNIE RAZ. */
    CHECK(kvideo_guard_expired(&g,1000u+200u)==1);
    CHECK(g.pending==0u);
    CHECK(kvideo_guard_expired(&g,1000u+9999u)==0);  /* drugi raz nie wraca */
    /* 3. Zapamietany tryb to tryb POPRZEDNI (do niego wracamy). */
    CHECK(g.prev_w==800u&&g.prev_h==600u&&g.prev_bpp==8u);
    /* 4. Potwierdzenie: zmiana zostaje na stale, choćby minęło duzo czasu. */
    kvideo_guard_init(&g);
    kvideo_guard_arm(&g,640u,480u,4u,5000u);
    kvideo_guard_confirm(&g);
    CHECK(g.pending==0u);
    CHECK(kvideo_guard_expired(&g,5000u+100000u)==0);
    /* 5. Ponowne uzbrojenie zastepuje poprzedni stan (nowy tryb, nowy czas). */
    kvideo_guard_init(&g);
    kvideo_guard_arm(&g,640u,400u,1u,100u);
    kvideo_guard_arm(&g,800u,600u,16u,300u);
    CHECK(g.prev_w==640u&&g.prev_h==400u&&g.prev_bpp==1u);  /* z PIERWSZEJ zmiany */
    CHECK(kvideo_guard_expired(&g,300u+199u)==0);
    CHECK(kvideo_guard_expired(&g,300u+200u)==1);
    /* 6. Przekrecenie licznika czasu (uint32) nie psuje zabezpieczenia. */
    kvideo_guard_init(&g);
    kvideo_guard_arm(&g,800u,600u,8u,0xFFFFFF00u);
    CHECK(kvideo_guard_expired(&g,0xFFFFFF00u+199u)==0);
    CHECK(kvideo_guard_expired(&g,0xFFFFFF00u+200u)==1);
    printf("PASS kvideo-guard: %u asercji (4 s bez potwierdzenia => powrot)\n",checks);
    return 0;
}
