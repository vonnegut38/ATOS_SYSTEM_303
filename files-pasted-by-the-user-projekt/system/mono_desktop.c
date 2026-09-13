#include "gui_app.h"
#include "mono_wm.h"

#define DEMO_CLIENTS 2
#define INTERNAL_PID 0xfffffff0u
#define TASKBAR_SLOT_W 100

static wm_state state;
static gui_draw_t list[GUI_DRAW_MAX];
static uint32_t gfx,gui;
static unsigned used,dirty,quitting;
static int fault;
static int32_t launch_error;
static uint32_t spawn_pid[DEMO_CLIENTS];
static void handle(const gui_message_t *);

/* Kompilator potrafi wygenerowac te prymitywy dla wiekszych struktur, a
 * aplikacje linkujemy bez libc. */
void *memcpy(void *destination,const void *source,unsigned length)
{uint8_t *d=(uint8_t *)destination;const uint8_t *s=(const uint8_t *)source;
 while(length--){*d++=*s++;}
 return destination;}
void *memset(void *destination,int value,unsigned length)
{uint8_t *d=(uint8_t *)destination;
 while(length--){*d++=(uint8_t)value;}
 return destination;}

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

static int slot_of(uint32_t pid){int i;for(i=0;i<WM_MAX;++i)if(state.win[i].open&&state.win[i].pid==pid)return i;return -1;}
static const char *kind_name(unsigned kind){return kind==2?"NOTATKA":kind==1?"LICZNIK":kind==3?"SYSTEM":"OKNO";}

static void content(int i){
 wm_window *w=&state.win[i];wm_rect r=wm_bounds(&state,i);char num[12],line[32];unsigned j;
 add(GUI_FILL,r.x+1,r.y+19,r.w-2,r.h-38,1,0);
 if(w->kind==1){text(r.x+16,r.y+36,"Licznik:");number(num,w->count);text(r.x+96,r.y+36,num);
  text(r.x+16,r.y+60,w->paused?"WSTRZYMANY":"PRACUJE TAKZE W TLE");
  text(r.x+16,r.y+100,"Enter: pauza / start");}
 else if(w->kind==2){unsigned width=(r.w-32)/8;if(width>28)width=28;
  text(r.x+16,r.y+36,"Wpisz tekst (35 znakow):");
  for(j=0;j<width&&w->text[j];j++)line[j]=w->text[j];
  line[j]=0;text(r.x+16,r.y+64,line);
  if(j==width)copytext(line,w->text+width,sizeof(line));else line[0]=0;
  text(r.x+16,r.y+84,line);text(r.x+16,r.y+116,"Backspace: usun znak");}
 else{number(num,(uint32_t)state.count);text(r.x+16,r.y+36,"Okien otwartych:");
  text(r.x+150,r.y+36,num);text(r.x+16,r.y+60,"Kolejnosc Z, fokus i przeciaganie");
  text(r.x+16,r.y+84,"dzialaja dla N okien.");}
}
static void window(int i){
 wm_window *w=&state.win[i];wm_rect r=wm_bounds(&state,i);char title[32],num[12];unsigned j,b=0;
 if(!w->open)return;
 {const char *nm=kind_name(w->kind);for(j=0;nm[j]&&b<20;++j)title[b++]=nm[j];}
 if(w->kind!=3){const char pid_label[]="  PID ";for(j=0;pid_label[j]&&b<24;++j)title[b++]=pid_label[j];
  number(num,w->pid);for(j=0;num[j]&&b<31;++j)title[b++]=num[j];}
 title[b]=0;
 add(GUI_WINDOW,r.x,r.y,r.w,r.h,i==state.active,title);
 content(i);
}

static int taskbar_count(void){int i,n=0;for(i=0;i<WM_MAX;++i)if(state.win[i].open)++n;return n;}
static int taskbar_slot(int index){int i,n=0;for(i=0;i<WM_MAX;++i)if(state.win[i].open){if(n++==index)return i;}return -1;}

