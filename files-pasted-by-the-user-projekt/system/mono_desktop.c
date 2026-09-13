#include "gui_app.h"
#include "mono_wm.h"

#define DEMO_CLIENTS 2
#define INTERNAL_PID 0xfffffff0u
#define TASKBAR_SLOT_W 100
#define MENUBAR_H 19
#define MENU_COUNT 4
#define TASKS_X 576
#define TASKS_W 56
#define TASKS_DROP_X 432
#define TASKS_DROP_W 200
#define TASKS_ITEM_H 17

static wm_state state;
static gui_draw_t list[GUI_DRAW_MAX];
static uint32_t gfx,gui;
static unsigned used,dirty,quitting;
static int fault;
static int32_t launch_error;
static uint32_t spawn_pid[DEMO_CLIENTS];
static int menu_open=-1,menu_highlight=0,tasks_open=0,dialog=0;
static const char *status=0;
static int32_t dmg_x,dmg_y,dmg_w,dmg_h;static int dmg_valid=0;
static void dmg_add(int32_t x,int32_t y,int32_t w,int32_t h){
 if(w<=0||h<=0)return;
 if(!dmg_valid){dmg_x=x;dmg_y=y;dmg_w=w;dmg_h=h;dmg_valid=1;return;}
 {int32_t x1=dmg_x+dmg_w,y1=dmg_y+dmg_h,nx=x+w,ny=y+h;
  if(x<dmg_x)dmg_x=x;
  if(y<dmg_y)dmg_y=y;
  if(nx>x1)x1=nx;
  if(ny>y1)y1=ny;
  dmg_w=x1-dmg_x;dmg_h=y1-dmg_y;}
}
static void mark_window(int i){wm_rect r=wm_bounds(&state,i);dmg_add(r.x,r.y,r.w,r.h);dirty=2+i;}

/* Widoczny region okna w stylu XaAES rectlist.c: obszar okna minus okna
 * lezace WYZEJ w kolejnosci Z (state.order rosnie od spodu do wierzchu). */
typedef struct {int32_t x,y,w,h;} rr_t;
#define RG_MAX 8
static int rg_sub(rr_t *in,int n,const rr_t *c,rr_t *out){
 int i,m=0;int32_t cx1,cy1;
 if(c->w<=0||c->h<=0){for(i=0;i<n&&m<RG_MAX;++i)out[m++]=in[i];return m;}
 cx1=c->x+c->w;cy1=c->y+c->h;
 for(i=0;i<n&&m<RG_MAX;++i){
  rr_t r=in[i];int32_t x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h,ty,by;
  if(cx1<=x0||c->x>=x1||cy1<=y0||c->y>=y1){out[m++]=r;continue;}
  ty=c->y>y0?c->y:y0;by=cy1<y1?cy1:y1;
  if(c->y>y0&&m<RG_MAX){out[m].x=x0;out[m].y=y0;out[m].w=x1-x0;out[m].h=c->y-y0;++m;}
  if(cy1<y1&&m<RG_MAX){out[m].x=x0;out[m].y=cy1;out[m].w=x1-x0;out[m].h=y1-cy1;++m;}
  if(c->x>x0&&by>ty&&m<RG_MAX){out[m].x=x0;out[m].y=ty;out[m].w=c->x-x0;out[m].h=by-ty;++m;}
  if(cx1<x1&&by>ty&&m<RG_MAX){out[m].x=cx1;out[m].y=ty;out[m].w=x1-cx1;out[m].h=by-ty;++m;}
 }
 return m;
}

static void dmg_chrome(void){dmg_add(0,0,360,300);}
static void dmg_tasks_area(void){dmg_add(0,0,640,19);dmg_add(TASKS_DROP_X,19,TASKS_DROP_W,230);}
static void handle(const gui_message_t *);
static void close_window(int i);

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

/* ---- menu z EmuTOS (Desk / File / View / Options) ----
 * Dane celowo bez wskaznikow: aplikacja jest linkowana PC-relative i tablica
 * wskaznikow w .rodata nie przetrwalaby zaladowania. Uzywamy indeksow. */
