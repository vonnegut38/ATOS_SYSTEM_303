/* SYSINFO 0.2 - windowed system inspector with graphical tabs and buttons.
   Runs as an ordinary compositor window inside the desktop (like AC.APP) and
   draws its client through the GUI batch opcode instead of the text terminal. */
#include "app_abi.h"
#include "gui_app.h"
#define LNG_SYS
#include "app_lng.h"
#include "system_report.h"

#define KKEY 0x4b4b4559u
#define KMOU 0x4b4d4f55u
#define WMOV 0x574d4f56u
#define WCLS 0x57434c53u
#define GFX_RESTORE 6u
#define GFX_DESKTOP 13u
#define GFX_BATCH 18u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GFX_VIDEO_MODE 30u
#define GUI_REG 1u
#define GUI_UNREG 2u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u
#define GUI_BG 1u
#define GUI_FILL 2u
#define GUI_FRAME 3u
#define GUI_TEXT 4u
#define GUI_BEVEL 6u

#define WW 600u
#define WH 470u
#define CLIENT_H (WH-16u)

static system_report_t info;
static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static gui_draw_t list[80];
static unsigned n;
static unsigned page=0,disk_page=0;
static uint8_t quit,fullscreen,chunky;
static unsigned prev_left;
static int16_t wx=120,wy=80;
static uint16_t scrw=640,scrh=400,client_w=WW,client_h=CLIENT_H;
static uint16_t ui_width(void){return fullscreen?scrw:client_w;}
static uint16_t ui_height(void){return fullscreen?scrh:client_h;}
static char status[79];

/* ---- service plumbing (same as AC.APP) ---------------------------------- */
static gui_message_t pend[16];static unsigned pend_head,pend_tail;
static void pend_push(const gui_message_t *s)
{unsigned j;if(pend_tail-pend_head<16u){gui_message_t *d=&pend[pend_tail++%16];
 for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}}
static int pend_pop(gui_message_t *d)
{unsigned j;if(pend_head==pend_tail)return 0;
 {gui_message_t *s=&pend[pend_head++%16];for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}
 return 1;}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
static int32_t gui_op(uint32_t op)
{
    if(gui_pid<0)return -1;
    if(send((uint32_t)gui_pid,op,0,0,0))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gui_pid)return (int32_t)m.v[1];
        pend_push(&m);
    }
}
static void add(uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color,const char *text)
{
    gui_draw_t *d;uint8_t *p;unsigned i;
    if(n>=sizeof(list)/sizeof(list[0]))return;
    d=&list[n++];p=(uint8_t *)d;
    for(i=0;i<sizeof(*d);++i)p[i]=0;
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=chunky?15u:1u;
    if(text)copytext(d->text,text,sizeof(d->text));
}
static void flush(void)
{
    if(!n)return;
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    n=0;
}

/* ---- text helpers -------------------------------------------------------- */
static unsigned length(const char *s){unsigned k=0;while(s[k])k++;return k;}
static void append(char *d,const char *s){unsigned k=length(d);copytext(d+k,s,79-k);}
static void addnum(char *d,uint32_t v){char t[12];number(t,v);append(d,t);}
static const char *cpu_name(uint32_t v){switch(v){case 0:return "Motorola 68000";case 1:return "Motorola 68010";case 2:return "Motorola 68020";case 3:return "Motorola 68030";case 4:return "Motorola 68040";case 5:return "Motorola 68060";default:return lng_text(SYS_UNKNOWN);}}
static const char *machine_name(uint32_t v){switch(v){case 0:return "Atari ST";case 1:return "Atari Mega ST";case 2:return "Atari STE";case 3:return "Atari Mega STE";case 4:return "Atari TT";case 5:return "Atari Falcon";default:return lng_text(SYS_UNKNOWN);}}
static const char *chip_name(uint32_t v){switch(v){case 0:return lng_text(SYS_NO_CARD);case 1:return lng_text(SYS_ET4000);case 2:return "ATI Mach32";case 3:return "ATI Mach64";default:return lng_text(SYS_UNKNOWN_CARD);}}
static const char *adapter_name(uint32_t v){switch(v){case 0:return lng_text(SYS_NONE);case 1:return "Nova VME";case 2:return "CrazyDots VME";case 3:return "VOFA / Volksfarben";default:return lng_text(SYS_UNKNOWN);}}
static int refresh(void){int r=gui_call(56,sizeof(info),0,&info,0);
 if(r||info.magic!=SYSREPORT_MAGIC||info.version!=SYSREPORT_VERSION||info.size!=sizeof(info)||info.volumes>SYSREPORT_VOLUMES)return -1;
 return 0;}
