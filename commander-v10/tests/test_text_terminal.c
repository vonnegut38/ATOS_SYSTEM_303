#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text_terminal.h"
static text_terminal_t a,b;
static term_display_t display;
static uint8_t source[256][16],font[256][16];
static uint16_t screen[16002];
static void reference(void){unsigned y,x,r;for(y=0;y<25;y++)for(x=0;x<80;x++){
 unsigned c=a.cells[y*80+x];for(r=0;r<16;r++){
  unsigned bits=font[c&255][r];if((c&0x200)&&r==14)bits=255;if(c&0x100)bits^=255;
  unsigned actual=screen[1+(y*16+r)*40+x/2];actual=(x&1)?(actual&255):(actual>>8);assert(actual==bits);
 }}assert(screen[0]==0xbeef&&screen[16001]==0xbeef);}
int main(void){unsigned c,r,x,n;uint32_t rng=7193;
 for(c=0;c<256;c++)for(r=0;r<16;r++)source[c][r]=(uint8_t)(c*19+r*13);
 memset(source[' '],0,16);term_font_prepare(font,source);
 for(c=0;c<16;c++)for(r=0;r<16;r++)for(x=0;x<8;x++){
  unsigned on=((c&1)&&x==3&&r<=7)||((c&4)&&x==3&&r>=7)||((c&8)&&r==7&&x<=3)||((c&2)&&r==7&&x>=3);
  assert(((font[128+c][r]>>(7-x))&1)==on);
 }
 term_clear(&a,0);term_clear(&b,0);for(x=0;x<16002;x++)screen[x]=0xbeef;
 assert(term_render_mono(&display,a.cells,screen+1,font)==1000);reference();
 assert(!term_render_mono(&display,a.cells,screen+1,font));
 term_text(&a,3,4,"A",TERM_INVERSE,1);assert(term_render_mono(&display,a.cells,screen+1,font)==1);reference();
 for(c=0;c<256;c++)for(r=0;r<4;r++){term_fill(&a,79,24,1,1,c,r);assert(term_render_mono(&display,a.cells,screen+1,font)<=1);reference();}
 for(n=0;n<400;n++){rng=rng*1664525u+1013904223u;x=(rng>>8)%2000;a.cells[x]=(uint16_t)rng;assert(term_render_mono(&display,a.cells,screen+1,font)<=1);reference();}
 term_display_invalidate(&display);assert(term_render_mono(&display,a.cells,screen+1,font)==1000);reference();
 /* Updating a background terminal never touches the displayed bitmap. */
 term_text(&b,0,0,"BACKGROUND",0,10);assert(!term_render_mono(&display,a.cells,screen+1,font));
 assert(term_render_mono(&display,b.cells,screen+1,font)>0);assert(!term_render_mono(&display,b.cells,screen+1,font));
 term_clear(&a,0);term_write(&a,"abc\bD\rZ\nnext\t!");assert(a.cells[0]=='Z'&&a.cells[1]=='b'&&a.cells[2]=='D');assert(a.cells[80]=='n'&&a.cells[88]=='!');
 term_clear(&a,0);for(n=0;n<80;n++)term_write(&a,"x");assert(a.column==80&&a.row==0);term_write(&a,"\nY");assert(a.row==1&&a.cells[80]=='Y');
 term_clear(&a,0);for(n=0;n<25;n++){term_write(&a,"line");if(n<24)term_write(&a,"\n");}term_write(&a,"\nlast");assert(a.row==24&&a.cells[24*80]=='l');
 term_clear(&a,0);term_fill(&a,-2,-2,4,4,'Q',1);assert(a.cells[0]==0x151&&a.cells[1]==0x151&&a.cells[2]==' '&&a.cells[160]==' ');
 term_frame(&a,1,1,10,4,0);assert(a.cells[81]==TERM_LINE(6)&&a.cells[90]==TERM_LINE(12)&&a.cells[321]==TERM_LINE(3));
 puts("Terminal PASS: semigraphics, attributes, raster reference, bounds, scroll/wrap, isolated buffers; initial 32000 bytes / unchanged 0 / single cell 32 bytes");return 0;}