enum {ACT_NONE=0,ACT_INFO,ACT_CLOSE,ACT_CYCLE,ACT_SHELL,ACT_QUIT,ACT_DEVICES};
typedef struct {const char text[28];int8_t action;} menu_item_t;
typedef struct {const char title[10];uint8_t first,count;} menu_t;

static const menu_item_t menu_items[]={
    {"Desktop info...",ACT_INFO},
    {"Open",ACT_NONE},{"Info/Rename...",ACT_NONE},{"Search...",ACT_NONE},
    {"New folder...",ACT_NONE},{"Close folder",ACT_NONE},
    {"Close top window",ACT_CLOSE},{"Cycle windows",ACT_CYCLE},
    {"Select all items",ACT_NONE},{"Set file mask...",ACT_NONE},
    {"",ACT_NONE},
    {"Delete...",ACT_NONE},{"Format floppy...",ACT_NONE},
    {"",ACT_NONE},
    {"Execute EmuCON",ACT_SHELL},{"Shutdown",ACT_QUIT},
    {"Show as icons",ACT_NONE},{"Show as text",ACT_NONE},
    {"",ACT_NONE},
    {"Sort by name",ACT_NONE},{"Sort by type",ACT_NONE},{"Sort by size",ACT_NONE},
    {"Sort by date",ACT_NONE},{"No sort",ACT_NONE},
    {"",ACT_NONE},
    {"Size to fit",ACT_NONE},
    {"Set background...",ACT_NONE},
    {"",ACT_NONE},
    {"Install icon...",ACT_NONE},{"Install application...",ACT_NONE},
    {"Install devices",ACT_DEVICES},
    {"",ACT_NONE},
    {"Desktop configuration...",ACT_NONE}
};
static const menu_t menus[MENU_COUNT]={
    {"Desk",0,1},
    {"File",1,15},
    {"View",16,10},
    {"Options",26,7}
};
static const int16_t menu_x[MENU_COUNT]={8,56,104,152};
static const int16_t menu_w[MENU_COUNT]={48,48,48,72};

static int reliable(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){unsigned tries=0;int32_t r;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)return r;pause_tick();}while(++tries<200);return -1;}
static int32_t rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){gui_message_t m;
 if(reliable(pid,op,a,b,c))return -1;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];handle(&m);}}
static void add(unsigned kind,int x,int y,int w,int h,unsigned color,const char *text){gui_draw_t *c;
 if(used>=GUI_DRAW_MAX){fault=1;return;}c=&list[used++];zero(c,sizeof(*c));
 c->kind=(uint16_t)kind;c->x=(int16_t)x;c->y=(int16_t)y;c->w=(uint16_t)w;c->h=(uint16_t)h;
 c->color=(uint16_t)color;c->background=1;
 if(text)copytext(c->text,text,sizeof(c->text));}
static void text(int x,int y,const char *s){add(GUI_TEXT,x,y,0,0,0,s);}
static int submit(unsigned count){return rpc(gfx,18,(uint32_t)(uintptr_t)list,count,0);}

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
  text(r.x+150,r.y+36,num);text(r.x+16,r.y+60,"Menu: Desk/File/View/Options");
  text(r.x+16,r.y+84,"TASKS (prawy gorny rog) = lista.");}
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