static void render(void){
 int k,maxs;
 used=0;
 if(dirty>=2){
  int i=(int)dirty-2;dirty=0;
  if(wm_is_open(&state,i))content(i);
  if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;
  return;}
 if(state.full>=0){int f=state.full;
  add(GUI_FILL,0,0,640,400,1,0);
  text(8,1,"F10: powrot do okien");
  content(f);dirty=0;
  if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;
  return;}
 add(GUI_BG,0,0,640,400,0,0);
 add(GUI_FILL,0,0,640,19,1,0);add(GUI_FRAME,0,0,640,19,0,0);
 text(8,1,"MonoDesk");text(168,1,"Alt+Tab: zmiana");text(352,1,"F10: okno / pelny ekran");
 for(k=0;k<state.count;++k){int i=state.order[k];if(state.win[i].open)window(i);}
 add(GUI_FILL,0,377,640,23,1,0);add(GUI_FRAME,0,377,640,23,0,0);
 maxs=taskbar_count();if(maxs>6)maxs=6;
 for(k=0;k<maxs;++k){
  int i=taskbar_slot(k);char label[24];unsigned j,b=0;
  if(i<0)continue;
  label[b++]=(char)('1'+k);label[b++]=' ';
  {const char *nm=kind_name(state.win[i].kind);for(j=0;nm[j]&&b<22;++j)label[b++]=nm[j];}
  label[b]=0;
  text(8+k*TASKBAR_SLOT_W,381,label);
  if(i==state.active)add(GUI_FRAME,6+k*TASKBAR_SLOT_W,379,TASKBAR_SLOT_W-4,19,0,0);
 }
 text(8+6*TASKBAR_SLOT_W>560?560:8+6*TASKBAR_SLOT_W,381,"Esc: wyjscie");
 dirty=0;if(rpc(gfx,18,(uint32_t)(uintptr_t)list,used,0))fault=18;}

static void close_window(int i){
 if(!wm_is_open(&state,i))return;
 if(state.win[i].kind!=3&&state.win[i].pid)if(reliable(state.win[i].pid,WM_QUIT,0,0,0))fault=1;
 wm_close(&state,i);dirty=1;}

static void key(const gui_message_t *m){unsigned packed=m->v[1],scan=packed&127,mods=(packed>>16)&255;
 if((packed>>24)!=1)return;
 if(scan==1){quitting=1;return;}
 if(scan==0x0f&&(mods&8)){wm_cycle(&state);dirty=1;return;}
 if(scan==0x44){if(state.active>=0){state.full=state.full<0?state.active:-1;state.gesture=0;dirty=1;}return;}
 if(scan==0x3e&&(mods&8)){close_window(state.active);return;}
 if(state.active>=0&&state.win[state.active].open&&state.win[state.active].kind!=3)
  if(reliable(state.win[state.active].pid,WM_INPUT,packed,m->v[2],0))fault=1;
}
static void mouse(const gui_message_t *m){int x=m->v[1]>>16,y=m->v[1]&65535,buttons=(m->v[2]>>24)&7;
 int down=buttons&3,press=down&&!state.buttons;wm_rect r;int i;
 if(!down&&(buttons&4)&&!state.buttons&&!state.gesture)press=1;
 if(state.gesture){wm_motion(&state,x,y);dirty=1;if(!down)state.gesture=0;state.buttons=down;return;}
 state.buttons=down;if(!press)return;
 if(state.full>=0)return;
 if(y>=377){int k=(x-6)/TASKBAR_SLOT_W;i=taskbar_slot(k);if(i>=0){wm_select(&state,i);dirty=1;}return;}
 i=wm_hit(&state,x,y);if(i<0)return;wm_select(&state,i);dirty=1;r=wm_bounds(&state,i);
 if(y<r.y+19){if(x<r.x+18){close_window(i);return;}
  if(down){state.gesture=1;state.dx=x-r.x;state.dy=y-r.y;}}
 else if(down&&x>=r.x+r.w-17&&y>=r.y+r.h-19){state.gesture=2;state.dx=r.x+r.w-x;state.dy=r.y+r.h-y;}
}
static void handle(const gui_message_t *m){int i;
 if(m->sender==0){if(m->v[0]==0x4b4b4559)key(m);else if(m->v[0]==0x4b4d4f55)mouse(m);return;}
 i=slot_of(m->sender);
 if(m->v[0]==WM_HELLO){
  if(m->v[1]!=1u&&m->v[1]!=2u){reliable(m->sender,WM_HELLO|0x80000000u,1,0,0);return;}
  if(i<0){i=wm_open(&state,m->sender,kind_name(m->v[1]));if(i>=0)state.win[i].kind=(uint8_t)m->v[1];}
  if(i<0){reliable(m->sender,WM_HELLO|0x80000000u,1,0,0);return;}
  if(reliable(m->sender,WM_HELLO|0x80000000u,0,0,0))fault=1;
  dirty=1;return;}
 if(i<0||!state.win[i].open)return;
 if(state.win[i].kind==1&&m->v[0]==WM_COUNT){state.win[i].count=m->v[1];state.win[i].paused=m->v[2]!=0;if(state.active==i&&dirty!=1)dirty=2+i;}
 if(state.win[i].kind==2&&m->v[0]>=WM_TEXT0&&m->v[0]<=WM_TEXT2){unsigned j=m->v[0]-WM_TEXT0,k;
  if(!j)state.win[i].parts=0;
  for(k=0;k<12;k++)state.win[i].staging[j*12+k]=((const char *)&m->v[1])[k];
  state.win[i].parts|=(uint8_t)(1u<<j);
  if(state.win[i].parts==7){for(k=0;k<35;k++)state.win[i].text[k]=state.win[i].staging[k];state.win[i].text[35]=0;if(state.active==i&&dirty!=1)dirty=2+i;}}
}
static int launch(int index,const char *path){gui_spawn_t request;zero(&request,sizeof(request));copytext(request.path,path,sizeof(request.path));
 launch_error=gui_call(34,0,0,&request,0);
 if(launch_error)return 0;
 spawn_pid[index]=request.pid;return 1;}
