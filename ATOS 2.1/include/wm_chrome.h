#ifndef WM_CHROME_H
#define WM_CHROME_H
#include "wm_core.h"

/* Match kernel_wm's 16-pixel title, two-pixel bevel and 14-pixel sizer.
   Client-only damage must not redraw chrome or dirty the entire frame. */
static inline int wm_chrome_damaged(const wm_rect_t *frame,
                                    const wm_rect_t *scrollbar,
                                    const wm_rect_t *damage)
{
    wm_rect_t hit,sizer;
    if(!wm_rect_intersect(frame,damage,&hit))return 0;
    if(hit.x<frame->x+2||hit.y<frame->y+16||
       (int32_t)hit.x+hit.w>(int32_t)frame->x+frame->w-2||
       (int32_t)hit.y+hit.h>(int32_t)frame->y+frame->h-2)return 1;
    sizer.x=(int16_t)(frame->x+frame->w-15);
    sizer.y=(int16_t)(frame->y+frame->h-15);sizer.w=sizer.h=14;
    if(wm_rect_intersect(&sizer,damage,&hit))return 1;
    return scrollbar&&wm_rect_intersect(scrollbar,damage,&hit);
}
/* 400 ms on the native 50 Hz timer, independent of repaint frequency. */
static inline int wm_open_guard_active(uint32_t now,uint32_t opened)
{return (uint32_t)(now-opened)<20u;}
#endif
