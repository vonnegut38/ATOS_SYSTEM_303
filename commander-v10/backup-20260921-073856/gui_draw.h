#ifndef GUI_DRAW_H
#define GUI_DRAW_H
#include <stdint.h>
#define GUI_DRAW_MAX 80u
/* Inline text: the privileged renderer never follows nested user pointers. */
typedef struct {
    uint16_t kind;
    int16_t x,y;
    uint16_t w,h,color,background,reserved;
    char text[32];
} gui_draw_t;
enum {GUI_BG=1,GUI_FILL,GUI_FRAME,GUI_TEXT,GUI_WINDOW};
#endif
