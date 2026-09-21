/* Native Mono Commander. Original implementation, inspired by MC's two-panel
 * interaction model; no Unix/GLib/terminal code is imported. */
#include "gui_app.h"
#include "kernel_fat.h"
#include "text_terminal.h"
#define ROWS 16u
typedef struct {char name[13];uint8_t attr,pad[2];uint32_t size;} entry;
typedef struct {char path[64];char focus[13];entry item[ROWS+1];unsigned top,sel,count,more;} panel;
static entry scanbuf[32];
static panel panels[2];
static text_terminal_t terminal;
static uint32_t gfx,gui;
static unsigned active,used,quit,dirty,modal,length;
static int screen_captured;
static unsigned panel_damage,painted_valid;
static char status[64],input[13],source[64],target[64];
static int fault;
static unsigned painted_active,painted_sel[2],mouse_buttons;
static unsigned last_click_panel,last_click_index,click_valid;
static uint32_t last_click_tick;
static void selection_dirty(void){if(!dirty||dirty==3)dirty=3;}

static gui_message_t pending[16];static unsigned head,tail;
static unsigned len(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void message(const char *s){copytext(status,s,sizeof(status));dirty=1;}
static void queue_input(const gui_message_t *m){unsigned j;gui_message_t *d;
 if(m->sender||(m->v[0]!=0x4b4b4559&&m->v[0]!=0x4b4d4f55))return;
 /* Adjacent mouse motion is state. Preserve clicks while replacing its position. */
 if(m->v[0]==0x4b4d4f55&&tail!=head){d=&pending[(tail-1)&15];
  if(d->v[0]==0x4b4d4f55&&!(m->v[2]&0x1f000000u)&&!(d->v[2]&0x1f000000u)){
   for(j=0;j<sizeof(*m);j++)((volatile uint8_t *)d)[j]=((const uint8_t *)m)[j];
   return;}}
 if(tail-head>=16)return;
 d=&pending[tail++&15];for(j=0;j<sizeof(*m);j++)((volatile uint8_t *)d)[j]=((const uint8_t *)m)[j];
}
static int rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){gui_message_t m;unsigned tries=0;int r;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)break;pause_tick();}while(++tries<100);
 if(r)return r;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];queue_input(&m);}
}
static int join(char *out,const char *dir,const char *name){unsigned n=len(dir),j=0;
 if(n+len(name)+2>64)return 0;
 copytext(out,dir,64);if(n>1)out[n++]='\\';
 while(name[j])out[n++]=name[j++];
 out[n]=0;return 1;}
static unsigned find_index(const char *path,const char *name)
{
    unsigned base=0,pages=0;
    while(pages++<8u){
        int r=gui_call(52,base,32,(char *)path,scanbuf);unsigned i;
        if(r<=0)break;
        for(i=0;i<(unsigned)r;++i){const char *a=scanbuf[i].name,*b=name;unsigned k=0;
            while(a[k]&&a[k]==b[k])++k;
            if(!a[k]&&!b[k])return base+i;}
        if((unsigned)r<32u)break;
        base+=(unsigned)r;
    }
    return 0xffffffffu;
}
static void load(panel *p){unsigned parent=p->path[1]!=0,skip,first=0;int r;
 if(p->focus[0]){unsigned idx=find_index(p->path,p->focus);
     if(idx!=0xffffffffu)p->top=(idx>=ROWS)?(idx-ROWS+1u):0u;}
 skip=p->top;
 p->count=p->more=0;
 if(parent&&!skip){zero(&p->item[0],sizeof(entry));copytext(p->item[0].name,"..",13);p->item[0].attr=16;first=1;}
 else if(parent)skip--;
 r=gui_call(52,skip,ROWS+1-first,p->path,&p->item[first]);
 if(r<0)message("Blad odczytu katalogu");
 else {p->count=(unsigned)r+first;p->more=p->count>ROWS;if(p->more)p->count=ROWS;}
 if(p->focus[0]){unsigned j;for(j=0;j<p->count;++j){const char *a=p->item[j].name,*b=p->focus;unsigned k=0;
    while(a[k]&&a[k]==b[k])++k;
    if(!a[k]&&!b[k]){p->sel=j;break;}}
    p->focus[0]=0;}
 if(!p->count&&p->top){p->top=0;p->sel=0;load(p);return;}
 if(p->sel>=p->count)p->sel=p->count?p->count-1:0;
 click_valid=0;panel_damage|=1u<<(p==&panels[1]);if(dirty!=1&&dirty!=2)dirty=4;
}
static void refresh(void){load(&panels[0]);load(&panels[1]);}
/* All layout is expressed in character cells. The display server compares
 * the submitted grid and rasterizes only changed character pairs. */
