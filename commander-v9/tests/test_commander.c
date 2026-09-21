/* Exercise actual application handlers and submitted drawing commands. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define app_main commander_main
#include "../system/commander.c"
#undef app_main
static unsigned calls,mutation,operation,clock_value,reply_pid,reply_op,draw_count;
static int32_t directory_count=40;
static term_cell_t expected[TERM_CELLS];
static term_display_t display;
static uint8_t font[256][16],rawfont[256][16];
static uint16_t pixels[16000];
static int reply_value;
static void compare_full(void){memcpy(expected,terminal.cells,sizeof(expected));dirty=1;render();assert(!memcmp(expected,terminal.cells,sizeof(expected)));assert(!used);}
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q){
 if(call==52){entry *out=q;unsigned i,n;calls++;n=a<(unsigned)directory_count?(unsigned)directory_count-a:0;if(n>b)n=b;
 for(i=0;i<n;i++){memset(&out[i],0,sizeof(entry));strcpy(out[i].name,i==0?"DIR":"FILE.TXT");out[i].attr=i==0?16:32;out[i].size=1200;}return n;}
 if(call==10)return clock_value;
 if(call==36){uint32_t *v=p;reply_pid=a;reply_op=v[0];if(reply_op==19){assert(v[2]==TERM_CELLS);draw_count=term_render_mono(&display,terminal.cells,pixels,font);reply_value=(int)draw_count;}else reply_value=0;return 0;}
 if(call==38){gui_message_t *m=p;memset(m,0,sizeof(*m));m->sender=reply_pid;m->v[0]=reply_op|0x80000000u;m->v[1]=(uint32_t)reply_value;return 0;}
 if((call>=49&&call<=51)||(call>=53&&call<=55)){mutation++;operation=call;return 0;}
 return 0;
}
static void reset(void){memset(panels,0,sizeof(panels));strcpy(panels[0].path,"\\");strcpy(panels[1].path,"\\");active=dirty=modal=quit=used=length=0;fault=0;panel_damage=painted_valid=0;term_display_invalidate(&display);calls=mutation=0;gfx=2;gui=3;click_valid=mouse_buttons=0;head=tail=0;directory_count=40;load(&panels[0]);load(&panels[1]);}
static void click_at(unsigned x,unsigned y,unsigned buttons){gui_message_t m={0,{0}};m.v[0]=0x4b4d4f55;m.v[1]=(x<<16)|y;m.v[2]=buttons<<24;mouse(&m);}
int main(void){unsigned full,old_calls,i,j;
 for(i=0;i<256;i++)for(j=0;j<16;j++)rawfont[i][j]=(uint8_t)(i+j);
 memset(rawfont[32],0,16);term_font_prepare(font,rawfont);
 reset();assert(calls==2&&panels[0].count==16&&panels[0].more);render();full=draw_count;assert(!fault);
 assert(full==1000&&terminal.cells[80]==TERM_LINE(6));
 assert(terminal.cells[81]==TERM_LINE(10));
 action(0x50,0);assert(dirty==3&&panels[0].sel==1);old_calls=calls;render();assert(draw_count==40&&calls==old_calls);
 action(15,0);render();assert(draw_count==80&&active==1);
 action(0x41,0);render();assert(!fault&&draw_count<=1000&&modal==7);
 action(0,'a');render();assert(draw_count==1&&draw_count<full&&strcmp(input,"a")==0);
 click_at(330,240,2);assert(!modal&&!mutation);click_at(330,240,0);
 click_at(10,82,2);assert(active==0&&panels[0].sel==1);click_at(10,82,0);
 action(0x3f,0);assert(modal==5);click_at(140,240,2);assert(mutation==1&&operation==49&&!modal);click_at(140,240,0);
 click_at(170,390,2);assert(modal==7);click_at(170,390,0);action(0,'n');click_at(140,240,2);assert(mutation==2&&operation==50);click_at(140,240,0);
 action(0x50,0);click_at(330,390,2);assert(modal==8);click_at(330,390,0);click_at(140,240,2);assert(mutation==3&&operation==53);click_at(140,240,0);
 reset();clock_value=100;click_at(10,65,2);click_at(10,65,0);assert(!strcmp(panels[0].path,"\\"));clock_value=115;click_at(10,65,2);assert(!strcmp(panels[0].path,"\\DIR"));click_at(10,65,0);
 /* Right click remains distinguishable even if press/release coalesce. */
 click_at(10,80,12);assert(!strcmp(panels[0].path,"\\"));
 old_calls=calls;click_at(280,330,20);assert(panels[0].top==16&&calls==old_calls+1);
 reset();strcpy(panels[0].path,"\\DIR");calls=0;load(&panels[0]);assert(calls==1&&!strcmp(panels[0].item[0].name,"..")&&panels[0].count==16&&panels[0].more);
 directory_count=0;load(&panels[0]);assert(panels[0].count==1&&!panels[0].more);render();
 click_at(10,140,20);assert(!modal&&!quit); /* empty row does nothing */
 click_at(500,390,20);assert(quit);
 reset();render();directory_count=4;strcpy(panels[0].path,"\\DIR");load(&panels[0]);assert(dirty==4);render();assert(draw_count>0&&draw_count<400);compare_full();
 directory_count=16;strcpy(panels[0].path,"\\");load(&panels[0]);render();assert(draw_count>0&&draw_count<400);compare_full();
 /* Batched panel load, focus switch and selection must match a full repaint. */
 load(&panels[0]);action(15,0);action(0x50,0);render();compare_full();
 strcpy(panels[0].path,"\\TRASH\\T0000001");active=0;load(&panels[0]);panels[0].sel=1;render();action(0x42,0);assert(modal==9);action(0,'y');assert(operation==55);
 action(0x40,0);assert(modal==6);action(0,'y');assert(operation==54);
 strcpy(panels[0].path,"\\");load(&panels[0]);strcpy(panels[0].item[0].name,"TRASH");panels[0].sel=0;action(0x42,0);assert(!modal);
 printf("Commander terminal PASS: first %u pairs; selection 40, Tab 80, typed character 1, unchanged 0; pixel bytes 32000 / 1280 / 2560 / 32 / 0; mouse/trash/restore/purge and incremental-grid equivalence\n",full);
 return 0;}
