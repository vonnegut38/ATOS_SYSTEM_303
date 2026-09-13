#ifndef MONO_WM_H
#define MONO_WM_H
#include <stdint.h>
typedef struct {int16_t x,y,w,h;} wm_rect;
typedef struct {wm_rect rect;uint32_t pid,count;uint8_t open,paused;char text[36],staging[36];uint8_t parts;} wm_window;
typedef struct {wm_window win[2];int8_t active,full,gesture;int16_t dx,dy;uint8_t buttons;} wm_state;
void wm_init(wm_state *);
wm_rect wm_bounds(const wm_state *,int);
int wm_select(wm_state *,int);
int wm_cycle(wm_state *);
void wm_close(wm_state *,int);
int wm_hit(const wm_state *,int,int);
void wm_motion(wm_state *,int,int);
#endif
