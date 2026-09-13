#ifndef MONO_WM_H
#define MONO_WM_H
#include <stdint.h>

/* Rdzen menedzera okien GUI: N okien, kolejnosc Z, fokus.
 * Model jest czysty (bez zaleznosci od jadra), wiec testujemy go na hoscie. */

#define WM_MAX 8
#define WM_TITLE_MAX 32

typedef struct {int16_t x,y,w,h;} wm_rect;

typedef struct {
    wm_rect rect;
    uint32_t pid,count;
    uint8_t open,paused,kind;
    char title[WM_TITLE_MAX];
    char text[36],staging[36];
    uint8_t parts;
} wm_window;

typedef struct {
    wm_window win[WM_MAX];
    int8_t order[WM_MAX];   /* sloty od spodu do wierzchu; tylko uzywane */
    int8_t count;           /* liczba slotow w kolejnosci Z */
    int8_t active;          /* slot z fokusem albo -1 */
    int8_t full;            /* slot na pelnym ekranie albo -1 */
    int8_t gesture;
    int16_t dx,dy;
    uint8_t buttons;
} wm_state;

void wm_init(wm_state *);
wm_rect wm_bounds(const wm_state *,int);
int wm_is_open(const wm_state *,int);
int wm_open(wm_state *,uint32_t pid,const char *title);
int wm_select(wm_state *,int);
int wm_cycle(wm_state *);
void wm_close(wm_state *,int);
int wm_hit(const wm_state *,int,int);
void wm_motion(wm_state *,int,int);
int wm_top(const wm_state *);
int wm_next_open(const wm_state *,int);

#endif