static const menu_item_t *mitem(int mi,int ii){return &menu_items[(unsigned)menus[mi].first+(unsigned)ii];}
static void drop_size(int mi,int *w,int *h){
 const menu_t *m=&menus[mi];unsigned i;int maxw=0;*h=4;
 for(i=0;i<m->count;++i){
  const menu_item_t *it=mitem(mi,(int)i);
  if(!it->text[0]){*h+=7;continue;}
  {unsigned n=0;while(it->text[n])++n;if((int)n>maxw)maxw=(int)n;}
  *h+=17;
 }
 *w=maxw*8+16;if(*w<90)*w=90;
}
static int item_at(int mi,int mx,int my){
 const menu_t *m=&menus[mi];int dx=menu_x[mi],dy=MENUBAR_H,y=dy+2,w=0,h=0;unsigned i;
 drop_size(mi,&w,&h);
 if(mx<dx||mx>=dx+w||my<dy||my>=dy+h)return -1;
 for(i=0;i<m->count;++i){
  const menu_item_t *it=mitem(mi,(int)i);
  int ih=it->text[0]?17:7;
  if(my>=y&&my<y+ih)return it->text[0]?(int)i:-1;
  y+=ih;
 }
 return -1;
}
static int first_item(int mi){
 const menu_t *m=&menus[mi];unsigned i;
 for(i=0;i<m->count;++i)if(mitem(mi,(int)i)->text[0])return (int)i;
 return -1;
}
static int step_item(int mi,int from,int dir){
 const menu_t *m=&menus[mi];int i=from,n;
 for(n=0;n<(int)m->count;++n){
  i=(i+dir+(int)m->count)%(int)m->count;
  if(mitem(mi,i)->text[0])return i;
 }
 return from;
}

static void info_dialog_render(void){
 add(GUI_FILL,140,110,360,150,1,0);
 add(GUI_FRAME,140,110,360,150,0,0);
 text(160,118,"DESKTOP INFO");
 add(GUI_FRAME,140,140,360,1,0,0);
 text(160,160,"MonoDesk - ASF GUI core");
 text(160,180,"Okna otwarte:");
 {char n[12];number(n,(uint32_t)taskbar_count());text(260,180,n);}
 text(160,200,"Menedzer: N okien, Z-order, fokus");
 text(160,220,"Menu: EmuTOS Desk/File/View/Options");
 text(160,240,"Nacisnij klawisz / kliknij: zamknij");
}
static void execute(int mi,int ii){
 int action=mitem(mi,ii)->action;
 status=0;
 switch(action){
 case ACT_INFO: dialog=1;break;
 case ACT_CLOSE: if(state.active>=0)close_window(state.active);break;
 case ACT_CYCLE: wm_cycle(&state);break;
 case ACT_SHELL: status="EMUCON: NIE ZAIMPLEMENTOWANO";break;
 case ACT_QUIT: quitting=1;break;
 case ACT_DEVICES: status="INSTALL DEVICES: NIE ZAIMPLEMENTOWANO";break;
 default: status="NIE ZAIMPLEMENTOWANO";break;
 }
 dirty=1;
}

