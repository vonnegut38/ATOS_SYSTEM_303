/* Native windowed SETTINGS. Uses the same WM/IPC contract as MODPLAY.
 * Coordinates in draw batches are client-local; input coordinates are global.
 * The kernel owns the four-second preview deadline and rollback transaction.
 */
#include "app_abi.h"

static void rc_message(const char *prefix,int32_t rc);   /* zdefiniowany nizej */
#include "gui_app.h"
#include "atos_display.h"
#include "settings_config.h"
#define LIST_MAX 48u
#define WW 304u
#define WH 360u
#define TOP 42
#define ROW_H 24
/* Zakladka DESK ma najwiecej wierszy (kolory, pasek, tapeta, piksel, rozmiar
   ikon, siatka, APPLY), wiec uzywa ciasniejszego rzedu 22 px i przyciskow 20 px
   - inaczej APPLY wchodzil na stopke okna. */
#define DESK_ROW_H 22
#define KKEY 0x4b4b4559u
#define KMOU 0x4b4d4f55u
#define WCLS 0x57434c53u
#define WMOV 0x574d4f56u
/* Kody bledow widziane przez aplikacje (patrz system/kernel_exception.c):
 *   -994 (-0x3e2) = broker GFX.APP nie zna opcodu (stary GFX.APP bez cmpi #40),
 *   -997 (-0x3e5) = bramka w JADRZE: podglad juz trwa albo gre zajmuje ekran,
 *     -2          = sterownik karty nie obsluguje zadanej trojki (w,h,bpp). */
#define RC_UNKNOWN_OPCODE (-994)
#define RC_PREVIEW_ACTIVE (-997)
#define RC_MODE_UNSUPPORTED (-2)
static int32_t gfx_pid;
static uint32_t my_pid;
static gui_draw_t *list;
static unsigned n,focus_row=6,tab,th_count=1,th_cur,quit,dirty=1;
static unsigned prev_left,confirm_pending;
static int repair_offer;   /* zapis odmowiony (-6): R = naprawa woluminu */
static int wx=248,wy=100;
static char status[40];
static gui_message_t pending[32];
static unsigned head,tail;
static uint32_t guard_poll;
#define MODE_COUNT 9u
static const uint16_t mode_w[MODE_COUNT]={640u,800u,1024u,640u,1024u,640u,800u,1024u,800u};
static const uint16_t mode_h[MODE_COUNT]={400u,600u,768u,480u,768u,480u,600u,768u,600u};
static const uint8_t  mode_b[MODE_COUNT]={1u,1u,1u,4u,4u,8u,8u,8u,16u};
static const char *mode_label(unsigned i)
{
    switch(i){
        case 0u:return "640 x 400 - MONO";
        case 1u:return "800 x 600 - MONO";
        case 2u:return "1024 x 768 - MONO";
        case 3u:return "640 x 480 - 16 COLORS";
        case 4u:return "1024 x 768 - 16 COLORS";
        case 5u:return "640 x 480 - 256 COLORS";
        case 6u:return "800 x 600 - 256 COLORS";
        case 7u:return "1024 x 768 - 256 COLORS";
        case 8u:return "800 x 600 - 65K COLORS";
        default:return "?";
    }
}
static void copy_message(gui_message_t *d,const gui_message_t *s)
{unsigned i;for(i=0;i<sizeof(*d);++i)((volatile uint8_t *)d)[i]=((const uint8_t *)s)[i];}
static void push(const gui_message_t *m)
{
    /* Coalesce motion, preserving button edges and ordered window events. */
    if(tail!=head&&m->v[0]==KMOU){
        gui_message_t *last=&pending[(tail-1u)%32u];
        if(last->v[0]==KMOU&&last->v[2]==m->v[2]){copy_message(last,m);return;}
    }
    if(tail-head<32u)copy_message(&pending[tail++%32u],m);
}
static int pop(gui_message_t *m)
{if(head==tail)return 0;copy_message(m,&pending[head++%32u]);return 1;}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    gui_message_t m;
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];
        push(&m);
    }
}
static void message(const char *s){copytext(status,s,sizeof status);dirty=1;}
/* Zakladka DESKTOP: kolor tla pulpitu.  Wpis 2 palety, reszta motywu bez zmian. */
#define BG_COUNT 6u
static const uint16_t bg_rgb[BG_COUNT]={0x122u,0x003u,0x020u,0x200u,0x222u,0x000u};
static const char *bg_label(unsigned i)
{switch(i){case 0:return "GRAPHITE";case 1:return "NAVY";case 2:return "FOREST";case 3:return "MAROON";case 4:return "GREY";case 5:return "BLACK";default:return "?";}}
static unsigned bg_sel;
static void bg_sync(void)
{
    int32_t c=gfx(37,0,0,6);unsigned i;
    for(i=0;i<BG_COUNT;++i)if((int32_t)bg_rgb[i]==c){bg_sel=i;return;}
}
static void bg_apply(void){ (void)gfx(37,bg_rgb[bg_sel],0,5); }
/* Kolor PASKA GORNEGO: 4 presety; jadro buduje z jednego koloru 4-stopniowa
   rampe (wpisy 252..255), wiec pasek dostaje gradient jak w motywie STEEL. */