static uint32_t quotient(uint32_t n,uint32_t d){uint32_t q=0,r=0;unsigned i;if(!d)return 0;
 for(i=0;i<32;i++){uint32_t carry=r>>31;r=(r<<1)|(n>>31);n<<=1;q<<=1;if(carry||r>=d){r-=d;q|=1;}}return q;}
static void size_kib(char *s,uint32_t v){s[0]=0;addnum(s,v/1024);append(s," KiB");}
static void size_mib(char *s,uint32_t v){s[0]=0;addnum(s,v/2048);append(s,".");addnum(s,((v%2048)*10)/2048);append(s," MiB");}

/* ---- benchmark state ----------------------------------------------------- */
static volatile uint32_t *ram;
static uint8_t disk_buffer[4096];
static uint32_t bench,all_tests,start,last_paint,amount,seed=0x13579bdf,offset,steps;
typedef struct {uint32_t amount,ticks,state;} bench_result;
static bench_result results[4];
static uint32_t wall(void){return (uint32_t)gui_call(58,0,0,0,0);}
static uint32_t cpu_step(uint32_t x){unsigned i;for(i=0;i<1024;i++){x^=x<<13;x^=x>>17;x^=x<<5;x+=0x9e3779b9u;}return x;}

/* ---- drawing ------------------------------------------------------------- */
static void button(int x,int y,int w,const char *label,int active)
{
    add(GUI_BEVEL,(int16_t)x,(int16_t)y,(uint16_t)w,20,0,0);
    list[n-1u].background=1u;   /* bevel highlight stays white */
    if(active){list[n-1u].reserved=1u;add(GUI_FILL,(int16_t)(x+1),(int16_t)(y+1),(uint16_t)(w-2),18,chunky?3u:0u,0);}
    add(GUI_TEXT,(int16_t)(x+6),(int16_t)(y+3),0,0,chunky?0u:0u,label);
    if(active){list[n-1u].color=chunky?0u:1u;list[n-1u].background=chunky?3u:0u;}
}
static void line(unsigned y,const char *label,const char *value)
{
    add(GUI_TEXT,16,(int16_t)(50+(int)y*16),0,0,chunky?0u:0u,label);
    add(GUI_TEXT,200,(int16_t)(50+(int)y*16),0,0,chunky?0u:0u,value);
}
/* Device inventory: report detected hardware and exposed API capabilities.
   Fixed controls stay in place; only the data rows are paged. */
#define DEV_SLOTS 32u
#define DEV_VIS   8u
static char dev_lbl[DEV_SLOTS][26];
static char dev_val[DEV_SLOTS][52];
static unsigned dev_count,dev_first;
static unsigned dev_visible(void){return ui_height()>116u?(ui_height()-116u)/16u:1u;}
static uint8_t saw_key,saw_mouse;   /* real observation, not a claim */

