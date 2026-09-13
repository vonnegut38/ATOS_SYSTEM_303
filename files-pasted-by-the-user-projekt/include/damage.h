#ifndef ASF_DAMAGE_H
#define ASF_DAMAGE_H
#include <stdint.h>
/* EmuTOS-style damage tracking: the UI never repaints the whole screen, it
 * records the rectangles it touched and reports the cost.  Overlapping regions
 * merge; an overflow collapses to a single full-screen rectangle. */
#define DAMAGE_MAX 8
typedef struct {uint16_t x,y,w,h;} damage_rect_t;
void damage_reset(void);
void damage_add(int x,int y,int w,int h);
uint8_t damage_count(void);
uint32_t damage_pixels(void);
const damage_rect_t *damage_rect(uint8_t index);
#endif
