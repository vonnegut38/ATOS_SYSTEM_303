/* Native language setup. Existing BIOS hardware setup remains independent. */
#include "gui_app.h"
#include "text_terminal.h"
#define LNG_SET
#include "app_lng.h"
static text_terminal_t screen;
static uint32_t gfx,gui;
static unsigned selected,quit,dirty=1,status_id=SET_CURRENT,mouse_down;
static int pack_ok;
static gui_message_t pending[16];static unsigned head,tail;
static void enqueue(const gui_message_t *m)
{unsigned i;if(m->sender||tail-head>=16)return;
 for(i=0;i<sizeof(*m);i++)((uint8_t *)&pending[tail&15])[i]=((const uint8_t *)m)[i];
 tail++;}
static int rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b)
{gui_message_t m;unsigned tries=0;int r;
 do{r=send(pid,op,a,b,0);if(r!=-0x3c2)break;pause_tick();}while(++tries<100);
 if(r)return r;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];enqueue(&m);}}
static void choose(unsigned language)
{selected=language;pack_ok=lng_load(selected)==0;status_id=pack_ok?SET_CURRENT:SET_PACK_ERROR;dirty=1;}
static void key(unsigned scan)
{
 if(scan==1||scan==0x44){quit=1;return;}
 if(scan==0x48||scan==0x50||scan==0x4b||scan==0x4d||scan==15){choose(selected^1u);return;}
 if(scan==28){status_id=!pack_ok?SET_PACK_ERROR:gui_call(59,selected,0,0,0)?SET_ERROR:SET_SAVED;dirty=1;}
}
static void handle(const gui_message_t *m)
{if(m->sender)return;
 if(m->v[0]==0x4b4b4559&&(m->v[1]>>24)==1)key(m->v[1]&127);
 else if(m->v[0]==0x4b4d4f55){unsigned b=m->v[2]>>24,down=b&3,press=(down&~mouse_down)|((b>>3)&3);
  unsigned x=(m->v[1]>>16)/8,y=(m->v[1]&65535)/16;mouse_down=down;
  if(!(press&2))return;
  if(x>=8&&x<72&&(y==8||y==10))choose(y==10);
  else if(y==24&&x<40)key(28);else if(y==24)key(1);
 }}
static int render(void)
{term_clear(&screen,0);term_frame(&screen,1,1,78,22,0);
 term_text(&screen,3,2,lng_text(SET_TITLE),0,74);
 term_text(&screen,8,6,lng_text(SET_LANGUAGE),0,64);
 term_fill(&screen,8,8,64,1,' ',!selected?TERM_INVERSE:0);
 term_text(&screen,10,8,"English (EN.LNG)",!selected?TERM_INVERSE:0,60);
 term_fill(&screen,8,10,64,1,' ',selected?TERM_INVERSE:0);
 term_text(&screen,10,10,"Polski (PL.LNG)",selected?TERM_INVERSE:0,60);
 term_text(&screen,3,14,lng_text(SET_NOTE),0,74);
 term_text(&screen,3,15,lng_text(SET_NOTE2),0,74);
 term_text(&screen,3,19,lng_text(SET_HELP),0,74);
 term_text(&screen,1,23,lng_text(status_id),0,78);
 term_fill(&screen,0,24,80,1,' ',TERM_INVERSE);
 term_text(&screen,1,24,lng_text(SET_SAVE),TERM_INVERSE,38);
 term_text(&screen,41,24,lng_text(SET_EXIT),TERM_INVERSE,38);
 dirty=0;return rpc(gfx,19,(uint32_t)(uintptr_t)screen.cells,TERM_CELLS);}
int app_main(void)
{gui_message_t m;int r=0,registered=0,captured=0;lng_init();choose(lng_selected);
 gfx=(uint32_t)lookup("SYSTEM.GFX");gui=(uint32_t)lookup("SYSTEM.GUI");
 if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(rpc(gui,1,0,0))return 2;
 registered=1;
 if(rpc(gui,6,0,0)){r=3;goto done;}captured=1;
 if(rpc(gfx,13,0,0)){r=4;goto done;}
 while(!quit){if(dirty&&render()<0){r=5;break;}
  if(head!=tail){unsigned i;for(i=0;i<sizeof(m);i++)((uint8_t *)&m)[i]=((uint8_t *)&pending[head&15])[i];head++;}
  else if(receive(&m,1))break;
  handle(&m);
 }
 done:if(captured)rpc(gui,7,0,0);if(registered)rpc(gui,2,0,0);return r;
}
