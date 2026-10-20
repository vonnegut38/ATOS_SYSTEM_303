/* FIXDISK.APP - stan woluminu ATOS i naprawa znacznika czystosci FAT[1].
 *
 * Po nieczystym resecie (albo ubiciu emulatora) w trakcie zapisu jadro widzi
 * wolumin jako DIRTY i ODMAWIA WSZYSTKICH ZAPISOW (-FAT_WRITE_READ_ONLY):
 * nie da sie zapisac ustawien ani skopiowac pliku.  Program pokazuje stan
 * i naprawia go na wyraźne zadanie (ENTER):
 *   1) obie kopie FAT musza byc bajt w bajt identyczne,
 *   2) FAT[0] musi miec sensowny znacznik nosnika,
 *   3) dopiero wtedy ustawiane sa bity "clean" (15) i "no error" (14).
 * To NIE jest fsck - dlatego jadro nie robi tego automatycznie przy starcie.
 *
 * Sekwencja startowa skopiowana z dzialajacej PLASMY: trzeba znac SYSTEM.GUI,
 * przejac klawiature sesji (GUI_BEGIN_KEYS) i zadeklarowac geometrie klienta
 * (GFX_CONTENT) - bez tego WM nie przydzielal okna.
 *
 * Klawisze: ENTER = napraw, ESC = zamknij.
 */
#include "gui_app.h"
#include "app_abi.h"

#define KKEY 0x4b4b4559u
#define WCLS 0x57434c53u
#define GFX_BATCH 18u
#define GFX_WINDOW 20u
#define GFX_TITLE 21u
#define GFX_CONTENT 29u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u

#define WINX 236
#define WINY 60
#define WINW 440
#define WINH 200
#define CLIENTH (WINH-16)

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static gui_draw_t list[12];
static unsigned n;

static void sappend(char *d,const char *t){unsigned k=0;while(d[k])++k;copytext(d+k,t,79u-k);}
static int32_t vol(uint32_t cmd){return (int32_t)gui_call(ATOS_CALL_VOLUME,cmd,0,0,0);}
static const char *health_text(int32_t h)
{
    if(h==1)return "CLEAN - WRITES ENABLED";
    if(h==2)return "DIRTY - WRITES BLOCKED";
    if(h==3)return "HARD ERROR - WRITES BLOCKED";
    return "UNKNOWN - NO VOLUME";
}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){if(receive(&m,1))return -1;if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];}
}
static int32_t gui_op(uint32_t op)
{
    if(gui_pid<0)return -1;
    if(send((uint32_t)gui_pid,op,0,0,0))return -1;
    for(;;){if(receive(&m,1))return -1;if(m.sender==(uint32_t)gui_pid)return (int32_t)m.v[1];}
}
static void add(uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color,const char *text)
{
    gui_draw_t *d;uint8_t *p;unsigned i;
    if(n>=sizeof(list)/sizeof(list[0]))return;
    d=&list[n++];p=(uint8_t *)d;
    for(i=0;i<sizeof(*d);i++)p[i]=0;
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=1;
    if(text)copytext(d->text,text,sizeof(d->text));
}
static void paint(const char *state,const char *detail)
{
    n=0;
    add(GUI_FILL,0,0,WINW,CLIENTH,1,0);
    add(GUI_TEXT,8,6,0,0,0,"VOLUME CHECK / REPAIR");
    add(GUI_TEXT,8,24,0,0,0,"FAT[1] CLEAN-SHUTDOWN FLAG");
    add(GUI_FRAME,8,42,(uint16_t)(WINW-16),42,0,0);
    add(GUI_TEXT,16,50,0,0,0,state);
    add(GUI_TEXT,16,64,0,0,0,detail);
    add(GUI_TEXT,8,96,0,0,0,"WHEN DIRTY, THE KERNEL REFUSES EVERY WRITE:");
    add(GUI_TEXT,8,112,0,0,0,"SETTINGS, FILE COPIES, SAVES - ALL BLOCKED.");
    add(GUI_TEXT,8,140,0,0,0,"REPAIR CHECKS BOTH FAT COPIES FIRST; IT IS NOT");
    add(GUI_TEXT,8,156,0,0,0,"A FULL FSCK - CONFIRM ONLY IF YOU TRUST THE DISK.");
    add(GUI_TEXT,8,178,0,0,0,"ENTER = REPAIR        ESC = CLOSE");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,n,0);
}
static void show(const char *detail){paint(health_text(vol(0)),detail);}
int app_main(void)
{
    uint8_t keys;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    keys=(uint8_t)(gui_op(GUI_BEGIN_KEYS)==0);
    (void)gfx(GFX_WINDOW,((uint32_t)WINX<<16)|WINY,((uint32_t)WINW<<16)|WINH,0u);
    (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"FIXDISK",0,0);
    show("ENTER TO REPAIR, ESC TO LEAVE");
    (void)gfx(GFX_CONTENT,CLIENTH,0,0);
    for(;;){
        if(receive(&m,0)){pause_tick();continue;}
        if(m.v[0]==KKEY&&((m.v[1]>>24)&0xffu)==1u){
            uint32_t scan=m.v[1]&0x7fu;
            if(scan==1u)break;
            if(scan==0x1cu){
                int32_t r=vol(1);
                if(r==0)show("REPAIRED - VOLUME IS CLEAN NOW");
                else{char d[32];d[0]=0;sappend(d,"REPAIR FAILED rc=");
                     {char num[12];number(num,(uint32_t)(r<0?-r:r));sappend(d,num);}
                     show(d);}
            }
        }else if(m.v[0]==WCLS)break;
    }
    if(keys)(void)gui_op(GUI_END_KEYS);
    return 0;
}
