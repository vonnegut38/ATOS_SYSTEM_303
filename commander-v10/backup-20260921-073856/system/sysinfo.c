/* Original 80x25 system inspector; DOS-style layout, native kernel API. */
#include "gui_app.h"
#include "text_terminal.h"
#include "system_report.h"
static text_terminal_t screen;
static system_report_t info;
static uint32_t gfx,gui,page,disk_page,dirty=1,quit,fault,mouse_down;
static gui_message_t pending[16];static unsigned head,tail;
static char status[79]="F1 Konfiguracja  F2 Dysk  F3 Benchmarki";
static volatile uint32_t *ram;
static uint8_t disk_buffer[4096];
static uint32_t bench,all_tests,start,last_paint,amount,seed=0x13579bdf,offset;
/* state: 0 untested, 1 complete, 2 unavailable, 3 error, 4 cancelled. */
typedef struct {uint32_t amount,ticks,state;} bench_result;
static bench_result results[4];
/* Avoid libgcc's non-PIC remainder helper in relocatable native apps. */
static uint32_t quotient(uint32_t n,uint32_t d)
{uint32_t q=0,r=0;unsigned i;if(!d)return 0;for(i=0;i<32;i++){uint32_t carry=r>>31;r=(r<<1)|(n>>31);n<<=1;q<<=1;if(carry||r>=d){r-=d;q|=1;}}return q;}
static uint32_t wall(void){return (uint32_t)gui_call(58,0,0,0,0);}
static unsigned length(const char *s){unsigned n=0;while(s[n])n++;return n;}
static void append(char *d,const char *s){unsigned n=length(d);copytext(d+n,s,79-n);}
static void addnum(char *d,uint32_t n){char t[12];number(t,n);append(d,t);}
static void text(unsigned x,unsigned y,const char *s,unsigned width){term_text(&screen,(int)x,(int)y,s,0,width);}
static void line(unsigned y,const char *label,const char *value){text(3,y,label,25);text(29,y,value,47);}
static void size_kib(char *s,uint32_t n){s[0]=0;addnum(s,n/1024);append(s," KiB");}
static void size_mib_sectors(char *s,uint32_t n){s[0]=0;addnum(s,n/2048);append(s,".");addnum(s,((n%2048)*10)/2048);append(s," MiB");}
static const char *cpu_name(uint32_t n){switch(n){case 0:return "Motorola 68000";case 1:return "Motorola 68010";case 2:return "Motorola 68020";case 3:return "Motorola 68030";case 4:return "Motorola 68040";case 5:return "Motorola 68060";default:return "nieznany";}}
static const char *machine_name(uint32_t n){switch(n){case 0:return "Atari ST";case 1:return "Atari Mega ST";case 2:return "Atari STE";case 3:return "Atari Mega STE";case 4:return "Atari TT";case 5:return "Atari Falcon";default:return "nieznany";}}
static const char *chip_name(uint32_t n){switch(n){case 0:return "brak wykrytej karty";case 1:return "ET4000 / rodzina W32";case 2:return "ATI Mach32";case 3:return "ATI Mach64";default:return "nieznana karta";}}
static const char *adapter_name(uint32_t n){switch(n){case 0:return "brak";case 1:return "Nova VME";case 2:return "CrazyDots VME";case 3:return "VOFA / Volksfarben";default:return "nieznany";}}
static int refresh(void){int r=gui_call(56,sizeof(info),0,&info,0);
 if(r||info.magic!=SYSREPORT_MAGIC||info.version!=SYSREPORT_VERSION||info.size!=sizeof(info)||info.volumes>SYSREPORT_VOLUMES)return -1;
 return 0;
}
/* Desktop busy (another graphics app owns the screen): print the report on
 * the console instead of refusing to start. */
