#include <stdio.h>
#include "gui_damage.h"
int main(void){gui_draw_t c={0};int l,t,r,b;c.kind=GUI_ICON_RGB;c.x=100;c.y=100;c.w=48;c.h=64;
 int ok=gui_damage(&c,1,800,600,&l,&t,&r,&b);
 printf("bounds %d,%d..%d,%d; need right>=148 bottom>=164\n",l,t,r,b);
 return !(ok&&r>=148&&b>=164);
}