static void render_menu_bar(void){
 int i;
 for(i=0;i<MENU_COUNT;++i){
  const char *t=menus[i].title;
  if(i==menu_open)add(GUI_FRAME,menu_x[i],1,(int16_t)(menu_w[i]-2),17,0,0);
  text(menu_x[i]+4,2,t);
 }
 if(tasks_open)add(GUI_FRAME,TASKS_X,1,TASKS_W-2,17,0,0);
 text(TASKS_X+6,2,"TASKS");
}
static void render_dropdown(int mi){
 const menu_t *m=&menus[mi];int dx=menu_x[mi],dy=MENUBAR_H,y=dy+2,w=0,h=0;unsigned i;
 drop_size(mi,&w,&h);
 if(dx+w>640)dx=640-w;
 add(GUI_FILL,dx,dy,w,h,1,0);
 add(GUI_FRAME,dx,dy,w,h,0,0);
 for(i=0;i<m->count;++i){
  const menu_item_t *it=mitem(mi,(int)i);
  if(!it->text[0]){add(GUI_FRAME,dx+2,y+3,w-4,1,0,0);y+=7;continue;}
  if((int)i==menu_highlight)add(GUI_FRAME,dx+1,y,w-2,17,0,0);
  text(dx+6,y+1,it->text);
  y+=17;
 }
}
static void render_tasks(void){
 int n=taskbar_count(),k,y=MENUBAR_H+2;
 add(GUI_FILL,TASKS_DROP_X,MENUBAR_H,TASKS_DROP_W,4+n*TASKS_ITEM_H,1,0);
 add(GUI_FRAME,TASKS_DROP_X,MENUBAR_H,TASKS_DROP_W,4+n*TASKS_ITEM_H,0,0);
 for(k=0;k<n;++k){
  int i=taskbar_slot(k);char label[24];unsigned j,b=0;
  if(i<0)continue;
  label[b++]=(char)('1'+k);label[b++]=' ';
  {const char *nm=kind_name(state.win[i].kind);for(j=0;nm[j]&&b<20;++j)label[b++]=nm[j];}
  if(i==state.active){label[b++]=' ';label[b++]='*';}
  label[b]=0;
  if(i==state.active)add(GUI_FRAME,TASKS_DROP_X+1,y,TASKS_DROP_W-2,TASKS_ITEM_H,0,0);
  text(TASKS_DROP_X+6,y+1,label);
  y+=TASKS_ITEM_H;
 }
 if(!n)text(TASKS_DROP_X+6,MENUBAR_H+3,"BRAK PROGRAMOW");
}
static void render(void){
 int k,maxs;char label[24];
 used=0;
 if(dmg_valid)add(GUI_CLIP,dmg_x,dmg_y,dmg_w,dmg_h,0,0);
 else add(GUI_CLIP,0,0,640,400,0,0);
 dmg_valid=0;
 if(dirty>=2){
  /* XaAES: rysujemy zmienione okno tylko w jego WIDOCZNYM regionie, wiec nie
     zamalowujemy okien wyzej i nie musimy ich przerysowywac. */
  int i=(int)dirty-2;int p=-1;dirty=0;
  if(wm_is_open(&state,i)){
   wm_rect wr=wm_bounds(&state,i);
   rr_t cur[RG_MAX],tmp[RG_MAX];int n=1,m,k2;
   cur[0].x=wr.x;cur[0].y=wr.y;cur[0].w=wr.w;cur[0].h=wr.h;
   for(k=0;k<state.count;++k)if(state.order[k]==i){p=k;break;}
   if(p>=0)for(k=p+1;k<state.count&&n>0;++k){
    int j=state.order[k];
    if(!state.win[j].open)continue;
    {wm_rect o=wm_bounds(&state,j);rr_t c;c.x=o.x;c.y=o.y;c.w=o.w;c.h=o.h;
     m=rg_sub(cur,n,&c,tmp);for(k2=0;k2<m;++k2)cur[k2]=tmp[k2];n=m;}
   }
   for(k=0;k<n;++k){add(GUI_CLIP,cur[k].x,cur[k].y,cur[k].w,cur[k].h,0,0);content(i);}
  }
  if(submit(used))fault=18;
  return;}
 if(state.full>=0){int f=state.full;
  add(GUI_FILL,0,0,640,400,1,0);
  text(8,1,"F10: powrot do okien");
  content(f);dirty=0;
  if(submit(used))fault=18;
  return;}
 add(GUI_BG,0,0,640,400,0,0);
 add(GUI_FILL,0,0,640,MENUBAR_H,1,0);add(GUI_FRAME,0,0,640,MENUBAR_H,0,0);
 render_menu_bar();
 for(k=0;k<state.count;++k){int i=state.order[k];if(state.win[i].open)window(i);}
 if(menu_open>=0)render_dropdown(menu_open);
 if(tasks_open)render_tasks();
 if(dialog)info_dialog_render();
 add(GUI_FILL,0,377,640,23,1,0);add(GUI_FRAME,0,377,640,23,0,0);
 maxs=taskbar_count();if(maxs>6)maxs=6;
 for(k=0;k<maxs;++k){
  int i=taskbar_slot(k);unsigned j,b=0;
  if(i<0)continue;
  label[b++]=(char)('1'+k);label[b++]=' ';
  {const char *nm=kind_name(state.win[i].kind);for(j=0;nm[j]&&b<20;++j)label[b++]=nm[j];}
  label[b]=0;
  text(8+k*TASKBAR_SLOT_W,381,label);
  if(i==state.active)add(GUI_FRAME,6+k*TASKBAR_SLOT_W,379,TASKBAR_SLOT_W-4,19,0,0);
 }
 if(status)text(8,381+0,status);
 dirty=0;if(submit(used))fault=18;}

