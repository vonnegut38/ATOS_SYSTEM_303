#include "text_terminal.h"
static term_cell_t cell(unsigned ch,unsigned attr)
{return (term_cell_t)((ch&255u)|((attr&3u)<<8));}
void term_fill(text_terminal_t *t,int x,int y,int w,int h,unsigned ch,unsigned attr)
{
 int i,j;term_cell_t c=cell(ch,attr);
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
 if(x>=80||y>=25||w<=0||h<=0)return;
 if(w>80-x)w=80-x;
 if(h>25-y)h=25-y;
 for(j=y;j<y+h;j++)for(i=x;i<x+w;i++)t->cells[j*80+i]=c;
}
void term_clear(text_terminal_t *t,unsigned attr)
{
 /* Grid defaults to the full 80x25; the console narrows it to fit its window
    so that output wraps instead of being clipped. */
 if(t->cols<1u||t->cols>TERM_COLS)t->cols=TERM_COLS;
 if(t->rows<1u||t->rows>TERM_ROWS)t->rows=TERM_ROWS;
 term_fill(t,0,0,(int)t->cols,(int)t->rows,' ',attr);
 t->column=t->row=0;t->attr=(uint8_t)(attr&3u);t->pad=0;}
void term_text(text_terminal_t *t,int x,int y,const char *s,unsigned attr,unsigned limit)
{unsigned i;int start=x;
 for(i=0;i<limit&&s[i];i++){
  if(s[i]=='\n'){x=start;y++;continue;}
  if(x>=0&&x<(int)t->cols&&y>=0&&y<(int)t->rows)
      t->cells[(unsigned)y*t->cols+(unsigned)x]=cell((uint8_t)s[i],attr);
  x++;
 }}
void term_frame(text_terminal_t *t,int x,int y,int w,int h,unsigned attr)
{
 if(w<2||h<2)return;
 term_fill(t,x+1,y,w-2,1,TERM_LINE(TERM_EAST|TERM_WEST),attr);
 term_fill(t,x+1,y+h-1,w-2,1,TERM_LINE(TERM_EAST|TERM_WEST),attr);
 term_fill(t,x,y+1,1,h-2,TERM_LINE(TERM_NORTH|TERM_SOUTH),attr);
 term_fill(t,x+w-1,y+1,1,h-2,TERM_LINE(TERM_NORTH|TERM_SOUTH),attr);
 term_fill(t,x,y,1,1,TERM_LINE(TERM_EAST|TERM_SOUTH),attr);
 term_fill(t,x+w-1,y,1,1,TERM_LINE(TERM_WEST|TERM_SOUTH),attr);
 term_fill(t,x,y+h-1,1,1,TERM_LINE(TERM_NORTH|TERM_EAST),attr);
 term_fill(t,x+w-1,y+h-1,1,1,TERM_LINE(TERM_NORTH|TERM_WEST),attr);
}
void term_scroll(text_terminal_t *t)
{
 unsigned i,total=(unsigned)t->cols*t->rows;
 for(i=0;i+1u<total;i++)t->cells[i]=t->cells[i+t->cols];
 term_fill(t,0,(int)t->rows-1,(int)t->cols,1,' ',t->attr);
}
static void next_row(text_terminal_t *t)
{t->column=0;if(t->row+1u<t->rows)t->row++;else term_scroll(t);}
static void put(text_terminal_t *t,unsigned c)
{if(t->column>=t->cols)next_row(t);
 if(t->row>=t->rows)t->row=(uint16_t)(t->rows-1u);
 t->cells[(unsigned)t->row*t->cols+t->column++]=cell(c,t->attr);}
void term_write(text_terminal_t *t,const char *s)
{while(*s){unsigned c=(uint8_t)*s++;
 if(c=='\n')next_row(t);
 else if(c=='\r')t->column=0;
 else if(c=='\b'){if(t->column){t->column--;t->cells[(unsigned)t->row*t->cols+t->column]=cell(' ',t->attr);}}
 else if(c=='\t'){unsigned n=8-(t->column&7u);while(n--)put(t,' ');}
 else if(c>=32)put(t,c);
 }}
void term_display_invalidate(term_display_t *d){d->valid=0;}
void term_font_prepare(uint8_t target[256][16],const uint8_t source[256][16])
{
 unsigned ch,row;
 for(ch=0;ch<256;ch++)for(row=0;row<16;row++){
  uint8_t bits=source[ch][row];
  if(ch>=0x80u&&ch<=0x8fu){unsigned mask=ch-0x80u;bits=0;
   if((mask&TERM_NORTH)&&row<=7)bits|=0x10;
   if((mask&TERM_SOUTH)&&row>=7)bits|=0x10;
   if(row==7){if(mask&TERM_WEST)bits|=0xf0;if(mask&TERM_EAST)bits|=0x1f;}
  }else if(ch==TERM_SHADE)bits=(row&1)?0x55:0xaa;
  else if(ch==TERM_BLOCK)bits=0xff;
  target[ch][row]=bits;
 }
}
unsigned term_render_mono8(term_display_t *d,const term_cell_t *cells,
                           volatile uint16_t *screen,const uint8_t font[256][8])
{
 unsigned y,x,row,changed=0;
 for(y=0;y<TERM_ROWS;y++)for(x=0;x<TERM_COLS;x+=2){unsigned i=y*80+x;
  term_cell_t a=cells[i]&0x03ffu,b=cells[i+1]&0x03ffu;
  if(d->valid&&d->cells[i]==a&&d->cells[i+1]==b)continue;
  {const uint8_t *ga=font[a&255u],*gb=font[b&255u];
   uint16_t invert=(uint16_t)(((a&0x100u)?0xff00u:0u)|((b&0x100u)?0x00ffu:0u));
   uint16_t underline=(uint16_t)(((a&0x200u)?0xff00u:0u)|((b&0x200u)?0x00ffu:0u));
   volatile uint16_t *out=screen+y*320+x/2;      /* 8 pixel rows per cell */
   for(row=0;row<8;row++,out+=40){uint16_t bits=(uint16_t)(((uint16_t)ga[row]<<8)|gb[row]);
    if(row==6)bits|=underline;
    *out=bits^invert;
   }
  }
  d->cells[i]=a;d->cells[i+1]=b;changed++;
 }
 d->valid=1;return changed;
}
unsigned term_render_mono(term_display_t *d,const term_cell_t *cells,
                          volatile uint16_t *screen,const uint8_t font[256][16])
{
 unsigned y,x,row,changed=0;
 for(y=0;y<TERM_ROWS;y++)for(x=0;x<TERM_COLS;x+=2){unsigned i=y*80+x;
  term_cell_t a=cells[i]&0x03ffu,b=cells[i+1]&0x03ffu;
  if(d->valid&&d->cells[i]==a&&d->cells[i+1]==b)continue;
  {const uint8_t *ga=font[a&255u],*gb=font[b&255u];
   uint16_t invert=(uint16_t)(((a&0x100u)?0xff00u:0u)|((b&0x100u)?0x00ffu:0u));
   uint16_t underline=(uint16_t)(((a&0x200u)?0xff00u:0u)|((b&0x200u)?0x00ffu:0u));
   volatile uint16_t *out=screen+y*640+x/2;
   for(row=0;row<16;row++,out+=40){uint16_t bits=(uint16_t)(((uint16_t)ga[row]<<8)|gb[row]);
    if(row==14)bits|=underline;
    *out=bits^invert;
   }
  }
  d->cells[i]=a;d->cells[i+1]=b;changed++;
 }
 d->valid=1;return changed;
}
