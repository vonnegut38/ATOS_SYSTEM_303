#include "gui_app.h"
#include "mono_wm.h"
static wm_state state;
static gui_draw_t list[GUI_DRAW_MAX];
static uint32_t gfx,gui;
static unsigned used,dirty,quitting;
static int fault;
static int32_t launch_error;
static void handle(const gui_message_t *);
static int reliable(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){unsigned tries=0;int32_t r;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)return r;pause_tick();}while(++tries<200);return -1;}
static int32_t rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){gui_message_t m;
 if(reliable(pid,op,a,b,c))return -1;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];handle(&m);}}
static void add(unsigned kind,int x,int y,int w,int h,unsigned color,const char *text){gui_draw_t *c;
 if(used>=GUI_DRAW_MAX){fault=1;return;}c=&list[used++];zero(c,sizeof(*c));
 c->kind=kind;c->x=x;c->y=y;c->w=w;c->h=h;c->color=color;c->background=1;
 if(text)copytext(c->text,text,sizeof(c->text));}
static void text(int x,int y,const char *s){add(GUI_TEXT,x,y,0,0,0,s);}
static void content(int i){
 wm_window *w=&state.win[i];wm_rect r=wm_bounds(&state,i);char num[12],line[32];unsigned j;
 add(GUI_FILL,r.x+1,r.y+19,r.w-2,r.h-38,1,0);
 if(i==0){text(r.x+16,r.y+36,"Licznik:");number(num,w->count);text(r.x+96,r.y+36,num);
 text(r.x+16,r.y+60,w->paused?"WSTRZYMANY":"PRACUJE TAKZE W TLE");
 text(r.x+16,r.y+100,"Enter: pauza / start");}
 else{unsigned width=(r.w-32)/8;if(width>28)width=28;
 text(r.x+16,r.y+36,"Wpisz tekst (35 znakow):");
 for(j=0;j<width&&w->text[j];j++)line[j]=w->text[j];
 line[j]=0;text(r.x+16,r.y+64,line);
 if(j==width)copytext(line,w->text+width,sizeof(line));else line[0]=0;
 text(r.x+16,r.y+84,line);text(r.x+16,r.y+116,"Backspace: usun znak");}
}
static void window(int i){
 wm_window *w=&state.win[i];wm_rect r=wm_bounds(&state,i);char title[32],num[12];unsigned j,n;
 if(!w->open)return;
 copytext(title,i?"NOTATKA  PID ":"LICZNIK  PID ",sizeof(title));number(num,w->pid);
 for(n=0;title[n];n++){}for(j=0;num[j]&&n<31;j++)title[n++]=num[j];title[n]=0;
 add(GUI_WINDOW,r.x,r.y,r.w,r.h,i==state.active,title);content(i);
}
static void render(void){int a=state.active,b=1-a;used=0;
 if(dirty>=2){content(dirty-2);dirty=0;
  if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;
  return;
 }
 if(state.full>=0){
  add(GUI_FILL,0,0,640,400,1,0);
  text(8,1,state.active?"NOTATKA - F10: powrot":"LICZNIK - F10: powrot");
  content(state.active);dirty=0;
  if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;
  return;
 }
 add(GUI_BG,0,0,640,400,0,0);
 add(GUI_FILL,0,0,640,19,1,0);add(GUI_FRAME,0,0,640,19,0,0);
 text(8,1,"MonoDesk");text(168,1,"Alt+Tab: zmiana");text(352,1,"F10: okno / pelny ekran");
 if(state.full<0)window(b);
 window(a);
 add(GUI_FILL,0,377,640,23,1,0);add(GUI_FRAME,0,377,640,23,0,0);
 text(8,381,state.win[0].open?"1 LICZNIK":"1 ZAMKNIETY");text(200,381,state.win[1].open?"2 NOTATKA":"2 ZAMKNIETA");
 if(state.win[state.active].open)add(GUI_FRAME,state.active?194:2,379,182,19,0,0);
 text(404,381,"Esc: zamknij pulpit");
 dirty=0;if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;}
static void close_window(int i){if(!state.win[i].open)return;
 if(reliable(state.win[i].pid,WM_QUIT,0,0,0))fault=1;
 wm_close(&state,i);dirty=1;}