static void dev_add(const char *l,const char *v)
{
    if(dev_count>=DEV_SLOTS)return;
    copytext(dev_lbl[dev_count],l,sizeof(dev_lbl[0]));
    copytext(dev_val[dev_count],v,sizeof(dev_val[0]));
    ++dev_count;
}
static void devices_build(void)
{
    int32_t cap_result=gui_call(ATOS_CALL_AUDIO_CAPS,0,0,0,0);
    uint32_t caps=cap_result>0?(uint32_t)cap_result:0u;
    uint32_t st[4];char s[79];
    st[0]=st[1]=st[2]=st[3]=0u;
    (void)gui_call(ATOS_CALL_AUDIO_STATE,0,0,st,0);
    dev_count=0u;

    /* graphics - the compositor is drawing through it right now */
    dev_add("Graphics",info.hardware_report?(info.active_video==1?"ET4000 + VOFA":
             info.active_video==2?"ATI Mach32":"Shifter"):"no hardware report");
    /* sound - capability word of the kernel audio service */
    dev_add("Sound (PSG)",caps&1u?"YM2149 - tone API available":"tone API unavailable");
    /* audio interrupt + queue, from the service counters */
    s[0]=0;append(s,"events ");addnum(s,st[2]);append(s,st[0]?"  owner set":"  idle");
    dev_add("Audio ISR",s);
    s[0]=0;append(s,"owner ");addnum(s,st[0]);append(s,"  rejected ");addnum(s,st[3]);
    dev_add("Audio owner",s);
    /* network - fields come from the boot hardware probe */
    dev_add("Network",info.netusbee_ethernet?"NetUSBee Ethernet detected":
             info.netusbee_usb?"NetUSBee USB detected":"not detected");
    dev_add("Blitter",info.blitter&8u?"enabled":info.blitter&4u?"present (CPU fallback)":"not present");
    s[0]=0;append(s,info.disk_name[0]?info.disk_name:"no model");
    dev_add("Disk",s);
    size_kib(s,info.tt_total);dev_add("TT-RAM",s);
    s[0]=0;append(s,"tick ");addnum(s,info.tick_hz);append(s," Hz  wall ");addnum(s,info.wall_ticks);
    dev_add("Timer / clock",s);
    dev_add("Keyboard",saw_key?"Working (key event seen)":"no input event observed");
    dev_add("Mouse",saw_mouse?"Working (click event seen)":"no input event observed");
    dev_add("PCM API",caps&16u?"available":"unavailable");
    dev_add("Serial / printer","not queried");
    /* System components that are "in the machine" too - same honest sources. */
    dev_add("Machine",machine_name(info.machine));
    dev_add("Processor",cpu_name(info.cpu));
    if(info.clock_mhz){s[0]=0;addnum(s,info.clock_mhz);append(s," MHz");}
    else copytext(s,"unknown",sizeof(s));
    dev_add("Clock",s);
    dev_add("FPU",info.hardware_report?(info.fpu==2?"integrated":info.fpu==1?"present":"not detected"):"unknown");
    s[0]=0;addnum(s,info.st_total/1024);append(s," KiB total / ");addnum(s,info.st_free/1024);
    append(s," KiB free");
    dev_add("ST-RAM",s);
    s[0]=0;addnum(s,scrw);append(s," x ");addnum(s,scrh);
    append(s," x ");addnum(s,chunky?256u:2u);
    dev_add("Video mode",s);
    s[0]=0;append(s,caps?"PSG tone":"none");append(s," (caps ");addnum(s,caps);append(s,")");
    dev_add("Audio caps",s);
    s[0]=0;addnum(s,info.volumes);append(s," partition(s)");
    dev_add("Volumes",s);
    s[0]=0;addnum(s,info.cache_sectors/2);append(s,info.cache_tt?" KiB (TT)":" KiB (ST)");
    dev_add("Disk cache",s);
    s[0]=0;append(s,"bus ");addnum(s,info.bus);append(s," device ");addnum(s,info.device);
    dev_add("Boot device",s);
    s[0]=0;append(s,"frames ");addnum(s,info.wm_frames);append(s,"  damage ");addnum(s,info.wm_damage_pixels);
    dev_add("Compositor",s);
    dev_add("Mouse wheel","not supported");   /* honest: WM/IKBD carry no wheel */
}
static void devices_view(void)
{
    unsigned i;char s[79];
    unsigned visible=dev_visible();
    if(dev_first>=dev_count||visible>=dev_count)dev_first=0;
    s[0]=0;append(s,"DEVICES ");addnum(s,dev_first+1);append(s,"-");
    addnum(s,dev_first+visible<dev_count?dev_first+visible:dev_count);append(s,"/");addnum(s,dev_count);
    append(s,"  Up/Down");
    add(GUI_TEXT,16,50,0,0,0,s);
    for(i=0;i<visible&&dev_first+i<dev_count;++i){
        int16_t y=(int16_t)(70+(int)i*16);
        char value[52];unsigned chars=(ui_width()>206u?(ui_width()-206u)/8u:1u);
        if(chars>=sizeof(value))chars=sizeof(value)-1u;
        copytext(value,dev_val[dev_first+i],chars+1u);
        add(GUI_TEXT,16,y,0,0,0,dev_lbl[dev_first+i]);
        add(GUI_TEXT,190,y,0,0,0,value);
    }
}
static void hardware_view(void){char s[79];
    line(0,lng_text(SYS_MACHINE),machine_name(info.machine));
    line(1,lng_text(SYS_PROCESSOR),cpu_name(info.cpu));
    s[0]=0;if(info.clock_source){addnum(s,info.clock_mhz);append(s,info.clock_source==2?lng_text(SYS_CLOCK_NOMINAL):lng_text(SYS_CLOCK_BIOS));}else append(s,lng_text(SYS_CLOCK_UNKNOWN));
    line(2,lng_text(SYS_CLOCK),s);
    line(3,"FPU",info.hardware_report?(info.fpu==2?lng_text(SYS_INTEGRATED):info.fpu==1?lng_text(SYS_EXTERNAL):lng_text(SYS_NOT_DETECTED)):lng_text(SYS_NO_BIOS_DATA));
    s[0]=0;size_kib(s,info.st_total);append(s,lng_text(SYS_FREE));addnum(s,info.st_free/1024);append(s," KiB");line(5,lng_text(SYS_ST_TOTAL_FREE),s);
    {uint32_t tt=info.tt_total-(info.tt_reserved<info.tt_total?info.tt_reserved:0u);
     s[0]=0;size_kib(s,tt);append(s,"/");addnum(s,info.tt_total/1024);append(s," KiB");append(s,lng_text(SYS_FREE));addnum(s,info.tt_free/1024);append(s," KiB");line(6,lng_text(SYS_TT_TOTAL_FREE),s);}
    s[0]=0;size_kib(s,info.st_largest);append(s," / TT ");addnum(s,info.tt_largest/1024);append(s," KiB");line(7,lng_text(SYS_LARGEST_BLOCK),s);
    line(9,lng_text(SYS_ACTIVE_OUTPUT),info.active_video==1?"VOFA + ET4000":info.active_video==2?"ATI Mach32":"Shifter");
    s[0]=0;addnum(s,scrw);append(s," x ");addnum(s,scrh);append(s," / ");addnum(s,chunky?256u:2u);append(s,lng_text(SYS_COLOURS));line(10,lng_text(SYS_OUTPUT_RESOLUTION),s);
    line(12,lng_text(SYS_BIOS_CARD),info.hardware_report?chip_name(info.detected_chip):lng_text(SYS_USE_NEW_BIOS));
    line(13,lng_text(SYS_ADAPTER),info.hardware_report?adapter_name(info.detected_adapter):lng_text(SYS_UNDETERMINED));
    s[0]=0;append(s,lng_text(SYS_FRAMES));addnum(s,info.wm_frames);append(s,lng_text(SYS_DAMAGE));addnum(s,info.wm_damage_pixels);append(s," px");line(15,lng_text(SYS_COMPOSITOR),s);
}
static void disk_view(void){char s[79];unsigned i;
    line(0,"Model / INQUIRY",info.disk_name[0]?info.disk_name:lng_text(SYS_DEVICE_NO_MODEL));
    s[0]=0;append(s,lng_text(SYS_CONNECTION_VALUE));addnum(s,info.device);line(1,lng_text(SYS_CONNECTION),s);
    if(info.disk_capacity_known)size_mib(s,info.disk_sectors);else copytext(s,lng_text(SYS_UNKNOWN_CAPACITY),79);line(2,lng_text(SYS_DISK_CAPACITY),s);
    s[0]=0;addnum(s,info.cache_sectors/2);append(s,info.cache_tt?lng_text(SYS_CACHE_TT):lng_text(SYS_CACHE_ST));append(s," / write-through");line(3,lng_text(SYS_CACHE),s);
    add(GUI_TEXT,16,118,0,0,chunky?0u:0u,lng_text(SYS_PARTITION_COLUMNS));
    for(i=0;i<8&&disk_page+i<info.volumes;i++){
        unsigned v=disk_page+i;char drive[3]={(char)('C'+v),':',0};
        add(GUI_TEXT,20,(int16_t)(134+(int)i*16),0,0,chunky?0u:0u,drive);
        add(GUI_TEXT,44,(int16_t)(134+(int)i*16),0,0,chunky?0u:0u,info.volume[v].id);
        s[0]=0;size_mib(s,info.volume[v].sectors);
        add(GUI_TEXT,220,(int16_t)(134+(int)i*16),0,0,chunky?0u:0u,s);
        add(GUI_TEXT,300,(int16_t)(134+(int)i*16),0,0,chunky?0u:0u,info.volume[v].boot?lng_text(SYS_YES):"-");
    }
}
static void result_line(unsigned y,unsigned which,const char *label){char s[79];bench_result *r=&results[which];
    if(bench==which+1)copytext(s,lng_text(SYS_RUNNING),79);
    else if(!r->state)copytext(s,lng_text(SYS_NOT_RUN),79);
    else if(r->state==2)copytext(s,lng_text(SYS_UNAVAILABLE),79);
    else if(r->state==3)copytext(s,lng_text(SYS_MEASUREMENT_ERROR),79);
    else if(r->state==4)copytext(s,lng_text(SYS_CANCELLED),79);
    else if(!r->ticks)copytext(s,lng_text(SYS_BELOW_RESOLUTION),79);
    else {uint32_t rate;if(which==0){uint32_t q=quotient(r->amount,r->ticks);rate=q*50+quotient((r->amount-q*r->ticks)*50,r->ticks);}
  else rate=quotient((r->amount/1024)*50,r->ticks);
        s[0]=0;addnum(s,rate);append(s,which==0?lng_text(SYS_LOOPS):" KiB/s");append(s,"  [");addnum(s,r->ticks*20);append(s," ms]");}
    line(y,label,s);
}
static void benchmark_view(void){unsigned fill;uint32_t elapsed=bench?wall()-start:0;
    result_line(0,0,lng_text(SYS_CPU_TEST));result_line(2,1,lng_text(SYS_ST_TEST));result_line(4,2,lng_text(SYS_TT_TEST));result_line(6,3,lng_text(SYS_DISK_TEST));
    add(GUI_FRAME,16,166,300,20,0,0);fill=elapsed>=100?298:(unsigned)(elapsed*298/100);
    if(fill)add(GUI_FILL,17,167,(uint16_t)fill,18,chunky?3u:0u,0);
    add(GUI_TEXT,16,190,0,0,chunky?0u:0u,lng_text(SYS_RAM_NOTE));
    add(GUI_TEXT,16,206,0,0,chunky?0u:0u,lng_text(SYS_DISK_NOTE));
    add(GUI_TEXT,16,222,0,0,chunky?0u:0u,lng_text(SYS_CLOCK_NOTE));
    add(GUI_TEXT,16,238,0,0,chunky?0u:0u,lng_text(SYS_CPU_NOTE));
}
static void paint(void)
{
    uint16_t cw=ui_width(),chh=ui_height();
    n=0;
    add(GUI_FILL,0,0,cw,chh,chunky?15u:1u,0);
    add(GUI_TEXT,8,0,0,0,0,"SYSINFO 0.3");
    if(cw<520u||chh<330u){
        add(GUI_TEXT,8,22,0,0,0,"Enlarge window: Ctrl+Alt+Enter");
        flush();(void)gfx(GFX_CONTENT,chh,0,0);return;
    }
    button(8,20,120,"HARDWARE",page==0);
    button(134,20,120,"DISK",page==1);
    button(260,20,120,"BENCHMARK",page==2);
    button(386,20,120,"DEVICES",page==3);
    add(GUI_FRAME,8,44,(uint16_t)(cw-16u),(uint16_t)(chh-76u),0,0);
    if(page==3)devices_build();
    if(page==0)hardware_view();else if(page==1)disk_view();else if(page==2)benchmark_view();else devices_view();
    button(8,(int)(chh-24),110,"QUIT",0);
    button(124,(int)(chh-24),110,"REFRESH",0);
    button(240,(int)(chh-24),110,"RUN",0);
    if(page==3){button(366,(int)(chh-24),82,"PREV",0);button(454,(int)(chh-24),82,"NEXT",0);}
    else if(status[0])add(GUI_TEXT,362,(int)(chh-20),0,0,chunky?0u:0u,status);
    flush();
    (void)gfx(GFX_CONTENT,(uint32_t)chh,0,0);
}