static void reap(void){unsigned i;uint32_t result[2];
 for(i=0;i<DEMO_CLIENTS;i++)if(spawn_pid[i]){
  if(gui_call(32,spawn_pid[i],1,result,0)==0&&result[0]==5){int s=slot_of(spawn_pid[i]);if(s>=0)wm_close(&state,s);spawn_pid[i]=0;dirty=1;}}}
static int any_pid(void){return spawn_pid[0]||spawn_pid[1];}

int app_main(void){gui_message_t m;unsigned i;int registered=0,captured=0;uint32_t start;
 wm_init(&state);gfx=lookup("SYSTEM.GFX");gui=lookup("SYSTEM.GUI");
 if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(gui_call(39,0,0,"SYSTEM.WM",0))return 2;
 if(rpc(gui,1,0,0,0))return 3;
 registered=1;
 if(rpc(gui,6,0,0,0)){fault=6;goto done;}
 captured=1;
 if(rpc(gfx,13,0,0,0)){fault=13;goto done;}
 if(rpc(gfx,1,0,0,0)!=(int32_t)((640u<<16)|400u)){fault=43;goto done;}
 if(!launch(0,"\\SYSTEM\\APP\\COUNT.APP")){fault=34;goto done;}
 if(!launch(1,"\\SYSTEM\\APP\\NOTE.APP")){fault=35;goto done;}
 wm_open(&state,INTERNAL_PID,"SYSTEM");state.win[slot_of(INTERNAL_PID)].kind=3;
 dirty=1;
 while(!quitting&&!fault){for(i=0;i<8&&receive(&m,0)==0;i++)handle(&m);reap();if(dirty)render();pause_tick();}
done:
 for(i=0;i<WM_MAX;++i)if(state.win[i].open&&state.win[i].kind!=3&&state.win[i].pid)
  reliable(state.win[i].pid,WM_QUIT,0,0,0);
 start=(uint32_t)ticks();while(any_pid()&&(uint32_t)ticks()-start<100){while(receive(&m,0)==0){}reap();pause_tick();}
 if(any_pid()&&!fault)fault=32;
 if(captured)rpc(gui,7,0,0,0);
 if(registered)rpc(gui,2,0,0,0);
 gui_call(1,0,0,fault?"MONODESK: FAILED\n":"MONODESK: N-OKIEN / EXIT OK\n",0);
 if(fault==34||fault==35)return launch_error;
 return fault;
}