static void key(const gui_message_t *m){unsigned packed=m->v[1],scan=packed&127,mods=(packed>>16)&255;
 if((packed>>24)!=1)return;
 if(scan==1){quitting=1;return;}
 if(scan==0x0f&&(mods&8)){wm_cycle(&state);dirty=1;return;}
 if(scan==0x44&&state.win[state.active].open){state.full=state.full<0?state.active:-1;state.gesture=0;dirty=1;return;}
 if(scan==0x3e&&(mods&8)){close_window(state.active);return;}
 if(state.win[state.active].open&&reliable(state.win[state.active].pid,WM_INPUT,packed,m->v[2],0))fault=1;
}
static void mouse(const gui_message_t *m){int x=m->v[1]>>16,y=m->v[1]&65535,buttons=(m->v[2]>>24)&7;
 int down=buttons&3,press=down&&!state.buttons;wm_rect r;int i;
 if(!down&&(buttons&4)&&!state.buttons&&!state.gesture)press=1;
 if(state.gesture){wm_motion(&state,x,y);dirty=1;if(!down)state.gesture=0;state.buttons=down;return;}
 state.buttons=down;if(!press)return;
 if(state.full>=0)return;
 if(y>=377){if(x<190)wm_select(&state,0);else if(x<390)wm_select(&state,1);dirty=1;return;}
 i=wm_hit(&state,x,y);if(i<0)return;wm_select(&state,i);dirty=1;r=wm_bounds(&state,i);
 if(y<r.y+19){if(x<r.x+18){close_window(i);return;}
 if(down&&state.full<0){state.gesture=1;state.dx=x-r.x;state.dy=y-r.y;}}
 else if(down&&state.full<0&&x>=r.x+r.w-17&&y>=r.y+r.h-19){state.gesture=2;state.dx=r.x+r.w-x;state.dy=r.y+r.h-y;}
}
static void handle(const gui_message_t *m){int i;unsigned j,k;
 if(m->sender==0){if(m->v[0]==0x4b4b4559)key(m);else if(m->v[0]==0x4b4d4f55)mouse(m);return;}
 for(i=0;i<2;i++)if(state.win[i].pid==m->sender)break;
 if(i==2)return;
 if(m->v[0]==WM_HELLO){int valid=m->v[1]==(uint32_t)(i+1);state.win[i].open=valid;
 if(reliable(m->sender,WM_HELLO|0x80000000u,valid?0:1,0,0))fault=1;
 dirty=1;return;}
 if(!state.win[i].open)return;
 if(i==0&&m->v[0]==WM_COUNT){state.win[i].count=m->v[1];state.win[i].paused=m->v[2]!=0;if(state.active==i&&dirty!=1)dirty=2;}
 if(i==1&&m->v[0]>=WM_TEXT0&&m->v[0]<=WM_TEXT2){j=m->v[0]-WM_TEXT0;
 if(!j)state.win[i].parts=0;
 for(k=0;k<12;k++)state.win[i].staging[j*12+k]=((const char *)&m->v[1])[k];
 state.win[i].parts|=1u<<j;
 if(state.win[i].parts==7){for(k=0;k<35;k++)state.win[i].text[k]=state.win[i].staging[k];state.win[i].text[35]=0;if(state.active==i&&dirty!=1)dirty=3;}}
}
static int launch(int i,const char *path){gui_spawn_t request;zero(&request,sizeof(request));copytext(request.path,path,sizeof(request.path));
 launch_error=gui_call(34,0,0,&request,0);
 if(launch_error)return 0;
 state.win[i].pid=request.pid;return 1;}
static void reap(void){unsigned i;uint32_t result[2];for(i=0;i<2;i++)if(state.win[i].pid){
 if(gui_call(32,state.win[i].pid,1,result,0)==0&&result[0]==5){state.win[i].pid=0;wm_close(&state,i);dirty=1;}}}
int app_main(void){gui_message_t m;unsigned i;int registered=0,captured=0;uint32_t start;
 wm_init(&state);gfx=lookup("SYSTEM.GFX");gui=lookup("SYSTEM.GUI");
 if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(gui_call(39,0,0,"SYSTEM.WM",0))return 2;
 if(rpc(gui,1,0,0,0))return 3;
 registered=1;
 if(rpc(gui,6,0,0,0)){fault=6;goto done;}captured=1;
 if(rpc(gfx,13,0,0,0)){fault=13;goto done;}
 if(rpc(gfx,1,0,0,0)!=(int32_t)((640u<<16)|400u)){fault=43;goto done;}
 if(!launch(0,"\\SYSTEM\\APP\\COUNT.APP")){fault=34;goto done;}
 if(!launch(1,"\\SYSTEM\\APP\\NOTE.APP")){fault=35;goto done;}
 dirty=1;
 while(!quitting&&!fault){for(i=0;i<8&&receive(&m,0)==0;i++)handle(&m);reap();if(dirty)render();pause_tick();}
done:
 for(i=0;i<2;i++)if(state.win[i].pid)reliable(state.win[i].pid,WM_QUIT,0,0,0);
 start=(uint32_t)ticks();while((state.win[0].pid||state.win[1].pid)&&(uint32_t)ticks()-start<100){
  while(receive(&m,0)==0){}reap();pause_tick();}
 if((state.win[0].pid||state.win[1].pid)&&!fault)fault=32;
 if(captured)rpc(gui,7,0,0,0);
 if(registered)rpc(gui,2,0,0,0);
 gui_call(1,0,0,fault?"MONODESK: FAILED\n":"MONODESK: CHILDREN CLOSED / EXIT OK\n",0);
 if(fault==34||fault==35)return launch_error;
 return fault;
}
