#ifndef GUI_DAMAGE_H
#define GUI_DAMAGE_H
#include "gui_draw.h"
/* Conservative bounds for the bounded display list. Coordinates are widened
 * before addition; text may contain newlines. Rectangles are half-open. */
static int gui_damage(const gui_draw_t *list,unsigned count,int width,int height,
                      int *left,int *top,int *right,int *bottom)
{
 unsigned i,j;int any=0;*left=width;*top=height;*right=*bottom=0;
 for(i=0;i<count;i++){const gui_draw_t *c=&list[i];int x=c->x,y=c->y,r=x+c->w,b=y+c->h;
  if(c->kind==GUI_TEXT){int col=0,max=0,lines=1;
   for(j=0;j<31&&c->text[j];j++){if(c->text[j]=='\n'){if(col>max)max=col;col=0;lines++;}else col++;}
   if(col>max)max=col;
   r=x+max*8;b=y+lines*16;
  }else if(c->kind==GUI_WINDOW){
   /* A window graphic paints its own rectangle (plus a small shadow margin).
      Reporting the whole surface here made EVERY batch that contained a window
      damage the entire screen, so a window move repainted all 640x400 - the
      reason dragging felt far too slow. */
   x-=4;y-=4;r+=4;b+=4;
  }else if(c->kind==GUI_ICON||c->kind==GUI_ICON_RGB){
   /* Union of the bitmap and its horizontally centred label.  Rozmiar bierzemy
      z POL ELEMENTU (w/h), bo ikony maja rozmiar wybierany w SETTINGS
      (32/48/64); stale sa tylko fallbackiem dla starszych wywolan z w/h = 0. */
   int isz=c->w?(int)c->w:((c->kind==GUI_ICON_RGB)?GUI_ICON_COLOR_SIZE:GUI_ICON_MONO_SIZE);
   int ih =c->h?(int)c->h:((c->kind==GUI_ICON_RGB)?GUI_ICON_COLOR_H:GUI_ICON_MONO_H);
   unsigned len=0;while(len<12u&&c->text[len])++len;
   {int w=(int)len*8;if(w<isz)w=isz;x+=(isz-w)/2;r=x+w;}
   b=y+ih;
  }else if(c->kind<GUI_BG||(c->kind>GUI_BEVEL&&c->kind!=GUI_PATTERN))continue;
   /* GUI_PATTERN (tapeta) musi wejsc do uszkodzenia jak kazdy prostokat -
      wczesniej wypadal z zakresu BG..BEVEL, wiec jego area nie byla nigdy
      odswiezana i tapeta "nie dzialala", mimo ze jadro ja rysowalo. */
  if(x<0)x=0;
  if(y<0)y=0;
  if(r>width)r=width;
  if(b>height)b=height;
  if(x>=r||y>=b)continue;
  if(x<*left)*left=x;
  if(y<*top)*top=y;
  if(r>*right)*right=r;
  if(b>*bottom)*bottom=b;
  any=1;
 }return any;
}
static void gui_damage_copy(volatile uint16_t *dst,const volatile uint16_t *src,
                            unsigned line_words,unsigned first,unsigned words,
                            unsigned top,unsigned bottom)
{
 unsigned row,j;
 for(row=top;row<bottom;row++){
  unsigned offset=row*line_words+first;
  for(j=0;j<words;j++)dst[offset+j]=src[offset+j];
 }
}
#endif