static void close_window(int i){
 if(!wm_is_open(&state,i))return;
 if(state.win[i].kind!=3&&state.win[i].pid)if(reliable(state.win[i].pid,WM_QUIT,0,0,0))fault=1;
 wm_close(&state,i);dirty=1;}

static void key(const gui_message_t *m){unsigned packed=m->v[1],scan=packed&127,mods=(packed>>16)&255;
 if((packed>>24)!=1)return;
 if(dialog){dialog=0;dirty=1;return;}
 if(menu_open>=0){
  if(scan==1){menu_open=-1;dmg_chrome();dirty=1;return;}
  if(scan==0x48||scan==0x50){menu_highlight=step_item(menu_open,menu_highlight,scan==0x48?-1:1);dmg_chrome();dirty=1;return;}
  if(scan==0x4b||scan==0x4d){menu_open=(menu_open+(scan==0x4d?1:MENU_COUNT-1))%MENU_COUNT;menu_highlight=first_item(menu_open);dmg_chrome();dirty=1;return;}
  if(scan==0x1c){if(menu_highlight>=0)execute(menu_open,menu_highlight);menu_open=-1;dmg_chrome();dirty=1;return;}
 }
 if(scan==1){quitting=1;return;}
 if(tasks_open&&(scan==0x48||scan==0x50)){
  int n=taskbar_count();if(n){int cur=state.active,k;
   for(k=0;k<n;++k)if(taskbar_slot(k)==cur)break;
   k=(k+(scan==0x48?n-1:1))%n;wm_select(&state,taskbar_slot(k));dirty=1;}
  return;}
 if(scan>=0x3b&&scan<=0x3e){menu_open=(int)(scan-0x3b);menu_highlight=first_item(menu_open);tasks_open=0;status=0;dmg_chrome();dirty=1;return;}
 if(scan==0x3f){tasks_open=!tasks_open;menu_open=-1;dmg_tasks_area();dirty=1;return;}
 if(scan==0x0f&&(mods&8)){wm_cycle(&state);dirty=1;return;}
 if(scan==0x44){if(state.active>=0){state.full=state.full<0?state.active:-1;state.gesture=0;dirty=1;}return;}
 if(scan==0x3e&&(mods&8)){close_window(state.active);return;}
 if(state.active>=0&&state.win[state.active].open&&state.win[state.active].kind!=3)
  if(reliable(state.win[state.active].pid,WM_INPUT,packed,m->v[2],0))fault=1;
}
static void mouse(const gui_message_t *m){int x=m->v[1]>>16,y=m->v[1]&65535,buttons=(m->v[2]>>24)&7;
 int down=buttons&3,press=down&&!state.buttons;wm_rect r;int i;
 if(!down&&(buttons&4)&&!state.buttons&&!state.gesture)press=1;
 if(dialog){if(press){dialog=0;dirty=1;}state.buttons=down;return;}
 if(y<MENUBAR_H){
  if(press){
   if(x>=TASKS_X){tasks_open=!tasks_open;menu_open=-1;}
   else{int mi=-1,k;for(k=0;k<MENU_COUNT;++k)if(x>=menu_x[k]&&x<menu_x[k]+menu_w[k])mi=k;
    if(mi>=0){if(menu_open==mi)menu_open=-1;else{menu_open=mi;menu_highlight=first_item(mi);}tasks_open=0;}}
   dmg_chrome();dirty=1;
  } else {
   int mi=-1,k;for(k=0;k<MENU_COUNT;++k)if(x>=menu_x[k]&&x<menu_x[k]+menu_w[k])mi=k;
   if(mi>=0&&menu_open>=0&&mi!=menu_open){menu_open=mi;menu_highlight=first_item(mi);dmg_chrome();dirty=1;}
  }
  state.buttons=down;return;
 }
 if(menu_open>=0){
  int it=item_at(menu_open,x,y);
  if(it>=0){if(press){execute(menu_open,it);menu_open=-1;}else if(it!=menu_highlight){menu_highlight=it;dirty=1;}}
  else if(press){menu_open=-1;}
  dmg_chrome();dirty=1;
  state.buttons=down;return;
 }
 if(tasks_open){
  int n=taskbar_count(),k,yy=MENUBAR_H+2,hit=-1;
  for(k=0;k<n;++k){if(x>=TASKS_DROP_X&&x<TASKS_DROP_X+TASKS_DROP_W&&y>=yy&&y<yy+TASKS_ITEM_H){hit=taskbar_slot(k);break;}yy+=TASKS_ITEM_H;}
  if(hit>=0&&press){wm_select(&state,hit);tasks_open=0;dirty=1;}
  else if(press){tasks_open=0;dirty=1;}
  state.buttons=down;return;
 }
 if(state.gesture){wm_rect old=wm_bounds(&state,state.active);wm_motion(&state,x,y);
  dmg_add(old.x,old.y,old.w,old.h);{wm_rect nw=wm_bounds(&state,state.active);dmg_add(nw.x,nw.y,nw.w,nw.h);}
  dirty=1;if(!down)state.gesture=0;state.buttons=down;return;}
 state.buttons=down;if(!press)return;
 if(state.full>=0)return;
 if(y>=377){int k=(x-6)/TASKBAR_SLOT_W;i=taskbar_slot(k);if(i>=0){wm_select(&state,i);dirty=1;}return;}
 i=wm_hit(&state,x,y);if(i<0)return;wm_select(&state,i);dirty=1;r=wm_bounds(&state,i);
 if(y<r.y+19){
  if(x<r.x+18){close_window(i);return;}
  if(down){state.gesture=1;state.dx=x-r.x;state.dy=y-r.y;}
 }
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
 if(state.win[i].kind==1&&m->v[0]==WM_COUNT){state.win[i].count=m->v[1];state.win[i].paused=m->v[2]!=0;mark_window(i);}
 if(state.win[i].kind==2&&m->v[0]>=WM_TEXT0&&m->v[0]<=WM_TEXT2){unsigned j=m->v[0]-WM_TEXT0,k;
  if(!j)state.win[i].parts=0;
  for(k=0;k<12;k++)state.win[i].staging[j*12+k]=((const char *)&m->v[1])[k];
  state.win[i].parts|=(uint8_t)(1u<<j);
  if(state.win[i].parts==7){for(k=0;k<35;k++)state.win[i].text[k]=state.win[i].staging[k];state.win[i].text[35]=0;mark_window(i);}}
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
 wm_open(&state,INTERNAL_PID,"SYSTEM");state.win[slot_of(INTERNAL_PID)].kind=3;
 if(!launch(0,"\\SYSTEM\\APP\\COUNT.APP")){fault=34;goto done;}
 if(!launch(1,"\\SYSTEM\\APP\\NOTE.APP")){fault=35;goto done;}
 dirty=1;
 while(!quitting&&!fault){for(i=0;i<8&&receive(&m,0)==0;i++)handle(&m);reap();if(dirty)render();pause_tick();}
done:
 for(i=0;i<WM_MAX;++i)if(state.win[i].open&&state.win[i].kind!=3&&state.win[i].pid)
  reliable(state.win[i].pid,WM_QUIT,0,0,0);
 start=(uint32_t)ticks();while(any_pid()&&(uint32_t)ticks()-start<100){while(receive(&m,0)==0){}reap();pause_tick();}
 if(any_pid()&&!fault)fault=32;
 if(captured)rpc(gui,7,0,0,0);
 if(registered)rpc(gui,2,0,0,0);
 gui_call(1,0,0,fault?"MONODESK: FAILED\n":"MONODESK: MENU + TASKS / EXIT OK\n",0);
 if(fault==34||fault==35)return launch_error;
 return fault;
}