/* ---- benchmark control --------------------------------------------------- */
static void free_ram(void){if(ram){gui_call(17,0,0,(void *)ram,0);ram=0;}}
static void begin(unsigned which,unsigned mode){int32_t p;unsigned i;
    free_ram();bench=which;all_tests=mode;amount=offset=steps=0;page=2;
    if(which==2||which==3){
        if(which==3&&!info.tt_total){results[2].state=2;bench=0;return;}
        p=gui_call(16,32768,which==3,0,0);
        if(p<0){results[which-1].state=2;bench=0;return;}
        ram=(volatile uint32_t *)(uintptr_t)(uint32_t)p;
        if(((uint32_t)p>=0x01000000u)!=(which==3)){free_ram();results[which-1].state=2;bench=0;return;}
        for(i=0;i<8192;i++)ram[i]=i^0xa55a5aa5u;
    }
    if(which==4&&info.boot_sectors<8){results[3].state=2;bench=0;return;}
    start=last_paint=wall();copytext(status,lng_text(SYS_MEASURING),79);
}
static void advance(unsigned previous){
    if(all_tests==1&&previous<4){begin(previous+1,1);if(!bench)advance(previous+1);}
    else if(all_tests==2&&previous==2){begin(3,2);if(!bench){all_tests=0;copytext(status,lng_text(SYS_RAM_DONE),79);}}
    else {bench=0;all_tests=0;copytext(status,lng_text(SYS_DONE),79);}
}
static void step(void){unsigned which=bench,i;uint32_t now;
    if(!which)return;
    if(which==1){seed=cpu_step(seed);amount+=1024;}
    else if(which==2||which==3){uint32_t a0=seed,a1=seed;
        for(i=0;i<512;i++){unsigned b=(offset+i*2)&8191;a0^=ram[b];a1^=ram[b+1];}
        seed=a0^a1;offset=(offset+1024)&8191;amount+=4096;}   /* read-only, 4 KiB */
    else {int r=gui_call(57,offset,8,disk_buffer,0);if(r!=4096){results[3].state=3;bench=0;all_tests=0;copytext(status,lng_text(SYS_DISK_ERROR),79);return;}
        seed^=disk_buffer[0];amount+=4096;offset+=8;if(offset>info.boot_sectors-8)offset=0;}
    ++steps;
    now=wall();
    /* Disk: bound the step count so a slow (or IRQ-stalled) IDE/ACSI read can
       never grind through 16 MiB for minutes; 32 reads = 128 KiB is plenty. */
    if(now-start>=100||amount>=0x1000000u||(which==4u&&steps>=32u)){results[which-1].amount=amount;results[which-1].ticks=now-start;results[which-1].state=1;free_ram();bench=0;advance(which);}
    else if(now-last_paint>=10){last_paint=now;paint();}
}
static void cancel(void){if(bench){results[bench-1].state=4;bench=all_tests=0;free_ram();copytext(status,lng_text(SYS_STOPPED),79);}}