#define BAR_COUNT 5u
static const uint16_t bar_rgb[BAR_COUNT]={0x59fu,0x0a3u,0xa50u,0x555u,0xfffu};
static const char *bar_label(unsigned i)
{switch(i){case 0:return "STEEL";case 1:return "GREEN";case 2:return "AMBER";case 3:return "GREY";case 4:return "WHITE";default:return "?";}}
static unsigned bar_sel;
static void bar_sync(void)
{
    int32_t c=gfx(37,0,0,8);unsigned i;
    for(i=0;i<BAR_COUNT;++i)if((int32_t)bar_rgb[i]==c){bar_sel=i;return;}
}
static void bar_apply(void){ (void)gfx(37,bar_rgb[bar_sel],0,7); }
/* Tapeta: wzor rysowany w jadrze (opcode 41).  0 = jednolite tlo.
   4 = CUSTOM: wlasny kafelek 16x16 z zakladki TILE (WALL=4). */
#define WALL_COUNT 5u
static const char *wall_label(unsigned i)
{switch(i){case 0:return "SOLID";case 1:return "DITHER";case 2:return "CHECKER";case 3:return "DIAGONAL";case 4:return "CUSTOM";default:return "?";}}
static unsigned wall_sel;
/* Rozmiar ikon i gestosc siatki: wartosci trzyma JADRO (opcode 48/49/50), bo
   dane ikon sa w ICONS.DAT i tylko jadro moze przeladowac inna sekcje. */
#define ISZ_COUNT 3u
static const uint16_t isz_px[ISZ_COUNT]={32u,48u,64u};
static const char *isz_label(unsigned i)
{switch(i){case 0:return "32 PX";case 1:return "48 PX";case 2:return "64 PX";default:return "?";}}
#define GRID_COUNT 3u
static const char *grid_label(unsigned i)
{switch(i){case 0:return "COMPACT";case 1:return "NORMAL";case 2:return "WIDE";default:return "?";}}
static unsigned isz_sel=1u,grid_sel=0u;   /* 48 px + COMPACT = stan domyslny */
/* Wiersz wybrany strzalkami na zakladce DESK (klawiatura: UP/DOWN + ENTER,
   tak samo jak listy na pozostalych zakladkach). */
#define DESK_APPLY_ROW (BG_COUNT+5u)
static unsigned desk_row;
static void icons_sync(void)
{
    int32_t v=gfx(49,0,0,0);
    unsigned i;
    if(v<=0)return;
    {unsigned px=((uint32_t)v>>8)&0xffu,mode=(uint32_t)v&0xffu;
     for(i=0;i<ISZ_COUNT;++i)if((unsigned)isz_px[i]==px){isz_sel=i;break;}
     if(mode<GRID_COUNT)grid_sel=mode;}
}
/* Wzor tapety trzyma JADRO (op 37 sub 10).  Bez tego SETTINGS startowal
   zawsze z SOLID i APPLY nadpisywal WALL=4 (CUSTOM) zerem. */