static void path_row(unsigned p){panel *q=&panels[p];unsigned n=len(q->path),x=p*40+1;
 term_fill(&terminal,x,2,38,1,' ',p==active?TERM_INVERSE:0);
 term_text(&terminal,x,2,n>38?q->path+n-38:q->path,p==active?TERM_INVERSE:0,38);
}
static void row(unsigned p,unsigned i,unsigned selected){panel *q=&panels[p];unsigned j,n;char line[39],num[12];unsigned x=p*40+1;
 if(i>=ROWS)return;
 for(j=0;j<38;j++)line[j]=' ';
 line[38]=0;
 if(i<q->count){line[0]=(q->item[i].attr&16)?'/':' ';
  for(j=0;q->item[i].name[j]&&j<12;j++)line[j+1]=q->item[i].name[j];
  if(q->item[i].attr&16)copytext(num,"<KATALOG>",12);else number(num,q->item[i].size);
  n=len(num);for(j=0;j<n;j++)line[37-n+j]=num[j];
 }
 term_text(&terminal,x,4+i,line,selected?TERM_INVERSE:0,38);
}
static void panel_view(unsigned p){unsigned i,x=p*40+1;
 term_fill(&terminal,x,2,38,19,' ',0);path_row(p);
 term_text(&terminal,x,3,"Nazwa                         Rozmiar",0,38);
 for(i=0;i<panels[p].count;i++)row(p,i,p==active&&i==panels[p].sel);
 term_text(&terminal,x,20,"^ PgUp                         PgDn v",0,38);
}
static void render(void){unsigned p,i,n;int r;
 if(!painted_valid)dirty=1;
 if(dirty==4&&!modal){
  for(p=0;p<2;p++){
   if(panel_damage&(1u<<p))panel_view(p);
   else if(painted_active!=active||painted_sel[p]!=panels[p].sel){
    row(p,painted_sel[p],0);row(p,panels[p].sel,p==active);
   }
   if(painted_active!=active)path_row(p);
  }
 }else if(dirty==3&&!modal){
  row(painted_active,painted_sel[painted_active],0);row(active,panels[active].sel,1);
  if(painted_active!=active){path_row(0);path_row(1);}
 }else if(!(dirty==2&&modal)){
  term_clear(&terminal,0);
  term_text(&terminal,1,0,"TEXT COMMANDER",0,38);
  term_text(&terminal,41,0,"F6 Przywroc    F1 Pomoc",0,38);
  for(p=0;p<2;p++){term_frame(&terminal,p*40,1,40,21,0);panel_view(p);}
  term_text(&terminal,1,22,"Klik: wybor   2x: katalog",0,38);
  term_text(&terminal,41,22,"Prawy: wyzej   Tab: panel",0,38);
  term_text(&terminal,1,23,status,0,78);
  for(i=0;i<4;i++)term_fill(&terminal,i*20,24,19,1,' ',TERM_INVERSE);
  term_text(&terminal,1,24,"F5 Kopiuj",TERM_INVERSE,18);
  term_text(&terminal,21,24,"F7 Katalog",TERM_INVERSE,18);
  term_text(&terminal,41,24,"F8 Kosz/Usun",TERM_INVERSE,18);
  term_text(&terminal,61,24,"F10 Koniec",TERM_INVERSE,18);
 }
 if(modal){term_fill(&terminal,12,8,56,9,' ',0);term_frame(&terminal,12,8,56,9,0);
  term_text(&terminal,14,9,modal==7?"Nowy katalog (nazwa 8.3)":modal==5?"Kopiowanie do drugiego panelu":modal==6?"Przywroc do drugiego panelu":modal==8?"Przenies do kosza TRASH?":"TRWALE usun wraz z zawartoscia?",0,52);
  if(modal==7)term_text(&terminal,14,11,input,0,52);
  else{n=len(source);term_text(&terminal,14,11,n>52?source+n-52:source,0,52);
   if(modal==5||modal==6){n=len(target);term_text(&terminal,14,12,n>52?target+n-52:target,0,52);}}
  term_fill(&terminal,14,15,22,1,' ',TERM_INVERSE);term_fill(&terminal,40,15,26,1,' ',TERM_INVERSE);
  term_text(&terminal,15,15,modal==7?"Enter: Utworz":"Y: Tak",TERM_INVERSE,20);
  term_text(&terminal,41,15,"Esc: Anuluj",TERM_INVERSE,24);
 }
 painted_active=active;painted_sel[0]=panels[0].sel;painted_sel[1]=panels[1].sel;
 dirty=0;panel_damage=0;painted_valid=1;
 r=rpc(gfx,19,(uint32_t)(uintptr_t)terminal.cells,TERM_CELLS,0);
 if(r<0)fault=19;else used=(unsigned)r;
}
static void result(int r){
 if(!r)message("Gotowe.");
 else if(r==-FAT_WRITE_EXISTS)message("Cel juz istnieje - bez nadpisania.");
 else if(r==-FAT_WRITE_FULL)message("Brak miejsca na dysku / w katalogu.");
 else if(r==-FAT_WRITE_NOT_EMPTY)message("Katalog nie jest pusty.");
 else if(r==-FAT_WRITE_READ_ONLY)message("Tylko odczyt / dysk wymaga kontroli.");
 else if(r==-FAT_WRITE_LIMIT)message("Przekroczony limit operacji.");
 else if(r==-FAT_WRITE_IO)message("Blad zapisu / odczytu dysku.");
 else message("Nieprawidlowa nazwa lub operacja.");
 refresh();
}
static void run_program(const char *path)
{
    gui_spawn_t request;uint32_t result[2];gui_message_t mm;unsigned guard;
    zero(&request,sizeof(request));
    copytext(request.path,path,sizeof(request.path));
    message("Uruchamiam...");render();
    if(screen_captured)rpc(gui,7,0,0,0);
    if(gui_call(34,0,0,&request,0)){message("Nie udalo sie uruchomic.");}
    else{
        for(guard=0;guard<6000u;++guard){
            result[0]=0;
            if(gui_call(32,request.pid,1,result,0)==0&&result[0]==5)break;
            while(receive(&mm,0)==0)queue_input(&mm);
            pause_tick();
        }
    }
    if(screen_captured)rpc(gui,6,0,0,0);
    refresh();render();
}
static int in_trash(const char *path){const char *t="\\TRASH";unsigned i;
 for(i=0;i<6;i++){char c=path[i];if(c>='a'&&c<='z')c-=32;if(c!=t[i])return 0;}
 return path[6]==0||path[6]=='\\';}