static void console_report(void)
{
    char s[79];
    if(refresh()){gui_call(1,0,0,"SYSINFO: brak raportu systemowego (API 56).\n",0);return;}
    gui_call(1,0,0,"SYSINFO - tryb konsolowy (desktop zajety przez inna aplikacje)\n",0);
    s[0]=0;append(s,"  Komputer: ");append(s,machine_name(info.machine));append(s,"   CPU: ");append(s,cpu_name(info.cpu));append(s,"\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  ST-RAM: ");addnum(s,info.st_total/1024);append(s," KiB, wolne ");addnum(s,info.st_free/1024);append(s," KiB\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  TT-RAM: ");addnum(s,info.tt_total/1024);append(s," KiB, wolne ");addnum(s,info.tt_free/1024);append(s," KiB\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  Wyjscie: ");append(s,info.active_video==1?"VOFA + ET4000":info.active_video==2?"ATI Mach32":"Shifter");
    append(s,"   Rozdzielczosc: ");addnum(s,info.output_width);append(s," x ");addnum(s,info.output_height);append(s," / ");addnum(s,1u<<info.planes);append(s," kolorow\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  Karta (BIOS): ");append(s,info.hardware_report?chip_name(info.detected_chip):"brak danych");append(s," / ");append(s,info.hardware_report?adapter_name(info.detected_adapter):"nieustalony");append(s,"\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  WM: klatek ");addnum(s,info.wm_frames);append(s,"  pole uszkodzen ");addnum(s,info.wm_damage_pixels);append(s," px\n");gui_call(1,0,0,s,0);
    s[0]=0;append(s,"  Dysk: ");append(s,info.disk_name[0]?info.disk_name:"bez modelu");append(s,"  Cache: ");addnum(s,info.cache_sectors/2);append(s,info.cache_tt?" KiB TT-RAM":" KiB ST-RAM");append(s,"\n");gui_call(1,0,0,s,0);
}
static void enqueue(const gui_message_t *m){unsigned i;if(m->sender||tail-head>=16)return;
 if(m->v[0]!=0x4b4b4559&&m->v[0]!=0x4b4d4f55)return;
 for(i=0;i<sizeof(*m);i++)((volatile uint8_t *)&pending[tail&15])[i]=((const uint8_t *)m)[i];
 tail++;
}
static int rpc(uint32_t pid,uint32_t op,uint32_t a,uint32_t b,uint32_t c){gui_message_t m;unsigned tries=0;int r;
 do{r=send(pid,op,a,b,c);if(r!=-0x3c2)break;pause_tick();}while(++tries<100);
 if(r)return r;
 for(;;){if(receive(&m,1))return -1;if(m.sender==pid&&m.v[0]==(op|0x80000000u))return (int32_t)m.v[1];enqueue(&m);}
}
static void hardware_view(void){char s[79];
 line(4,"Komputer",machine_name(info.machine));line(5,"Procesor",cpu_name(info.cpu));
 s[0]=0;if(info.clock_source){addnum(s,info.clock_mhz);append(s,info.clock_source==2?" MHz (nominalnie; nie pomiar)":" MHz (dane BIOS, nie pomiar)");}else append(s,"nieustalone");line(6,"Taktowanie",s);
 line(7,"FPU",info.hardware_report?(info.fpu==2?"zintegrowany":info.fpu==1?"zewnetrzny":"nie wykryto"):"brak danych z tego BIOS-u");
 size_kib(s,info.st_total);append(s," / wolne ");addnum(s,info.st_free/1024);append(s," KiB");line(9,"ST-RAM razem / wolne",s);
 size_kib(s,info.tt_total);append(s," / wolne ");addnum(s,info.tt_free/1024);append(s," KiB");line(10,"TT-RAM razem / wolne",s);
 size_kib(s,info.st_largest);append(s," / TT ");addnum(s,info.tt_largest/1024);append(s," KiB");line(11,"Najwiekszy blok ST",s);
 line(13,"Aktywne wyjscie",info.active_video==1?"VOFA + ET4000":info.active_video==2?"ATI Mach32":"Shifter");
 s[0]=0;addnum(s,info.output_width);append(s," x ");addnum(s,info.output_height);append(s," / ");addnum(s,1u<<info.planes);append(s," kolory");line(14,"Rozdzielczosc wyjscia",s);
 s[0]=0;addnum(s,info.width);append(s," x ");addnum(s,info.height);append(s," / terminal 80 x 25");line(15,"Obszar aplikacji",s);
 line(17,"Karta wykryta przez BIOS",info.hardware_report?chip_name(info.detected_chip):"brak danych; uzyj BIOS-u v9");
 line(18,"Adapter karty",info.hardware_report?adapter_name(info.detected_adapter):"nieustalony");
 line(19,"Blitter",info.blitter&8?"aktywny":info.blitter&4?"obecny, wylaczony":"nie wykryto");
 {char s2[79];s2[0]=0;append(s2,"klatek ");addnum(s2,info.wm_frames);
  append(s2,"  pole uszkodzen ");addnum(s2,info.wm_damage_pixels);append(s2," px");
  line(20,"Kompozytor WM",s2);}
 text(3,21,"Wykryta karta nie oznacza, ze system jej teraz uzywa.",74);
}
static void disk_view(void){char s[79];unsigned i;
 line(4,"Model / INQUIRY",info.disk_name[0]?info.disk_name:"urzadzenie nie podalo modelu");
 s[0]=0;append(s,"ACSI / port DMA / ID ");addnum(s,info.device);line(5,"Podlaczenie",s);
 if(info.disk_capacity_known)size_mib_sectors(s,info.disk_sectors);else copytext(s,"nieustalona (READ CAPACITY niedostepne)",79);line(6,"Pojemnosc calego dysku",s);
 s[0]=0;if(info.disk_capacity_known){addnum(s,info.disk_sectors);append(s," sektorow po 512 B");}else append(s,"Rozmiary partycji ponizej sa znane.");line(7,"Adresowalny dysk",s);
 s[0]=0;addnum(s,info.cache_sectors/2);append(s,info.cache_tt?" KiB w TT-RAM":" KiB w ST-RAM");append(s," / write-through");line(8,"Cache systemowy",s);
 term_fill(&screen,3,10,74,1,' ',TERM_INVERSE);term_text(&screen,4,10,"Dysk  Typ   Start LBA       Pojemnosc         Startowy",TERM_INVERSE,72);
 for(i=0;i<8&&disk_page+i<info.volumes;i++){
  unsigned n=disk_page+i;char drive[3]={(char)('C'+n),':',0};
  text(4,12+i,drive,3);text(10,12+i,info.volume[n].id,3);
  s[0]=0;addnum(s,info.volume[n].start);text(16,12+i,s,12);
  size_mib_sectors(s,info.volume[n].sectors);text(32,12+i,s,19);
  text(53,12+i,info.volume[n].boot?"TAK":"-",8);
 }
 text(3,21,"PgUp/PgDn: partycje. Raport dotyczy dysku startowego ACSI.",74);
}
static void result_line(unsigned y,unsigned which,const char *label){char s[79];bench_result *r=&results[which];
 if(bench==which+1)copytext(s,"trwa...",79);
 else if(!r->state)copytext(s,"jeszcze nie wykonano",79);
 else if(r->state==2)copytext(s,"niedostepny / brak odpowiedniej pamieci",79);
 else if(r->state==3)copytext(s,"blad pomiaru lub odczytu",79);
 else if(r->state==4)copytext(s,"przerwany",79);
 else if(!r->ticks)copytext(s,"ponizej rozdzielczosci zegara",79);
 else {uint32_t rate;if(which==0){uint32_t q=quotient(r->amount,r->ticks);rate=q*50+quotient((r->amount-q*r->ticks)*50,r->ticks);}
  else rate=quotient((r->amount/1024)*50,r->ticks);
  s[0]=0;addnum(s,rate);append(s,which==0?" petli/s":" KiB/s");append(s,"  [");addnum(s,r->ticks*20);append(s," ms]");}
 line(y,label,s);
}
static void benchmark_view(void){unsigned fill;uint32_t elapsed=bench?wall()-start:0;
 result_line(4,0,"CPU / arytmetyka 32-bit");result_line(6,1,"ST-RAM / odczyt+zapis");result_line(8,2,"TT-RAM / odczyt+zapis");result_line(10,3,"Dysk / odczyt ACSI");
 term_frame(&screen,3,12,74,3,0);fill=elapsed>=100?70:(unsigned)quotient(elapsed*70,100);
 if(bench)term_fill(&screen,5,13,(int)fill,1,TERM_BLOCK,0);
 text(3,16,"RAM: blok 32 KiB; suma bajtow odczytanych i zapisanych.",74);
 text(3,17,"Dysk: tylko odczyt, poza cache OS; adapter moze miec cache.",74);
 text(3,18,"Zegar 50 Hz / 20 ms. Inne procesy wplywaja na wyniki.",74);
 text(3,19,"CPU: petle testowe, nie MIPS ani pomiar MHz. Esc przerywa.",74);
 text(3,21,"F5 Wszystko   F6 CPU   F7 RAM   F8 Dysk",74);
}
static void render(void){unsigned i;term_clear(&screen,0);text(1,0,"SYSINFO 0.1  /  ATARI SYSTEM",68);
 term_frame(&screen,1,1,78,22,0);
 for(i=0;i<3;i++){unsigned x=3+i*25;term_fill(&screen,(int)x,2,23,1,' ',page==i?TERM_INVERSE:0);
 term_text(&screen,(int)x+1,2,i==0?"F1 Konfiguracja":i==1?"F2 Dysk i partycje":"F3 Benchmarki",page==i?TERM_INVERSE:0,22);}
 if(page==0)hardware_view();else if(page==1)disk_view();else benchmark_view();
 text(1,23,status,78);term_fill(&screen,0,24,80,1,' ',TERM_INVERSE);
 term_text(&screen,1,24,"F1 Sprzet  F2 Dysk  F3 Testy  F5 Start  F9 Odswiez  F10 Koniec",TERM_INVERSE,78);
 if(rpc(gfx,19,(uint32_t)(uintptr_t)screen.cells,TERM_CELLS,0)<0)fault=5;
 dirty=0;
}
static void free_ram(void){if(ram){gui_call(17,0,0,(void *)ram,0);ram=0;}}
static void begin(unsigned which,unsigned mode){int32_t p;unsigned i;
 free_ram();bench=which;all_tests=mode;amount=offset=0;dirty=1;page=2;
 if(which==2||which==3){
  if(which==3&&!info.tt_total){results[2].state=2;bench=0;return;}
  p=gui_call(16,32768,which==3,0,0);
  if(p<0){results[which-1].state=2;bench=0;return;}
  ram=(volatile uint32_t *)(uintptr_t)(uint32_t)p;
  if(((uint32_t)p>=0x01000000u)!=(which==3)){free_ram();results[which-1].state=2;bench=0;return;}
  for(i=0;i<8192;i++)ram[i]=i^0xa55a5aa5u;
 }
 if(which==4&&info.boot_sectors<8){results[3].state=2;bench=0;return;}
 start=last_paint=wall();copytext(status,"Pomiar trwa. Esc: przerwij. Przelaczanie tts nadal dziala.",79);
}
static void advance(unsigned previous){
 if(all_tests==1&&previous<4){begin(previous+1,1);if(!bench)advance(previous+1);}
 else if(all_tests==2&&previous==2){begin(3,2);if(!bench){all_tests=0;copytext(status,"Test RAM zakonczony; niedostepne obszary pominieto.",79);}}
 else {bench=0;all_tests=0;copytext(status,"Pomiar zakonczony. Wyniki czasu rzeczywistego, bez zapisu dysku.",79);}
}
static uint32_t cpu_step(uint32_t x){unsigned i;for(i=0;i<1024;i++){x^=x<<13;x^=x>>17;x^=x<<5;x+=0x9e3779b9u;}return x;}
static void step(void){unsigned which=bench,i;uint32_t now;
 if(!which)return;
 if(which==1){seed=cpu_step(seed);amount+=1024;}
 else if(which==2||which==3){for(i=0;i<1024;i++){unsigned n=(offset+i)&8191;uint32_t x=ram[n];ram[n]=x^seed;}offset=(offset+1024)&8191;amount+=8192;}
 else {int r=gui_call(57,offset,8,disk_buffer,0);if(r!=4096){results[3].state=3;bench=0;all_tests=0;copytext(status,"Blad odczytu dysku; nic nie zostalo zapisane.",79);dirty=1;return;}
  seed^=disk_buffer[0];amount+=4096;offset+=8;if(offset>info.boot_sectors-8)offset=0;}
 now=wall();
 if(now-start>=100||amount>=0x1000000u){results[which-1].amount=amount;results[which-1].ticks=now-start;results[which-1].state=1;free_ram();bench=0;advance(which);dirty=1;}
 else if(now-last_paint>=10){last_paint=now;if(page==2)dirty=1;}
}
static void cancel(void){if(bench){results[bench-1].state=4;bench=all_tests=0;free_ram();copytext(status,"Pomiar przerwany.",79);dirty=1;}}
static void key(unsigned scan){
 if(scan==1){if(bench)cancel();else quit=1;return;}
 if(scan==0x44){cancel();quit=1;return;}
 if(scan>=0x3b&&scan<=0x3d){page=scan-0x3b;dirty=1;return;}
 if(scan==0x43&&!bench){if(refresh())copytext(status,"Blad odczytu raportu systemowego.",79);else copytext(status,"Dane odswiezone.",79);dirty=1;return;}
 if(page==1&&(scan==0x49||scan==0x51)){if(scan==0x49)disk_page=disk_page>=8?disk_page-8:0;else if(disk_page+8<info.volumes)disk_page+=8;dirty=1;return;}
 if(bench)return;
 if(scan==0x3f){unsigned i;for(i=0;i<4;i++)results[i].state=0;begin(1,1);}
 else if(scan==0x40)begin(1,0);
 else if(scan==0x41){begin(2,2);if(!bench)advance(2);}
 else if(scan==0x42)begin(4,0);
}
static void handle(const gui_message_t *m){if(m->sender)return;
 if(m->v[0]==0x4b4b4559&&(m->v[1]>>24)==1)key(m->v[1]&255);
 else if(m->v[0]==0x4b4d4f55){unsigned b=m->v[2]>>24,down=b&3,press=(down&~mouse_down)|((b>>3)&3);unsigned x=(m->v[1]>>16)/8,y=(m->v[1]&65535)/16;mouse_down=down;
  if(!(press&2))return;
  if(y==2&&x>=3&&x<78)key(x<28?0x3b:x<53?0x3c:0x3d);
  else if(y==24){if(x<11)key(0x3b);else if(x<20)key(0x3c);else if(x<30)key(0x3d);else if(x<40)key(0x3f);else if(x<52)key(0x43);else key(0x44);}
  else if(page==2&&y==21){if(x<17)key(0x3f);else if(x<26)key(0x40);else if(x<35)key(0x41);else key(0x42);}
 }
}
int app_main(void){gui_message_t m;int registered=0,captured=0;
 gfx=(uint32_t)lookup("SYSTEM.GFX");gui=(uint32_t)lookup("SYSTEM.GUI");if((int32_t)gfx<=0||(int32_t)gui<=0)return 1;
 if(rpc(gui,1,0,0,0))return 2;
 registered=1;
 if(rpc(gui,6,0,0,0)){console_report();goto done;}   /* desktop busy -> console report */
 captured=1;
 if(rpc(gfx,13,0,0,0)){fault=4;goto done;}
 if(refresh()){fault=6;goto done;}
 while(!quit&&!fault){unsigned n=0;int got;
  do{if(head!=tail){unsigned j;for(j=0;j<sizeof(m);j++)((volatile uint8_t *)&m)[j]=((uint8_t *)&pending[head&15])[j];head++;got=1;}else got=receive(&m,0)==0;
   if(got)handle(&m);
  }while(got&&++n<16&&!quit);
  if(quit)break;
  step();if(dirty)render();
  if(!bench&&head==tail){if(receive(&m,1))break;handle(&m);}
 }
 done:free_ram();if(captured)rpc(gui,7,0,0,0);if(registered)rpc(gui,2,0,0,0);
 if(fault)gui_call(1,0,0,"SYSINFO: blad API lub brak raportu.\n",0);
 return (int)fault;
}