/* ---- input --------------------------------------------------------------- */
static void run_all(void){unsigned i;for(i=0;i<4;i++)results[i].state=0;begin(1,1);}
static void mouse_click(int mx,int my)
{
    int cx=fullscreen?mx:mx-(int)wx,cy=fullscreen?my:my-(int)wy-16;
    if(cy<0||cx<0||cx>=(int)ui_width()||cy>=(int)ui_height())return;
    /* tabs */
    saw_mouse=1u;
    if(ui_width()<520u||ui_height()<330u)return;
    if(cy>=20&&cy<40){if(cx>=8&&cx<128)page=0;else if(cx>=134&&cx<254)page=1;else if(cx>=260&&cx<380)page=2;else if(cx>=386&&cx<506)page=3;else return;paint();return;}

    /* bottom buttons (windowed or full-screen row) */
    {int by=(int)ui_height();
     if(cy>=by-24&&cy<=by-4){
        if(cx>=8&&cx<118){cancel();quit=1;return;}
        if(cx>=124&&cx<234){if(!bench){if(refresh())copytext(status,lng_text(SYS_REPORT_ERROR),79);else copytext(status,lng_text(SYS_REFRESHED),79);}paint();return;}
        if(cx>=240&&cx<350){if(!bench)run_all();paint();return;}
        if(page==3&&cx>=366&&cx<448){dev_first=dev_first>=dev_visible()?dev_first-dev_visible():0;paint();return;}
        if(page==3&&cx>=454&&cx<536){if(dev_first+dev_visible()<dev_count)dev_first+=dev_visible();paint();return;}
     }}
}
static void key_event(uint32_t scan)
{
    if(scan==1||scan==0x44){cancel();quit=1;return;}
    saw_key=1u;
    if(scan==0x3b){page=0;paint();return;}
    if(scan==0x3c){page=1;paint();return;}
    if(scan==0x3d){page=2;paint();return;}
    if(scan==0x3e){page=3;paint();return;}
    if(scan==0x43&&!bench){if(refresh())copytext(status,lng_text(SYS_REPORT_ERROR),79);else copytext(status,lng_text(SYS_REFRESHED),79);paint();return;}
    if(scan==0x3f&&!bench){run_all();paint();return;}
    if(page==3&&(scan==0x48||scan==0x50||scan==0x49||scan==0x51)){
        unsigned step=(scan==0x49||scan==0x51)?dev_visible():1u;
        if(scan==0x48||scan==0x49)dev_first=dev_first>=step?dev_first-step:0;
        else if(dev_first+dev_visible()<dev_count)dev_first+=step;
        paint();return;
    }
    if(page==1&&(scan==0x49||scan==0x51)){if(scan==0x49)disk_page=disk_page>=8?disk_page-8:0;else if(disk_page+8<info.volumes)disk_page+=8;paint();return;}

}