static void parent(panel *p){unsigned n=len(p->path);if(n<=1)return;
 {unsigned e=n,s;while(e>1&&p->path[e-1]=='\\')--e;
  s=e;
  while(s>1&&p->path[s-1]!='\\')--s;
  {unsigned j=0;while(s+j<e&&j<12){p->focus[j]=p->path[s+j];++j;}p->focus[j]=0;}}
 while(n>1&&p->path[n-1]!='\\')n--;
 if(n>1)n--;
 p->path[n]=0;p->top=p->sel=0;load(p);}
static void key(const gui_message_t *m){unsigned scan=m->v[1]&127;char c=(char)m->v[2];panel *p=&panels[active];entry *e;
 if((m->v[1]>>24)!=1)return;
 if(modal){dirty=2;
 if(scan==1||(modal!=7&&(c=='n'||c=='N'))){modal=0;message("Anulowano.");return;}
 if(modal==7){if(scan==14){if(length)input[--length]=0;}
 else if(scan==28&&length){if(join(target,p->path,input)){modal=0;message("Tworzenie katalogu...");render();result(gui_call(50,0,0,target,0));}else message("Sciezka za dluga.");}
 else if(length<12&&((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')){input[length++]=c;input[length]=0;}}
 else if(c=='y'||c=='Y'){unsigned op=modal;modal=0;message("Operacja dyskowa...");render();result(gui_call(op==5?49:op==6?54:op==8?53:55,0,0,source,(op==5||op==6)?target:0));}
 return;}
 if(scan==0x44||scan==1){quit=1;return;}
 if(scan==0x3b){message("F8: kosz / usun. F6: przywroc z kosza.");return;}
 if(scan==15){active^=1;click_valid=0;selection_dirty();return;}
 if(scan==14){parent(p);return;}
 if(scan==0x48){if(p->sel)p->sel--;else if(p->top){p->top--;load(p);}click_valid=0;selection_dirty();return;}
 if(scan==0x50){if(p->sel+1<p->count)p->sel++;else if(p->more){p->top++;load(p);}click_valid=0;selection_dirty();return;}
 if(scan==0x49){p->top=p->top>ROWS?p->top-ROWS:0;p->sel=0;load(p);return;}
 if(scan==0x51){if(p->more){p->top+=ROWS;p->sel=0;load(p);}return;}
 if(scan==0x47){p->top=p->sel=0;load(p);return;}
 if(scan==0x41){modal=7;length=0;input[0]=0;dirty=1;return;}
 if(!p->count)return;
 e=&p->item[p->sel];
 if(scan==28){                               /* Enter: katalog -> wejdz, plik -> uruchom */
     if(e->name[0]=='.'){parent(p);return;}
     if(e->attr&16){if(join(source,p->path,e->name)){copytext(p->path,source,64);p->top=p->sel=0;p->focus[0]=0;load(p);}return;}
     if(join(source,p->path,e->name))run_program(source);
     return;}
 if(scan!=0x3f&&scan!=0x40&&scan!=0x42)return;
 if(scan==0x40&&(!in_trash(p->path)||in_trash(panels[active^1].path))){message("F6: z kosza do drugiego panelu.");return;}
 if(e->name[0]=='.'){message("Wybierz plik lub katalog.");return;}
 if(scan==0x3f&&(e->attr&16)){message("F5: kopiowanie pojedynczych plikow.");return;}
 if(!join(source,p->path,e->name)||!join(target,panels[active^1].path,e->name)){message("Sciezka za dluga.");return;}
 if(scan==0x42&&in_trash(source)&&len(source)==6){message("Nie mozna usunac katalogu TRASH.");return;}
 modal=scan==0x3f?5:scan==0x40?6:in_trash(p->path)?9:8;dirty=1;
}
static void action(unsigned scan,unsigned character){gui_message_t m;zero(&m,sizeof(m));m.v[1]=0x01000000u|scan;m.v[2]=character;key(&m);}
static void mouse(const gui_message_t *m){unsigned buttons=(m->v[2]>>24)&31,down=buttons&3;
 unsigned press=(down&~mouse_buttons);int x=(int)(m->v[1]>>16),y=(int)(m->v[1]&65535);
 unsigned p,index;uint32_t now;
 if(!down&&!mouse_buttons){press=(buttons>>3)&3;if(!press&&(buttons&4))press=2;}
 mouse_buttons=down;
 if(!press||x<0||x>=640||y<0||y>=400)return;
 if(modal){click_valid=0;if(!(press&2))return;
  if(y>=240&&y<256){if(x>=112&&x<288)action(modal==7?28:0,modal==7?0:'y');else if(x>=320&&x<528)action(1,0);}return;}
 if(y>=384){click_valid=0;if(press&2){if(x<160)action(0x3f,0);else if(x<320)action(0x41,0);else if(x<480)action(0x42,0);else action(0x44,0);}return;}
 if(y<16||y>=352)return;
 p=x>=320;if(x-(int)(p*320)<8||x-(int)(p*320)>=312)return;
 if(active!=p){active=p;selection_dirty();}
 if(press&1){click_valid=0;parent(&panels[p]);return;}
 if(!(press&2))return;
 if(y>=320&&y<336){click_valid=0;action((x-(int)(p*320))<160?0x49:0x51,0);return;}
 if(y<64||y>=320)return;
 index=(unsigned)(y-64)/16;if(index>=panels[p].count)return;
 now=(uint32_t)ticks();panels[p].sel=index;selection_dirty();
 if(click_valid&&last_click_panel==p&&last_click_index==panels[p].top+index&&now-last_click_tick<=20u){click_valid=0;action(28,0);}
 else{click_valid=1;last_click_panel=p;last_click_index=panels[p].top+index;last_click_tick=now;}
}
static void handle(const gui_message_t *m){if(m->sender)return;if(m->v[0]==0x4b4b4559)key(m);else if(m->v[0]==0x4b4d4f55)mouse(m);}
#include "../tests/commander_selftest.inc"
int app_main(void){gui_message_t m;int registered=0,captured=0;
 gfx=lookup("SYSTEM.GFX");gui=lookup("SYSTEM.GUI");if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(rpc(gui,1,0,0,0))return 2;
 registered=1;
 if(rpc(gui,6,0,0,0)){fault=3;goto done;}captured=1;screen_captured=1;
 if(rpc(gfx,13,0,0,0)||rpc(gfx,1,0,0,0)!=(int32_t)((640u<<16)|400u)){fault=4;goto done;}
 copytext(panels[0].path,"\\",64);copytext(panels[1].path,"\\SYSTEM\\APP",64);
 message("Terminal 80x25 - nazwy FAT 8.3");refresh();
#ifdef COMMANDER_SELFTEST
 commander_selftest();goto done;
#endif
 while(!quit&&!fault){unsigned n=0;int got;
  do{if(head!=tail){unsigned j;gui_message_t *d=&pending[head++&15];for(j=0;j<sizeof(m);j++)((volatile uint8_t *)&m)[j]=((uint8_t *)d)[j];got=1;}
  else got=receive(&m,0)==0;
  if(got)handle(&m);
  }while(got&&++n<16&&!quit&&!fault);
  if(quit||fault)break;
  if(dirty)render();
  if(head==tail){if(receive(&m,1))break;handle(&m);}
 }

 done:if(captured)rpc(gui,7,0,0,0);if(registered)rpc(gui,2,0,0,0);
#ifdef COMMANDER_SELFTEST
 {char n[12];gui_call(1,0,0,fault?"COMTEST: FAIL\n":"COMTEST: MOUSE/PAGE/DRAW PASS\n",0);
 gui_call(1,0,0,"PAGE TICKS: ",0);number(n,bench_page);gui_call(1,0,0,n,0);
 gui_call(1,0,0," / LEGACY TICKS: ",0);number(n,bench_single);gui_call(1,0,0,n,0);gui_call(1,0,0,"\n",0);}
#endif
 return fault;
}
