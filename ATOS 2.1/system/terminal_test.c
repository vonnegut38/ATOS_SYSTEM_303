#include "gui_app.h"
#include "text_terminal.h"
static text_terminal_t first,second;
static uint32_t gfx,gui;
static int rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{gui_message_t m;int r;unsigned tries=0;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)break;pause_tick();}while(++tries<100);
 if(r)return r;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];}}
static int present(text_terminal_t *t){return rpc(gfx,19,(uint32_t)(uintptr_t)t->cells,TERM_CELLS,0);}
#define CHECK(c,n) do{if(!(c)){fault=n;goto done;}}while(0)
int app_main(void){int fault=0,registered=0,captured=0;char digits[12];
 gfx=lookup("SYSTEM.GFX");gui=lookup("SYSTEM.GUI");if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 CHECK(!rpc(gui,1,0,0,0),2);registered=1;
 CHECK(!rpc(gui,6,0,0,0),3);captured=1;CHECK(!rpc(gfx,13,0,0,0),4);
 term_clear(&first,0);term_clear(&second,0);term_frame(&first,1,1,78,23,0);
 term_text(&first,3,3,"TERMINAL 80x25 - SEMIGRAPHICS",0,70);
 CHECK(present(&first)==1000,5);CHECK(present(&first)==0,6);
 term_text(&first,3,5,"X",TERM_INVERSE,1);CHECK(present(&first)==1,7);
 term_write(&second,"Background console\nIndependent cursor and buffer");
 CHECK(present(&first)==0,8);CHECK(present(&second)>0,9);CHECK(present(&second)==0,10);
 CHECK(present(&first)>0,11);CHECK(present(&first)==0,12);
 CHECK(rpc(gfx,19,1,TERM_CELLS,0)<0,13); /* Unaligned/inaccessible pointer. */
 CHECK(rpc(gfx,19,0,TERM_CELLS,0)<0,14);
 CHECK(rpc(gfx,19,(uint32_t)(uintptr_t)first.cells,TERM_CELLS-1,0)<0,15);
 CHECK(rpc(gfx,19,(uint32_t)(uintptr_t)first.cells,TERM_CELLS,1)<0,16);
 CHECK(present(&first)==0,17); /* Invalid submissions must not change cache. */
 CHECK(!rpc(gfx,2,0,0,0),18);CHECK(present(&first)==1000,19); /* Legacy drawing invalidates cells. */
 done:
 if(captured)rpc(gui,7,0,0,0);
 if(registered)rpc(gui,2,0,0,0);
 if(fault){number(digits,(unsigned)fault);gui_call(1,0,0,"TERMTST: FAIL ",0);gui_call(1,0,0,digits,0);gui_call(1,0,0,"\n",0);}
 else gui_call(1,0,0,"TERMTST: ALL PASS - 1000 / 0 / 1 PAIRS\n",0);
 return fault;
}