int app_main(void)
{
    lng_init();
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    {int32_t v=gfx(GFX_VIDEO_MODE,0,0,0);
     if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
     chunky=(uint8_t)(gfx(35u,0,0,0)==8u);}
    /* Like AC: fullscreen only if we own the keyboard; inside the desktop we
       become an ordinary window. */
    if(gui_op(GUI_BEGIN_KEYS)==0){
        if(gfx(GFX_DESKTOP,0,0,0)==0)fullscreen=1;
        else (void)gui_op(GUI_END_KEYS);
    }
    if(refresh()){copytext(status,lng_text(SYS_REPORT_ERROR),79);}
    client_w=scrw<WW?scrw:WW;
    client_h=scrh>32u&&scrh-32u<CLIENT_H?scrh-32u:CLIENT_H;
    wx=(int16_t)(((int)scrw-(int)client_w)/2);if(wx<0)wx=0;
    wy=(int16_t)(((int)scrh-(int)client_h-16)/2);if(wy<16)wy=16;
    if(!fullscreen){
        (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,((uint32_t)client_w<<16)|(client_h+16u),0u);
        (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"SYSINFO",0,0);
    }
    paint();
    for(;;){
        int have=0;
        if(pend_pop(&m))have=1;
        else if(receive(&m,1)==0)have=1;
        if(have){
            if(!m.sender){
                if(m.v[0]==KKEY){if(((m.v[1]>>24)&0xffu)==1u)key_event(m.v[1]&0x7fu);}
                else if(m.v[0]==KMOU){int mx=(int)(uint16_t)(m.v[1]>>16),my=(int)(uint16_t)(m.v[1]&0xffffu);
                    unsigned buttons=(m.v[2]>>24)&7u;
                    if((buttons&1u)&&!prev_left)mouse_click(mx,my);
                    prev_left=buttons&1u;}
                else if(m.v[0]==WMOV){
                    uint16_t w=(uint16_t)(m.v[2]>>16),h=(uint16_t)m.v[2];
                    wx=(int16_t)(m.v[1]>>16);wy=(int16_t)m.v[1];
                    if(w!=client_w||h!=client_h+16u){client_w=w;client_h=h>16u?h-16u:0u;paint();}
                }
                else if(m.v[0]==WCLS){cancel();quit=1;}
            }
        }else pause_tick();
        if(quit)break;
        step();
        /* Run the active benchmark to completion without sleeping: the wall
           clock inside step() bounds it, so the measured rate reflects the real
           memory/disk speed and not the message-arrival rate. */
        while(bench)step();
    }
    free_ram();
    if(fullscreen){(void)gui_op(GUI_END_KEYS);(void)gfx(GFX_RESTORE,0,0,0);}
    return 0;
}
