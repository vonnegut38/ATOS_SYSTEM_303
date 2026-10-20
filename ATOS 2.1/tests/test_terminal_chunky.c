#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text_terminal.h"
static text_terminal_t terminal;
static term_display_t display;
static uint32_t pixels[800*600/4];
static uint8_t font[256][16];
int main(void)
{
 unsigned first,idle,single,invalidated;
 term_clear(&terminal,0);
 memset(font['A'],0x5a,16);
 first=term_render_chunky_polled(&display,terminal.cells,(uint8_t *)pixels,800,font,0);
 idle=term_render_chunky_polled(&display,terminal.cells,(uint8_t *)pixels,800,font,0);
 terminal.cells[10]='A';
 single=term_render_chunky_polled(&display,terminal.cells,(uint8_t *)pixels,800,font,0);
 printf("CHUNKY cells rendered: first=%u unchanged=%u one-change=%u\n",first,idle,single);fflush(stdout);
 assert(first==TERM_CELLS && idle==0 && single==1 && display.valid);
 assert(((uint8_t *)pixels)[80]==0 && ((uint8_t *)pixels)[81]==1);
 term_display_invalidate(&display);
 invalidated=term_render_chunky_polled(&display,terminal.cells,(uint8_t *)pixels,800,font,0);
 assert(invalidated==TERM_CELLS);
 puts("Chunky terminal cache PASS");
 return 0;
}