static void wall_sync(void)
{
    int32_t w=gfx(37,0,0,10);
    if(w>=0&&w<(int32_t)WALL_COUNT)wall_sel=(unsigned)w;
}
/* Kolor ZAPALONEGO piksela kafelka (wpis 3 palety). */
#define TFG_COUNT 5u
static const uint16_t tfg_rgb[TFG_COUNT]={0x889u,0xf00u,0x0f0u,0x08fu,0xfffu};
static const char *tfg_label(unsigned i)
{switch(i){case 0:return "GREY";case 1:return "RED";case 2:return "GREEN";case 3:return "BLUE";case 4:return "WHITE";default:return "?";}}
static unsigned tfg_sel;
static void tfg_apply(void){ (void)gfx(45,tfg_rgb[tfg_sel]&0xfffu,0,4u); }
static int bg_save(void)
{
    /* Dopisz/zastap TYLKO linie BG= w DESKTOP.CFG.  RES i wszystkie inne klucze
       zostaja nietkniete - wczesniej ta funkcja przepuszczala plik przez
       settings_merge_mode, co nadpisaloby rozdzielczosc wybrana w zakladce
       RESOLUTION. */
    char src[1024],dst[1200],line[16];unsigned at=0,i,n=0,k=0;uint32_t st[2];int32_t fd,rc;
    fd=0;
    if(gui_call(ATOS_CALL_STAT,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",st)<0){message("CONFIG READ FAILED");return -1;}
    if(st[0]>sizeof src){message("CONFIG TOO LARGE");return -1;}
    fd=gui_call(ATOS_CALL_OPEN,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",0);
    if(fd<0){message("CONFIG OPEN FAILED");return -1;}
    while(at<st[0]){int32_t rd=gui_call(ATOS_CALL_READ,(uint32_t)fd,st[0]-at,src+at,0);if(rd<=0)break;at+=(unsigned)rd;}
    (void)gui_call(ATOS_CALL_CLOSE,(uint32_t)fd,0,0,0);
    if(at!=st[0]){message("CONFIG READ FAILED");return -1;}
    {static const char hex[]="0123456789ABCDEF";unsigned v=bg_rgb[bg_sel];
     line[n++]='B';line[n++]='G';line[n++]='=';
     line[n++]=hex[(v>>8)&15u];line[n++]=hex[(v>>4)&15u];line[n++]=hex[v&15u];line[n++]='\n';
     if(n+8u<(unsigned)sizeof line){v=bar_rgb[bar_sel];
      line[n++]='B';line[n++]='A';line[n++]='R';line[n++]='=';
      line[n++]=hex[(v>>8)&15u];line[n++]=hex[(v>>4)&15u];line[n++]=hex[v&15u];line[n++]='\n';}
     if(n+8u<(unsigned)sizeof line){line[n++]='W';line[n++]='A';line[n++]='L';line[n++]='L';line[n++]='=';
      line[n++]=(char)('0'+wall_sel);line[n++]='\n';
      {unsigned f=tfg_rgb[tfg_sel];
       line[n++]='T';line[n++]='I';line[n++]='L';line[n++]='E';line[n++]='F';line[n++]='G';line[n++]='=';
       line[n++]=hex[(f>>8)&15u];line[n++]=hex[(f>>4)&15u];line[n++]=hex[f&15u];line[n++]='\n';}}}
    for(i=0;i<n;++i)dst[k++]=line[i];
    i=0;
    while(i<at){
        unsigned start=i;while(i<at&&src[i]!='\n')++i;if(i<at)++i;
        if(i-start>=3&&src[start]=='B'&&src[start+1]=='G'&&src[start+2]=='=')continue;
        if(i-start>=4&&src[start]=='B'&&src[start+1]=='A'&&src[start+2]=='R'&&src[start+3]=='=')continue;
        if(i-start>=5&&src[start]=='W'&&src[start+1]=='A'&&src[start+2]=='L'&&src[start+3]=='L'&&src[start+4]=='=')continue;
        if(i-start>=7&&src[start]=='T'&&src[start+1]=='I'&&src[start+2]=='L'&&src[start+3]=='E'&&src[start+4]=='F'&&src[start+5]=='G'&&src[start+6]=='=')continue;
        while(start<i){if(k>=sizeof dst)return -1;dst[k++]=src[start++];}
    }
    rc=gui_call(ATOS_CALL_SAVE,k,0,(void *)"\\SYSTEM\\DESKTOP.CFG",dst);
    if(rc<0){rc_message("SAVE FAILED rc=",rc);return -1;}
    message("BACKGROUND SAVED");return 0;
}
static const char *theme_label(unsigned i)
{switch(i){case 0:return "CLASSIC";case 1:return "STEEL";case 2:return "PETROL";case 3:return "AMBER";case 4:return "MONO";default:return "THEME";}}
static void add(unsigned kind,int x,int y,unsigned w,unsigned h,unsigned color,const char *s)
{
    gui_draw_t *d;if(n>=LIST_MAX)return;d=&list[n++];zero(d,sizeof *d);
    d->kind=(uint16_t)kind;d->x=(int16_t)x;d->y=(int16_t)y;
    d->w=(uint16_t)w;d->h=(uint16_t)h;d->color=(uint16_t)color;d->background=1;
    if(s)copytext(d->text,s,sizeof d->text);
}
static void text(int x,int y,const char *s){add(GUI_TEXT,x,y,0,0,0,s);}
static void button(int x,int y,unsigned w,const char *s)
{add(GUI_BEVEL,x,y,w,22,0,0);text(x+6,y+3,s);}
static void choice(unsigned row,unsigned selected,const char *s)
{
    char buf[32];int y=TOP+(int)row*ROW_H;
    buf[0]='(';buf[1]=selected?'o':' ';buf[2]=')';buf[3]=' ';copytext(buf+4,s,sizeof(buf)-4);
    button(8,y,WW-16,buf);
    if(!tab&&focus_row==row)add(GUI_FRAME,10,y+1,WW-20,20,0,0);
}
/* Wiersze zakladki DESK: ciasniejszy rzad i nizszy przycisk (20 px). */
static void desk_button(int x,int y,unsigned w,const char *s)
{add(GUI_BEVEL,x,y,w,20,0,0);text(x+6,y+2,s);}
static void desk_choice(unsigned row,unsigned selected,const char *s)
{
    char buf[32];int y=TOP+(int)row*DESK_ROW_H;
    buf[0]='(';buf[1]=selected?'o':' ';buf[2]=')';buf[3]=' ';copytext(buf+4,s,sizeof(buf)-4);
    desk_button(8,y,WW-16,buf);
}
/* Stan woluminu dla zakladki VOLUME (ATOS_CALL_VOLUME d1=0 -> enum fat_health). */
static const char *volume_state_text(void)
{
    int32_t h=(int32_t)gui_call(ATOS_CALL_VOLUME,0,0,0,0);
    if(h==1)return "CLEAN - WRITES ENABLED";
    if(h==2)return "DIRTY - WRITES BLOCKED";
    if(h==3)return "HARD ERROR - WRITES BLOCKED";
    return "UNKNOWN - NO VOLUME";
}
/* Edytor kafelka: bufor statusu + zapis TILE= (64 hex) do DESKTOP.CFG. */
static char tile_status[40];
static int dbg_x=-1,dbg_y=-1;   /* ostatnie klikniecie (wsp. klienta) */
static int tile_save(void)
{
    uint8_t tile[32];char line[80],src[1024],dst[1200];unsigned at=0,i,n=0,k=0;uint32_t st[2];int32_t fd,rc;
    static const char hex[]="0123456789ABCDEF";
    if(gui_call(ATOS_CALL_STAT,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",st)<0){copytext(tile_status,"CONFIG READ FAILED",sizeof tile_status);dirty=1;return -1;}
    if(st[0]>sizeof src){copytext(tile_status,"CONFIG TOO LARGE",sizeof tile_status);dirty=1;return -1;}
    fd=gui_call(ATOS_CALL_OPEN,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",0);
    if(fd<0){copytext(tile_status,"CONFIG OPEN FAILED",sizeof tile_status);dirty=1;return -1;}
    while(at<st[0]){int32_t rd=gui_call(ATOS_CALL_READ,(uint32_t)fd,st[0]-at,src+at,0);if(rd<=0)break;at+=(unsigned)rd;}
    (void)gui_call(ATOS_CALL_CLOSE,(uint32_t)fd,0,0,0);
    if(at!=st[0]){copytext(tile_status,"CONFIG READ FAILED",sizeof tile_status);dirty=1;return -1;}
    {unsigned b;for(b=0;b<32u;++b)tile[b]=(uint8_t)(gfx(45,b,0,8u)&0xffu);}  /* odczyt bajt po bajcie */
    line[n++]='T';line[n++]='I';line[n++]='L';line[n++]='E';line[n++]='=';
    for(i=0;i<32u;++i){line[n++]=hex[(tile[i]>>4)&15u];line[n++]=hex[tile[i]&15u];}
    line[n++]='\n';
    /* dopisz TILE=, usun stare linie TILE i WALL (WALL ustawiamy na 4=CUSTOM) */
    line[n++]='W';line[n++]='A';line[n++]='L';line[n++]='L';line[n++]='=';line[n++]='4';line[n++]='\n';
    for(i=0;i<n;++i)dst[k++]=line[i];
    i=0;
    while(i<at){
        unsigned start=i;while(i<at&&src[i]!='\n')++i;if(i<at)++i;
        if(i-start>=5&&src[start]=='T'&&src[start+1]=='I'&&src[start+2]=='L'&&src[start+3]=='E'&&src[start+4]=='=')continue;
        if(i-start>=5&&src[start]=='W'&&src[start+1]=='A'&&src[start+2]=='L'&&src[start+3]=='L'&&src[start+4]=='=')continue;
        if(i-start>=7&&src[start]=='T'&&src[start+1]=='I'&&src[start+2]=='L'&&src[start+3]=='E'&&src[start+4]=='F'&&src[start+5]=='G'&&src[start+6]=='=')continue;
        while(start<i){if(k>=sizeof dst)return -1;dst[k++]=src[start++];}
    }
    rc=gui_call(ATOS_CALL_SAVE,k,0,(void *)"\\SYSTEM\\DESKTOP.CFG",dst);
    if(rc<0){rc_message("SAVE FAILED rc=",rc);return -1;}
    copytext(tile_status,"TILE SAVED - WALLPAPER: CUSTOM",sizeof tile_status);
    (void)gfx(37,4,0,9);            /* tapeta = wlasny kafelek */
    dirty=1;return 0;
}
/* Akcja wiersza zakladki DESK - wspolna dla kliknięcia i klawisza ENTER. */
static void desk_activate(void)
{
    if(desk_row<BG_COUNT){bg_sel=desk_row;bg_apply();}
    else if(desk_row==BG_COUNT){bar_sel=(bar_sel+1u)%BAR_COUNT;bar_apply();}
    else if(desk_row==BG_COUNT+1u){wall_sel=(wall_sel+1u)%WALL_COUNT;(void)gfx(37,wall_sel,0,9);}
    else if(desk_row==BG_COUNT+2u){tfg_sel=(tfg_sel+1u)%TFG_COUNT;tfg_apply();}
    else if(desk_row==BG_COUNT+3u){isz_sel=(isz_sel+1u)%ISZ_COUNT;
        (void)gfx(48,(uint32_t)isz_px[isz_sel],0,0);icons_sync();}
    else if(desk_row==BG_COUNT+4u){grid_sel=(grid_sel+1u)%GRID_COUNT;(void)gfx(50,grid_sel,0,0);}
    else if(desk_row==BG_COUNT+5u){bg_apply();bar_apply();tfg_apply();
        (void)gfx(37,wall_sel,0,9);bg_save();}
    dirty=1;
}
static void paint(void)
{
    unsigned i;n=0;add(GUI_FILL,0,0,WW,WH,1,0);
    button(4,8,58,(tab==0)?"[RES]":"RES");
    button(64,8,58,(tab==1)?"[THM]":"THM");
    button(124,8,58,(tab==2)?"[VOL]":"VOL");
    button(184,8,58,(tab==3)?"[DESK]":"DESK");
    button(244,8,58,(tab==4)?"[TILE]":"TILE");
    if(tab==0){
        for(i=0;i<MODE_COUNT;++i)choice(i,i==focus_row,mode_label(i));
        button(8,TOP+(int)MODE_COUNT*ROW_H,136,"APPLY [ENTER]");
        button(152,TOP+(int)MODE_COUNT*ROW_H,144,"CLOSE [ESC]");
    }else if(tab==1){
        for(i=0;i<th_count&&i<8;++i)choice(i,i==th_cur,theme_label(i));
        button(8,TOP+(int)th_count*ROW_H,136,"APPLY [ENTER]");
        button(152,TOP+(int)th_count*ROW_H,144,"CLOSE [ESC]");
    }else if(tab==3){
        /* Zakladka DESKTOP: kolor tla pulpitu (wpis 2 palety).  Zmiana jest
           natychmiast widoczna, bo pulpit przemalowuje sie po zmianie koloru. */
        for(i=0;i<BG_COUNT;++i)desk_choice(i,i==bg_sel,bg_label(i));
        {char b2[40];const char *pre="BAR COLOUR: ";const char *l=bar_label(bar_sel);
         unsigned q=0,z=0;
         while(pre[z]&&q<28u){b2[q]=pre[z];++q;++z;}
         z=0;
         while(l[z]&&q<38u){b2[q]=l[z];++q;++z;}
         b2[q]=0;
         desk_button(8,TOP+(int)BG_COUNT*DESK_ROW_H,WW-16,b2);}
        {char c2[40];const char *pre="PIXEL COLOUR: ";const char *l=tfg_label(tfg_sel);
         unsigned q=0,z=0;
         while(pre[z]&&q<28u){c2[q]=pre[z];++q;++z;}
         z=0;
         while(l[z]&&q<38u){c2[q]=l[z];++q;++z;}
         c2[q]=0;
         desk_button(8,TOP+(int)(BG_COUNT+2u)*DESK_ROW_H,WW-16,c2);}
        {char w2[40];const char *pre="WALLPAPER: ";const char *l=wall_label(wall_sel);
         unsigned q=0,z=0;
         while(pre[z]&&q<28u){w2[q]=pre[z];++q;++z;}
         z=0;
         while(l[z]&&q<38u){w2[q]=l[z];++q;++z;}
         w2[q]=0;
         desk_button(8,TOP+(int)(BG_COUNT+1u)*DESK_ROW_H,WW-16,w2);}
        {char b3[40];const char *pre="ICON SIZE: ";const char *l=isz_label(isz_sel);
         unsigned q=0,z=0;
         while(pre[z]&&q<28u){b3[q]=pre[z];++q;++z;}
         z=0;
         while(l[z]&&q<38u){b3[q]=l[z];++q;++z;}
         b3[q]=0;
         desk_button(8,TOP+(int)(BG_COUNT+3u)*DESK_ROW_H,WW-16,b3);}
        {char b4[40];const char *pre="ICON GRID: ";const char *l=grid_label(grid_sel);
         unsigned q=0,z=0;
         while(pre[z]&&q<28u){b4[q]=pre[z];++q;++z;}
         z=0;
         while(l[z]&&q<38u){b4[q]=l[z];++q;++z;}
         b4[q]=0;
         desk_button(8,TOP+(int)(BG_COUNT+4u)*DESK_ROW_H,WW-16,b4);}
        if(desk_row<=DESK_APPLY_ROW)
            add(GUI_FRAME,6,(int16_t)(TOP+(int)desk_row*DESK_ROW_H),
                (uint16_t)(WW-12),20,0,0);
        desk_button(8,TOP+(int)(BG_COUNT+5u)*DESK_ROW_H,136,"APPLY [ENTER]");
        desk_button(152,TOP+(int)(BG_COUNT+5u)*DESK_ROW_H,144,"CLOSE [ESC]");
    }else if(tab==4){
        /* Edytor kafelka: siatke 16x16 rysuje JADRO (256 komorek nie zmiesci
           sie w liscie rysowania).  Klik przelacza piksel, APPLY zapisuje
           kafelek i ustawia tapete na CUSTOM. */
        /* Siatka jako element LISTY rysowania (GUI_TILE_GRID) - wtedy jadro
           rysuje ja w powierzchni okna, a nie w ekranie.  Komorka 14 px,
           siatka 224x224 od (8,40); kolejnosc: najpierw tlo okna, potem
           siatka, na koncu ramka. */
        add(GUI_TILE_GRID,8,40,224,224,0,0);
        add(GUI_FRAME,7,39,226,226,0,0);
        button(8,272,110,"CLEAR");
        button(126,272,150,"APPLY [ENTER]");
        {char b[40];unsigned q=0;const char *pre="X=";unsigned v;
         while(pre[q]){b[q]=pre[q];++q;}
         v=(unsigned)(dbg_x<0?-dbg_x:dbg_x);
         {char d[6];int dn=0;
          if(!v)d[dn++]='0';
          else while(v){d[dn++]=(char)('0'+v%10u);v/=10u;}
          if(dbg_x<0)b[q++]='-';
          while(dn)b[q++]=d[--dn];}
         b[q++]=(char)',';
         {const char *p2="Y=";unsigned z=0;while(p2[z])b[q++]=p2[z++];}
         v=(unsigned)(dbg_y<0?-dbg_y:dbg_y);
         {char d[6];int dn=0;
          if(!v)d[dn++]='0';
          else while(v){d[dn++]=(char)('0'+v%10u);v/=10u;}
          if(dbg_y<0)b[q++]='-';
          while(dn)b[q++]=d[--dn];}
         b[q]=0;
         text(8,300,tile_status[0]?tile_status:b);}
    }else{
        /* Zakladka VOLUME: stan woluminu i naprawa znacznika FAT[1]. */
        add(GUI_FRAME,8,TOP,WW-16,86,0,0);
        text(16,TOP+8,volume_state_text());
        text(16,TOP+28,"FAT[1] CLEAN-SHUTDOWN FLAG");
        text(16,TOP+48,"DIRTY => KERNEL REFUSES ALL WRITES");
        text(16,TOP+66,"(SETTINGS, COPIES, SAVES ARE BLOCKED)");
        button(8,TOP+100,136,"REPAIR [ENTER]");
        button(152,TOP+100,144,"CLOSE [ESC]");
        text(8,TOP+128,"CHECKS BOTH FAT COPIES FIRST - NOT A FSCK");
    }
    /* Stopka z podpowiedzia tylko na zakladkach z lista - na TILE zaslania
       przyciski CLEAR/APPLY (zgloszenie uzytkownika). */
    if(tab!=4){
        text(8,WH-38,status[0]?status:"SELECT MODE, THEN APPLY");
        text(8,WH-20,"ARROWS / TAB / ENTER / ESC");
    }
    if(confirm_pending){
        add(GUI_FILL,12,112,280,90,1,0);add(GUI_FRAME,12,112,280,90,0,0);
        text(20,120,"KEEP MODE? 4 SEC TO CONFIRM");
        button(24,158,120,"YES [ENTER]");button(160,158,120,"NO [ESC]");
    }
    dirty=0;if(gfx(18,(uint32_t)(uintptr_t)list,n,0)<0)message("DRAW FAILED");
    /* Siatka edytora kafelka: rysowana po liscie, zeby tlo okna jej nie
       zamalowalo (wczesniej "nie dalo sie rysowac" - siatka byla niewidoczna). */

}
static void query_selection(void)
{
    unsigned i;int32_t g=gfx(30,0,0,0),b=gfx(35,0,0,0);
    if(g<=0)return;
    for(i=0;i<MODE_COUNT;++i)
        if(mode_w[i]==(uint16_t)((uint32_t)g>>16)&&mode_h[i]==(uint16_t)g&&
           (int32_t)mode_b[i]==b){focus_row=i;return;}
}
static void query_themes(void)
{
    int32_t t=gfx(37,0,0,0);
    if(t>=0){th_count=(uint32_t)t>>16;th_cur=(uint32_t)t&65535u;}
    if(!th_count||th_count>8)th_count=1;
    if(th_cur>=th_count)th_cur=0;
}
/* Preserve desktop icon positions, TOS options and unknown configuration keys. */
static int save_cfg(void)
{
    char src[1024],dst[1100],mode[40],num[12];unsigned at=0,i;
    int32_t fd,r;unsigned size;
    uint32_t st[2];
    /* Refuse unreadable input instead of replacing it with a partial config. */
    r=gui_call(ATOS_CALL_STAT,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",st);
    if(r<0){message("CONFIG READ FAILED");return -1;}
    size=st[0];if(size>sizeof src){message("CONFIG TOO LARGE");return -1;}
    fd=gui_call(ATOS_CALL_OPEN,0,0,(void *)"\\SYSTEM\\DESKTOP.CFG",0);
    if(fd<0){message("CONFIG OPEN FAILED");return -1;}
    while(at<size){r=gui_call(ATOS_CALL_READ,(uint32_t)fd,size-at,src+at,0);if(r<=0)break;at+=(unsigned)r;}
    (void)gui_call(ATOS_CALL_CLOSE,(uint32_t)fd,0,0,0);
    if(at!=size){message("CONFIG READ FAILED");return -1;}
    copytext(mode,"RES=",sizeof mode);at=4;
    for(i=0;i<3;++i){unsigned j;number(num,i==0?mode_w[focus_row]:i==1?mode_h[focus_row]:mode_b[focus_row]);
        for(j=0;num[j];++j)mode[at++]=num[j];
        if(i<2)mode[at++]='x';}
    mode[at]=0;r=settings_merge_mode(src,size,mode,dst,sizeof dst);
    if(r<0){message("CONFIG TOO LARGE");return -1;}
    r=gui_call(ATOS_CALL_SAVE,(uint32_t)r,0,(void *)"\\SYSTEM\\DESKTOP.CFG",dst);
    if(r<0){
        /* -6 = FAT_WRITE_READ_ONLY: wolumin "dirty" (np. po nieczystym resecie).
           Bez tego uzytkownik nie ma z GUI zadnej drogi naprawy. */
        if(r==-6){repair_offer=1;message("SAVE FAILED rc=6 - PRESS R TO REPAIR");}
        else rc_message("SAVE FAILED rc=",r);
    }else{repair_offer=0;message("CONFIG SAVED");}
    return r<0?-1:0;
}
/* Komunikat z KODEM bledu zamiast samego "REFUSED" (sugestia z audytu r2):
 * bez kodu nie dalo sie odroznic -994 (stary broker) od -997 (bramka w jadrze). */
static void rc_message(const char *prefix,int32_t rc)
{
    char t[40],d[12];unsigned j=0,k=0;int32_t v=rc<0?-rc:rc;
    while(prefix[j]&&j+1u<sizeof t){t[j]=prefix[j];++j;}
    if(!v)d[k++]='0';
    else{char rv[12];unsigned m=0;while(v){rv[m++]=(char)('0'+v%10);v/=10;}while(m)d[k++]=rv[--m];}
    d[k]=0;
    if(j<sizeof t)copytext(t+j,d,sizeof t-j);
    message(t);
}
static void finish_preview(int keep)
{
    int32_t r=gfx(ATOS_GFX_DISPLAY_GUARD,keep?0u:1u,0,0);
    confirm_pending=0;
    if(r<0){query_selection();rc_message("PREVIEW EXPIRED/FAILED rc=-",r);}
    else if(keep)(void)save_cfg();
    else{query_selection();message("PREVIOUS MODE RESTORED");}
}
static void apply(void)
{
    int32_t r;
    /* APPLY podczas trwajacego podgladu nie moze byc CICHO ignorowane: okno
       YES/NO juz czeka, a uzytkownik widzial tylko "MODE REFUSED" i nie wiedzial,
       czego aplikacja od niego chce.  Mowimy wprost, co zrobic. */
    if(confirm_pending){message("PREVIEW RUNNING - ANSWER YES/NO");return;}
    r=gfx(ATOS_GFX_DISPLAY_SET,((uint32_t)mode_w[focus_row]<<16)|mode_h[focus_row],
          (uint32_t)mode_b[focus_row]|ATOS_DISPLAY_PREVIEW,0);
    if(r<0){
        if(r==RC_PREVIEW_ACTIVE){
            /* Bramka w jadrze.  Jesli to NASZ podglad (op 39 sub 2 zwraca 1 dla
               wlasciciela), to nie blad, a stan: wracamy do okna YES/NO, zeby
               APPLY nie zostawial uzytkownika bez wyjscia. */
            if(gfx(ATOS_GFX_DISPLAY_GUARD,2,0,0)==1){
                confirm_pending=1;guard_poll=hires();
                message("PREVIEW RUNNING - ANSWER YES/NO");return;}
            message("PREVIEW BUSY (OTHER APP) rc=-997");return;}
        rc_message("MODE REFUSED rc=-",r);return;}
    confirm_pending=1;guard_poll=hires();message("WAITING FOR CONFIRMATION");
}
static void close_app(void)
{if(confirm_pending)finish_preview(0);quit=1;}
static void set_theme(unsigned t)
{
    if(t>=th_count)return;
    if(gfx(37,t,0,1)<0)message("THEME REFUSED");
    else{th_cur=t;message("THEME APPLIED (THIS SESSION)");}
}
static void event(const gui_message_t *m)
{
    if(m->sender)return;
    if(m->v[0]==WCLS){close_app();return;}
    if(m->v[0]==WMOV){wx=(int16_t)(m->v[1]>>16);wy=(int16_t)m->v[1];return;}
    if(m->v[0]==ATOS_DISPLAY_CHANGED){dirty=1;return;}
    if(m->v[0]==KKEY&&((m->v[1]>>24)&255u)==1u){
        unsigned scan=m->v[1]&127u;
        if(confirm_pending){if(scan==1)finish_preview(0);else if(scan==0x1c)finish_preview(1);return;}
        if(scan==1){close_app();return;}
        if(scan==0x0f||scan==0x4b||scan==0x4d){tab=(tab+1u)%5u;if(tab==0)query_selection();else if(tab==1)query_themes();else if(tab==3){bg_sync();bar_sync();wall_sync();icons_sync();}dirty=1;return;}
        if(scan==0x48||scan==0x50){
            int up=scan==0x48;
            if(tab==3){
                if(up){if(desk_row)desk_row--;}
                else if(desk_row<DESK_APPLY_ROW)++desk_row;
                dirty=1;return;}
            if(tab){if(up){if(th_cur)--th_cur;}else if(th_cur+1<th_count)++th_cur;}
            else{if(up){if(focus_row)--focus_row;}else if(focus_row+1<MODE_COUNT)++focus_row;}
            dirty=1;return;
        }
        if(repair_offer&&scan==0x13u){                 /* R = napraw wolumin */
            int32_t rr=(int32_t)gui_call(ATOS_CALL_VOLUME,1u,0,0,0);
            if(rr==0){repair_offer=0;message("VOLUME REPAIRED - PRESS APPLY AGAIN");}
            else rc_message("REPAIR FAILED rc=",rr);
            return;
        }
        if(scan==0x1c){
            if(tab==2){
                int32_t rr=(int32_t)gui_call(ATOS_CALL_VOLUME,1u,0,0,0);
                if(rr==0)message("VOLUME REPAIRED - WRITES ENABLED");
                else rc_message("REPAIR FAILED rc=",rr);
                dirty=1;return;
            }
            if(tab==1)set_theme(th_cur);
            else if(tab==3)desk_activate();
            else if(tab==4){tile_save();}
            else apply();
        }
    }else if(m->v[0]==KMOU){
        int x=(int16_t)(m->v[1]>>16)-wx,y=(int16_t)m->v[1]-wy-16;
        unsigned left=(m->v[2]>>24)&1u,press=left&&!prev_left;prev_left=left;
        dbg_x=x;dbg_y=y;
        if(!press||x<0||x>=(int)WW||y<0||y>=(int)WH)return;
        if(confirm_pending){
            if(y>=158&&y<180){if(x>=24&&x<144)finish_preview(1);else if(x>=160&&x<280)finish_preview(0);}return;
        }
        if(y>=8&&y<30){
            if(x>=4&&x<62)tab=0;else if(x>=64&&x<122)tab=1;
            else if(x>=124&&x<182)tab=2;else if(x>=184&&x<242)tab=3;
            else if(x>=244&&x<302)tab=4;
            if(tab==3){bg_sync();bar_sync();wall_sync();icons_sync();}
            dirty=1;return;}
        if(tab==4&&x>=8&&x<232&&y>=40&&y<264){
            int cx=(x-8)/14,cy=(y-40)/14;
            {int32_t rc=gfx(45,((uint32_t)(unsigned)cx<<8)|(uint32_t)(unsigned)cy,0,2u);
             char b[32];unsigned q=0;
             b[q++]='r';b[q++]='c';b[q++]='=';
             {int32_t v=rc<0?-rc:rc;char d[8];int dn=0;
              if(!v)d[dn++]='0';
              else while(v){d[dn++]=(char)('0'+v%10);v/=10;}
              if(rc<0)b[q++]='-';
              while(dn)b[q++]=d[--dn];}
             b[q]=0;copytext(tile_status,b,sizeof tile_status);}
            dirty=1;return;}
        if(x<8||x>=(int)WW-8||y<TOP)return;
        {unsigned rh=(tab==3)?(unsigned)DESK_ROW_H:(unsigned)ROW_H;
         unsigned row=(unsigned)(y-TOP)/rh;if((y-TOP)%rh>=(rh-2u))return;
         if(tab==3){
             /* Ten sam kod co klawisz ENTER (desk_activate); APPLY/CLOSE dziela
                sie na dwie polowki przycisku. */
             if(row<=BG_COUNT+4u){desk_row=row;desk_activate();}
             else if(row==BG_COUNT+5u){
                 if(x<144){desk_row=BG_COUNT+5u;desk_activate();}else close_app();}
         }
         else if(tab==1){if(row<th_count)set_theme(row);}
         else if(row<MODE_COUNT){focus_row=row;dirty=1;}
         else if(row==MODE_COUNT){if(x<144)apply();else if(x>=152)close_app();}}
    }
}
int app_main(void)
{
    gui_message_t m;int32_t g;char args[16];unsigned budget;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    my_pid=(uint32_t)gui_call(ATOS_CALL_GETPID,0,0,0,0);if((int32_t)my_pid<=0)return 1;
    list=(gui_draw_t *)(uintptr_t)(uint32_t)gui_call(ATOS_CALL_ALLOC,LIST_MAX*sizeof(*list),1,0,0);
    if((int32_t)(uintptr_t)list<=0)return 1;
    g=gfx(30,0,0,0);if(g>0){wx=((int)((uint32_t)g>>16)-(int)WW)/2;wy=((int)(uint16_t)g-(int)WH-16)/2;if(wy<16)wy=16;}
    if(gfx(20,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,(WW<<16)|(WH+16),0)<0)return 2;
    (void)gfx(21,(uint32_t)(uintptr_t)"SETTINGS",0,0);
    query_selection();query_themes();
    zero(args,sizeof args);if(gui_call(ATOS_CALL_ARGS,sizeof(args)-1,0,args,0)>0&&args[0]=='T')tab=1;
    paint();(void)gfx(32,0,0,my_pid);
    while(!quit){
        budget=0;while(budget++<16&&(pop(&m)||receive(&m,0)==0)){event(&m);if(quit)break;}
        if(quit)break;
        if(confirm_pending&&(uint32_t)(hires()-guard_poll)>=1229u){
            guard_poll=hires();if(gfx(ATOS_GFX_DISPLAY_GUARD,2,0,0)!=1){confirm_pending=0;query_selection();message("PREVIEW EXPIRED - RESTORED");}
        }
        if(dirty)paint();
        pause_tick();
    }
    (void)gfx(20,0,0,0);
    (void)gui_call(ATOS_CALL_FREE,0,0,list,0);return 0;
}
