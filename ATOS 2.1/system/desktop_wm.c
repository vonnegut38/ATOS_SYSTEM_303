#include "atos_display.h"
#include "settings_config.h"
/* DESKTOP.APP - EmuTOS-style desktop shell for the ATOS compositor.
 *
 * Auto-started by the kernel (kernel_main.c) so the user never types
 * 'desktop.app'.  It owns the GUI keyboard, asks the WM for a full-screen
 * desktop canvas (GFX opcode 20/18) and draws through the gui_draw_t batch.
 *
 * Launcher: the white FILE menu opens a drop-down with
 *   OPEN TERMINAL  - GFX opcode 24 brings the shell console window back
 *   RUN...         - type an 8.3 name; SPAWN_ASYNC launches \SYSTEM\APP\NAME.APP
 *   CLOSE          - leave the desktop, returning to the text shell
 * Mouse clicks work; F10 opens the menu and the arrow/Enter/Escape keys drive
 * it so the desktop is usable without a mouse.
 */
#include "app_abi.h"
#include "setup_hit.h"
#include "gui_app.h"
#include "icons_ids.h"
#include "system_report.h"
#include "tos_ext.h"

/* Desktop canvas size: queried from the kernel at start-up (GFX 30), so the
   shell follows the active video mode instead of assuming ST high 640x400. */
static uint16_t SCRW=640,SCRH=400;
#define GFX_VIDEO_INFO 35u   /* returns depth: 8 (256c) or 1 (mono) */
#define KKEY 0x4b4b4559u

#define KMOU 0x4b4d4f55u
#define GUI_REG 1u
#define GUI_UNREG 2u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u
#define GFX_RESTORE 6u
#define GFX_DESKTOP 13u
#define GFX_BATCH 18u
#define GFX_CYCLE 22u
#define GFX_CONSOLE_SHOW 24u
#define SYS_SPAWN_ASYNC 34u
#define GFX_MOUSE_INJECT 25u
#define GFX_MOUSE_SCRIPT 26u
#define GFX_WINDOW 20u
#define GFX_VIDEO_MODE 30u
#define GFX_OVERLAY 31u    /* kernel keeps the pull-down above app windows */
#define GFX_WINDOW_ACTIVATE 32u /* wake a running app's window by pid */
#define GFX_WINDOW_STATE 33u    /* 0 none / 1 visible / 2 minimised */
#define WMOV 0x574d4f56u
/* Window closed by the compositor (its X gadget): value1 = window handle. */
#define WCLS 0x57434c53u

/* ---- File window -----------------------------------------------------------
   Clicking DISK C / DISK D opens a listing of that root in its own WM window
   (handle 2).  The listing is PAGED by the app: the app owns fb_page, so a
   click maps to a list row without having to know the compositor's scroll
   offset (the WM owns the scrollbar and the surface src_y). */
#define SYS_DIR_PAGE 52u
#define SYS_COPY 49u      /* a0=source a1=target -> copy file/dir */
#define SYS_TRASH 53u     /* a0=path -> move to trash */
#define SYS_MOVE 54u      /* a0=source a1=target -> rename/move */
#define SYS_PURGE 55u     /* a0=path -> erase (trash bucket) */
#define FBW 2u
#define FB_ROWS 10u
#define FB_ROW_H 18       /* kgfx_text draws 8x16 glyphs */
/* Ile wierszy listy miesci sie w powierzchni okna (800x600, lista od y=64):
   (600-64)/18 = 29.  Gdy katalog jest mniejszy, rysujemy CALOSC i oddajemy
   przewijanie WM (prawdziwy pasek); wieksze katalogi zostaja stronnicowane
   przez "MORE >", bo powierzchnia ma tylko ekran wysokosci. */
#define FB_MAXROWS 28u
#define FB_ATTR_DIR 0x10u
#define FB_TOTAL 64u
#define FB_CHUNK 32u
typedef struct {char name[13];uint8_t attributes,padding[2];
                uint32_t size,datetime;} dir_ent_t;
static int16_t fb_wx=104,fb_wy=40;
static uint16_t fb_ww=420,fb_wh=300;
/* Rozmiar okna z DESKTOP.CFG musi byc bezpieczny dla BIEZACEGO ekranu: po
   zmianie trybu na mniejszy zapisane 900x700 nie moze wyjsc za pulpit.
   Minima sa ROZNE, bo uklad okien jest rozny: lista plikow uklada sie plynnie
   (wystarczy 200x120), a okno KOSZA ma przyciski RESTORE/DELETE/EMPTY ALL na
   STALYM y=240 - ponizej ~270 wysokosci zniklyby pod dolna krawedzia. */
static void win_clamp(uint16_t *w,uint16_t *h,unsigned minw,unsigned minh)
{
    unsigned mw=(unsigned)SCRW>minw+16u?(unsigned)SCRW-16u:minw;
    unsigned mh=(unsigned)SCRH>minh+32u?(unsigned)SCRH-32u:minh;
    if(*w<minw)*w=(uint16_t)minw;
    if(*h<minh)*h=(uint16_t)minh;
    if(*w>mw)*w=(uint16_t)mw;
    if(*h>mh)*h=(uint16_t)mh;
}
static uint8_t fb_open,fb_more,fb_sort,fb_scrollmode;
static char fb_path[64],fb_title[24];
static dir_ent_t *fb_all;      /* whole directory, sorted in place */
static unsigned fb_total,fb_count,fb_page;
static int fb_sel;

/* ---- Trash window (handle 3): items parked by kfat_trash_path() ------------
   One item per bucket: \TRASH\Txxxxxxx\<name>.  The app lists \TRASH, reads the
   single entry inside each bucket and offers RESTORE (move the item back to the
   root, call 54), DELETE (purge that bucket) and EMPTY ALL (purge every bucket,
   call 55).  \TRASH itself is READ_ONLY; only the buckets may be purged. */
#define TRW 3u
#define TR_MAX 12u
#define TR_ROW_H 18
/* Naturalna wysokosc okna KOSZA: 12 wierszy listy (24 + 12*18 = 240) plus
   pasek przyciskow RESTORE/DELETE/EMPTY ALL i podpowiedz (44 px).  KOSZ jest
   ukladem STALYM, wiec deklarujemy te wysokosc jako dokument: gdy okno jest
   mniejsze, WM pokazuje pasek przewijania i mozna doscrollowac do przyciskow
   (wczesniej dol byl po prostu obcinany). */
#define TR_DOC_H (24u+TR_MAX*(uint32_t)TR_ROW_H+44u)
static int16_t tr_wx=116,tr_wy=56;
static uint16_t tr_ww=440,tr_wh=300;
static uint8_t trash_open;
static unsigned tr_count;
static int tr_sel=-1;
static char tr_name[TR_MAX][13],tr_bucket[TR_MAX][13],tr_msg[32];
static uint32_t tr_size[TR_MAX];
static uint8_t tr_isdir[TR_MAX];
static dir_ent_t tr_ents[TR_MAX],tr_inner[2];

/* ---- My Atari window (handle 6): the drives C:, D: and HDD partitions ----
   A "Computer" folder that replaces the separate DISK C / DISK D icons. */
#define MAW 6u
#define MA_ROW_H ((int)icon_px+4)          /* one drive icon per row */
static int16_t ma_wx=140,ma_wy=64;
static uint16_t ma_ww=340,ma_wh=300;
static uint8_t ma_open;
static int ma_sel=-1;
static system_report_t ma_info;
static unsigned ma_count;
static char ma_msg[32];

#define MENU_X 8
#define MENU_Y 18
#define MENU_W 152
#define MENU_ROW 18      /* kgfx_text is 8x16: 16px rows would overlap */
#define MENU_ITEMS 4u
#define OPT_X 64
#define OPT_W 176
#define OPT_ITEMS 2u    /* DISPLAY + THEMES (both open the SETUP window) */

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
/* The draw list lives on the kernel heap.  An application image is addressed
   PC-relative, so text+data+bss must stay inside one 32 KB window; this 1920-byte
   array was the difference between linking and "relocation truncated to fit". */
#define LIST_MAX 40u
static gui_draw_t *list;
static unsigned n;

static uint8_t menu_open,menu_which,menu_sel,run_mode,quit,prev_left,prev_right;
static char run_buf[40];
static unsigned run_len;
static char status[40];

/* Input messages share the mailbox with service replies.  A naive call_wait
   discarded anything whose sender was not the service, so every key/mouse event
   that arrived during a paint was lost and the broker reply stayed queued: the
   next call_wait returned the previous reply, outstanding replies piled up
   until the GFX mailbox was full and send() started failing (the desktop froze
   mid-drag).  Keep them in a small queue for the main loop instead. */
#define PEND_MAX 16u
static gui_message_t pending[PEND_MAX];
static unsigned pending_head,pending_tail;
static void copy_message(gui_message_t *d,const gui_message_t *s)
{unsigned j;for(j=0;j<sizeof(*d);++j)((uint8_t *)d)[j]=((const uint8_t *)s)[j];}
static unsigned pending_dropped;   /* observable overload counter */
static void pending_push(const gui_message_t *msg)
{
    unsigned count=pending_tail-pending_head;
    if(count>=PEND_MAX){
        /* Overload must not silently swallow a release.  Discard the oldest
           pure-motion KMOU (it carries no button edge); only when every entry
           is an edge does the oldest go, and that is counted so the failure is
           visible instead of turning into a stuck button. */
        unsigned i,slot=PEND_MAX;
        for(i=0;i<count;++i){
            const gui_message_t *p=&pending[(pending_head+i)%PEND_MAX];
            if(p->sender==0u&&p->v[0]==KMOU&&!((p->v[2]>>24)&0x1fu)){slot=i;break;}
        }
        if(slot<PEND_MAX){
            for(i=slot;i+1u<count;++i)
                copy_message(&pending[(pending_head+i)%PEND_MAX],&pending[(pending_head+i+1u)%PEND_MAX]);
            --pending_tail;
        }else{++pending_dropped;++pending_head;}
    }
    copy_message(&pending[pending_tail++%PEND_MAX],msg);
}
static int pending_pop(gui_message_t *msg)
{if(pending_head==pending_tail)return 0;copy_message(msg,&pending[pending_head++%PEND_MAX]);return 1;}

static int32_t call_wait(int32_t dest,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    if(send((uint32_t)dest,a,b,c,d))return -1;
    for(;;){
        if(receive(&m,1))return -1;
        if(m.sender==(uint32_t)dest)return (int32_t)m.v[1];
        pending_push(&m);
    }
}
static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c){return call_wait(gfx_pid,op,a,b,c);}
static int32_t gui(uint32_t op){return call_wait(gui_pid,op,0,0,0);}

static void add(uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color,const char *text)
{
    gui_draw_t *d;uint8_t *p;unsigned i;
    if(n>=LIST_MAX)return;
    d=&list[n++];p=(uint8_t *)d;
    for(i=0;i<sizeof(*d);i++)p[i]=0;
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=1;
    if(text)copytext(d->text,text,sizeof(d->text));
}

/* ---- desktop launcher bar --------------------------------------------------
   A configurable bottom bar of application buttons, so common programs do not
   need FILE -> RUN... every time.  Entries are read from \SYSTEM\LAUNCH.CFG
   ("LABEL=path" per line, ';' or '#' starts a comment, "*TERMINAL" and
   "*TRASH" are builtins).  A missing or empty file falls back to a built-in
   list with Atari Commander first. */
#define LBAR_MAX 16u
/* ---- Desktop icon grid -----------------------------------------------------
   Every icon (the two builtin disks + LAUNCH.CFG shortcuts) sits on a fixed
   grid and can be dragged to another cell.  FILE -> SAVE DESKTOP persists the
   layout to \SYSTEM\DESKTOP.CFG and restores it on the next start. */
/* Grid geometry: RUNTIME, bo uzytkownik wybiera rozmiar ikon (32/48/64) i
   gestosc siatki w SETTINGS -> DESK.  Rozmiar ikon trzyma JADRO (ICONS.DAT ma
   sekcje rozmiarow), a tryb siatki (COMPACT/NORMAL/WIDE) tez jest w jadrze -
   dzieki temu pulpit dostaje OBA w jednym odpytaniu (opcode 49) i przelicza
   uklad bez czytania plikow.  Wszystkie miejsca (rysowanie, hit test,
   uszkodzenia, przyciaganie, picker) licza z TYCH SAMYCH zmiennych. */
static int16_t icon_px=GUI_ICON_COLOR_SIZE;  /* bok rysowanej ikony (z jadra) */
static uint8_t grid_mode=0u;                 /* 0 COMPACT (domyslnie), 1 NORMAL, 2 WIDE */
static const uint8_t grid_gap_px[3]={0u,8u,24u};
static int16_t cell_w=(GUI_ICON_COLOR_SIZE+52),cell_h=(GUI_ICON_COLOR_SIZE+20);
static int16_t cell_left_px;                 /* ikona wysrodkowana w komorce */
#define CELL_W cell_w
#define CELL_H cell_h
#define CELL_LEFT cell_left_px
#define CELL_ERASE_W cell_w
#define CELL_ERASE_H (icon_px+16)
/* Bitmap origin; a twelve-character colour label extends 24 pixels left. */
#define GRID_X 28
#define GRID_Y 28
static void layout_cells(void);   /* potrzebuje chunky (deklaracja nizej) */
#define ICON_MAX (3u+LBAR_MAX)   /* DISK C/D + up to LBAR_MAX shortcuts */
typedef struct {char label[13];char path[64];uint8_t builtin;uint32_t pid;} launch_t;
/* Keep expandable shortcut tables off the PC-relative 32 KB image. */
static launch_t *launch;
static unsigned launch_count;
static uint8_t launch_loaded;
static uint8_t launch_last[LBAR_MAX];  /* last drawn state per button */
static unsigned launch_tick;
typedef struct {uint8_t res,builtin;int8_t gx,gy,li;char label[13];} icon_t;
static icon_t *icons;
static unsigned icon_count;
static int drag_icon=-1,drag_moved,drag_ox,drag_oy,drag_gx,drag_gy;
static int sel_icon=-1;        /* highlighted (single-clicked) icon */
/* Double-click candidate kept SEPARATE from the selection: clearing the
   selection (empty-desktop click, another icon, a menu) must not destroy the
   first click of a pair, and a selection made by any other route must not be
   mistaken for it. */
static int dbl_icon=-1;
static uint32_t dbl_tick;
static int hover_icon=-1;      /* icon under the mouse (colour hover state) */

/* ---- right-click context menu -------------------------------------------- */
#define CTX_MAX 6u
#define CTX_W 168
#define CTX_ROW 18
static uint8_t ctx_open;
static int16_t ctx_x,ctx_y;
static int ctx_sel,ctx_items,ctx_icon;   /* ctx_icon = -1 desktop, >=0 icon */
static uint8_t ctx_what;                 /* 0 desktop, 1 icon, 2 file row */
static int ctx_file_row;                 /* file window row (ctx_what==2) */
static const char *ctx_label[CTX_MAX];

/* ---- clipboard (COPY / PASTE) -------------------------------------------- */
static char clip_src[64];                /* full source path */
static char clip_name[13];               /* basename, for the paste target */
static uint8_t clip_set;
   /* hires() at the last icon click */
static uint8_t bg_dirty=1;     /* full background must be repainted */
static int8_t dirty_gx[ICON_MAX+2],dirty_gy[ICON_MAX+2]; /* old icon cells */
static unsigned dirty_count;
static void dirty_add(int8_t gx,int8_t gy)
{
    unsigned i;
    for(i=0;i<dirty_count;++i)if(dirty_gx[i]==gx&&dirty_gy[i]==gy)return;
    if(dirty_count<ICON_MAX+2u){dirty_gx[dirty_count]=gx;dirty_gy[dirty_count]=gy;++dirty_count;}
}
static uint8_t chunky;   /* 1 = 8bpp colour desktop */
/* Siatka pulpitu zalezy od trybu (chunky/mono), wiec liczy sie PO mode_query(). */
static void layout_cells(void)
{
    unsigned gap=(grid_mode<3u)?grid_gap_px[grid_mode]:grid_gap_px[1];
    cell_w=(int16_t)((int)icon_px+52+(int)gap);
    cell_h=(int16_t)((int)icon_px+20+(int)gap);
    cell_left_px=(int16_t)(((chunky?icon_px:GUI_ICON_MONO_SIZE)-cell_w)/2);
}
static uint8_t card_bpp;   /* format KARTY: 1/4/8/16 */
static uint8_t cur_bpp;  /* active depth: 1 (mono) or 8 (256 colours) */
/* Re-read the live mode (geometry via opcode 30, depth via opcode 35) and keep
   SCRW/SCRH/chunky in sync. */
static void mode_query(void)
{
    int32_t m=gfx(GFX_VIDEO_MODE,0,0,0);
    if(m>0){SCRW=(uint16_t)((uint32_t)m>>16);SCRH=(uint16_t)((uint32_t)m&0xffffu);}
    /* Opcode 35 zwraca REALNY format KARTY (1/4/8/16).  Pulpit rysuje ZAWSZE
       8 bpp chunky (albo mono dla 1 bpp), wiec do cur_bpp bierzemy format RAM -
       wczesniej kazda glebia poza 8 byla tu redukowana do 1 (mono). */
    {int32_t cb=gfx(35u,0,0,0);
     card_bpp=(uint8_t)((cb>0&&(uint32_t)cb<=16u)?cb:1);
     cur_bpp=(uint8_t)((card_bpp<=1u)?1u:8u);}
    chunky=(uint8_t)(cur_bpp==8u);
    if(SCRW<320u)SCRW=320u;
    if(SCRH<200u)SCRH=200u;
}
static void paint_icons(int clear_cells);
static void paint_menu(void);
static int launch_refresh(void);
static void icons_build(void);
static void desktop_load(void);
static void icon_activate(int idx);
static int desktop_save(void);
static int text_eq(const char *a,const char *b);
/* OPTIONS -> SETTINGS -> THEMES.  The shell palette lives in the kernel
   (system/kernel_theme.c): the window manager draws its chrome from entries
   16..19 and the compositor fills the desktop with entry 2, so one privileged
   path owns the colours.  The desktop only picks an index, and the kernel also
   owns the UI ramp the steel top bar is drawn from. */
#define GFX_THEME 37u
static uint8_t theme_index,theme_count=1u;
/* Tapeta: 0 = jednolite tlo, 1 = dither 50%, 2 = szachownica, 3 = skos,
   4 = WLASNY KAFELEK 16x16 (CUSTOM, edytor SETTINGS -> TILE).
   Rysowana jako GUI_PATTERN (jedna operacja na wiersz w jadrze), nie setkami
   prostokatow w liscie rysowania. */
static uint8_t wall_sel;
/* Co zostalo NAMALOWANE w ostatnim paint() - polling porownuje z tym, a nie
   z "poprzednio widziana" wartoscia: tamta inicjowala sie przy pierwszym
   odczycie i zjadala pierwsza zmiane z SETTINGS. */
static int32_t painted_bg=-2,painted_bar=-2,painted_wall=-2;
/* Diagnostyka tapety: ile razy polling wykryl zmiane i ile razy paint()
   naprawde wszedl w galaz tla.  Czytane przez memdump. */
uint32_t kdbg_dbg_poll,kdbg_dbg_wall_diff,kdbg_dbg_bg_paint,kdbg_dbg_pat;
/* Diagnostyka rozmiaru ikon: ostatnia odpowiedz opcode 49, uzyty px i licznik
   przelaczen.  Czytane przez memdump (m68k-elf-nm + hatari-debug). */
uint32_t kdbg_icon_q,kdbg_icon_px,kdbg_icon_changed;

static unsigned theme_flags,theme_ramp;
static uint8_t theme_bar_gradient(void){return (uint8_t)((theme_flags>>8)&1u);}
static void theme_sync(void)
{
    int32_t v=gfx(GFX_THEME,0,0,0u);              /* (count<<16)|current */
    if(v>0){theme_count=(uint8_t)((uint32_t)v>>16);theme_index=(uint8_t)((uint32_t)v&0xffu);}
    if(theme_count<1u)theme_count=1u;
    theme_ramp=(unsigned)gfx(GFX_THEME,0,0,3u);   /* first ramp entry */
    theme_flags=(unsigned)gfx(GFX_THEME,0,0,2u);  /* flags of the current theme */

}
static void theme_apply_index(uint8_t i)
{
    if(i>=theme_count)return;
    if(gfx(GFX_THEME,(uint32_t)i,0,1u)!=0)return;
    theme_index=i;
    theme_flags=(unsigned)gfx(GFX_THEME,0,0,2u);
    bg_dirty=1;    /* the desktop background colour changed */
}
static void paint(void);   /* show_notice() uses it before its definition */
/* Transient launch notice.  The desktop used to sit on an internal status
   string forever; it was removed, but that also removed the ONLY feedback a
   failed launch had ("TOS RC -2", "RUN FAILED").  This shows the message for a
   few seconds after a launch attempt and then lets the next repaint drop it. */
#define NOTICE_TICKS 30000u            /* ~2.4 s at 12288 hires counts/s */
static char notice[32];static uint32_t notice_tick;
static void show_notice(const char *s)
{
    copytext(notice,s,sizeof(notice));
    notice_tick=hires();
    paint();
}
/* The RUN prompt.  paint() adds it on top of a full desktop pass; typing
   uses paint_run(), which submits ONLY the prompt: the batch bounds are then
   the 340x56 box, so the kernel composes and flushes that rectangle instead
   of the whole desktop (a full pass cost ~218 ms per key at 800x600). */
static void add_run_box(void)
{
    add(GUI_FILL,150,120,340,56,1,0);
    add(GUI_FRAME,150,120,340,56,0,0);
    add(GUI_TEXT,158,126,0,0,0,"RUN PROGRAM (SYSTEM/APP):");
    add(GUI_TEXT,158,146,0,0,0,run_buf);
    add(GUI_TEXT,(int16_t)(158+(int)run_len*8),146,0,0,0,"_");
}
static void paint_run(void)
{
    /* only the input line: its paper strip, the text and the cursor (the
       frame and the caption above it do not change while typing) */
    n=0;add(GUI_FILL,152,144,336,16,1,0);
    add(GUI_TEXT,158,146,0,0,0,run_buf);
    add(GUI_TEXT,(int16_t)(158+(int)run_len*8),146,0,0,0,"_");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
}
static void paint(void)
{
    unsigned i;int full_background=bg_dirty;
    static uint8_t old_run;
    /* Refresh the launcher button colours before every repaint: the idle tick
       path is not guaranteed to run, and this is event-driven anyway. */
    (void)launch_refresh();
    n=0;
    /* The desktop background only changes on a mode/theme switch; every other
       event (icon drag, selection, menu) repaints just the affected cells, so
       the 480000..786432-byte background fill does NOT run per event. */
    if(bg_dirty){
        ++kdbg_dbg_bg_paint;
        /* Tapeta: wzor rysuje jadro (opcode 41, jedna operacja na wiersz);
           0 = jednolite tlo, jak dotad. */
        /* Wzor bierzemy z jadra (zmiana na zywo), a jesli jadro jeszcze nic nie
           wie - z wlasnego configu (WALL=).  Wczesniej po restarcie z WALL=1
           tlo zostawalo jednolite, bo getter zwracal 0. */
        int32_t wp=gfx(GFX_THEME,0,0,10u);
        /* painted_* zapisuje surowe wartosci JADRA (bez fallbacku na config!),
           inaczej polling porownywal 0 z 1 i przemalowywal tlo w kolko. */
        painted_wall=wp;
        painted_bg=gfx(GFX_THEME,0,0,6u);
        painted_bar=gfx(GFX_THEME,0,0,8u);
        if(wp<=0)wp=(int32_t)wall_sel;
        if(wp>0){++kdbg_dbg_pat;add(GUI_PATTERN,0,0,SCRW,SCRH,(uint16_t)wp,0);}
        else add(GUI_BG,0,0,SCRW,SCRH,0,0);
        bg_dirty=0;}
    /* Clear transient desktop UI before icons so their backgrounds are ready. */
    if(!full_background){
        add(GUI_BG,8,20,31u*8u,16,0,0);
        if(old_run)add(GUI_BG,150,120,340,56,0,0);
    }
    /* The RUN prompt must stay above application windows, exactly like the
       pull-down menus.  With a game window on top it was invisible and the
       keyboard went to the game, so a TOS program could not be started while a
       native owner held the audio (audit point 7).  x is 8-aligned because the
       kernel copies the rectangle in 1bpp byte runs. */
    if(old_run!=run_mode)
        (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)144u<<16)|(uint16_t)120u,
                  ((uint32_t)352u<<16)|(uint16_t)56u,run_mode?1u:0u);
    old_run=run_mode;
    paint_icons(!full_background);
    if(theme_bar_gradient()){                                /* steel top bar */
        unsigned tb;int16_t by=0;
        for(tb=0;tb<4u;++tb){add(GUI_FILL,0,by,SCRW,4,(uint16_t)(theme_ramp+tb),0);by=(int16_t)(by+4);}
    }else{
        /* Plaski pasek.  Domyslnie jasny (paper), ale gdy uzytkownik wybral
           kolor paska w SETTINGS -> DESKTOP, rysujemy go wpisem rampy (252),
           ktory jadro ustawilo na ten kolor.  Motyw CLASSIC ma rampe czarna,
           wiec bez wyboru zostaje dotychczasowy wyglad. */
        int32_t bc=gfx(GFX_THEME,0,0,8);
        if(bc>=0&&bc!=0xffff)add(GUI_FILL,0,0,SCRW,16,(uint16_t)theme_ramp,0);
        else add(GUI_FILL,0,0,SCRW,16,1,0);
    }
    add(GUI_FILL,0,16,SCRW,1,0,0);                          /* 1px bottom edge */
    add(GUI_TEXT,8,0,0,0,0,"FILE");                         /* fills the 16px bar */
    list[n-1u].reserved=1;                                  /* no background: the
        bar is a gradient the renderer cannot know about */
    add(GUI_TEXT,72,0,0,0,0,"SETTINGS");
    list[n-1u].reserved=1;
    if(menu_open){
        /* EmuTOS-style pull-down: FILE and OPTIONS each own their own list. */
        int16_t bx=(int16_t)(menu_which?OPT_X:MENU_X);
        uint16_t bw=(uint16_t)(menu_which?OPT_W:MENU_W);
        unsigned items=menu_which?OPT_ITEMS:MENU_ITEMS;
        add(GUI_FILL,bx,MENU_Y,bw,(uint16_t)(items*MENU_ROW),1,0);
        add(GUI_BEVEL,bx,MENU_Y,bw,(uint16_t)(items*MENU_ROW),0,0);   /* raised */
        for(i=0;i<items;++i){
            int sel=(i==menu_sel);
            const char *lbl=menu_which?
                ((i==0u)?"DISPLAY":(i==1u)?"THEMES":""):
                ((i==0u)?"OPEN TERMINAL":(i==1u)?"RUN...":(i==2u)?"SAVE DESKTOP":"CLOSE");
            if(sel){
                /* Inverse video: black bar, white glyphs (background 0). */
                add(GUI_FILL,(int16_t)(bx+2),(int16_t)(MENU_Y+(int)i*MENU_ROW+1),
                    (uint16_t)(bw-4),(uint16_t)(MENU_ROW-2),0,0);
                add(GUI_TEXT,(int16_t)(bx+6),(int16_t)(MENU_Y+(int)i*MENU_ROW+2),0,0,1,lbl);
                list[n-1u].background=0;
            }else
                add(GUI_TEXT,(int16_t)(bx+6),(int16_t)(MENU_Y+(int)i*MENU_ROW+2),0,0,0,lbl);
        }
    }
    if(run_mode)add_run_box();
    if(notice[0]&&(uint32_t)(hires()-notice_tick)<NOTICE_TICKS){
        add(GUI_TEXT,8,20,0,0,0,notice);
        list[n-1u].reserved=1;      /* no paper pill: it sits on the wallpaper */
    }
    /* The desktop used to echo an internal status string here ("MOVE DONE",
       "MODE SET", "STARTED", ...).  Those are diagnostics, not user-facing
       information, and they sat permanently in the top-left of the wallpaper.
       The status buffer is still used where it belongs to a dialog: the SETUP
       window reports "CONFIG SAVED" inside its own frame. */
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    /* Tell the kernel where the pull-down list is so it stays above app
       windows (they may sit at y=16, exactly where the list starts). */
    if(menu_open){
        int16_t bx=(int16_t)(menu_which?OPT_X:MENU_X);
        uint16_t bw=(uint16_t)(menu_which?OPT_W:MENU_W);
        unsigned items=menu_which?OPT_ITEMS:MENU_ITEMS;
        (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)bx<<16)|(uint16_t)MENU_Y,
                  ((uint32_t)bw<<16)|(uint16_t)(items*MENU_ROW),1u);
    }
}

/* Redraw ONLY the pull-down list.  Menu navigation used to call paint(), which
   redrew the whole desktop canvas - at 1024x768 that is a 98 KB full-screen
   dither on every arrow key, and an 8 MHz 68000 could not afford it.  The list
   rect fully covers what it draws, and the kernel overlay keeps it above any
   window that overlaps it. */
static void paint_menu(void)
{
    unsigned i,items=menu_which?OPT_ITEMS:MENU_ITEMS;
    int16_t bx=(int16_t)(menu_which?OPT_X:MENU_X);
    uint16_t bw=(uint16_t)(menu_which?OPT_W:MENU_W);
    uint16_t bh=(uint16_t)(items*MENU_ROW);
    n=0;
    add(GUI_FILL,bx,MENU_Y,bw,bh,1,0);
    add(GUI_BEVEL,bx,MENU_Y,bw,bh,0,0);
    for(i=0;i<items;++i){
        int sel=(i==menu_sel);
        const char *lbl=menu_which?
            ((i==0u)?"SETTINGS":(i==1u)?"THEME":""):
            ((i==0u)?"OPEN TERMINAL":(i==1u)?"RUN...":(i==2u)?"SAVE DESKTOP":"CLOSE");
        if(sel){
            add(GUI_FILL,(int16_t)(bx+2),(int16_t)(MENU_Y+(int)i*MENU_ROW+1),
                (uint16_t)(bw-4),(uint16_t)(MENU_ROW-2),0,0);
            add(GUI_TEXT,(int16_t)(bx+6),(int16_t)(MENU_Y+(int)i*MENU_ROW+2),0,0,1,lbl);
            list[n-1u].background=0;
        }else
            add(GUI_TEXT,(int16_t)(bx+6),(int16_t)(MENU_Y+(int)i*MENU_ROW+2),0,0,0,lbl);
    }
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)bx<<16)|(uint16_t)MENU_Y,
              ((uint32_t)bw<<16)|bh,1u);
}

/* Scripted compositor tests (F1/F2/F3): inject synthetic mouse events so the
   drag / resize / raise paths can be exercised without a host mouse. */
static void inject(int x,int y,unsigned buttons)
{
    (void)gfx(GFX_MOUSE_INJECT,(uint32_t)(((uint32_t)(uint16_t)x<<16)|(uint16_t)y),0,(uint32_t)buttons);
    pause_tick();pause_tick();
}
static void script_drag(void)
{
    int y;
    inject(500,50,1u);                       /* grab the WINDEMO title */
    for(y=65;y<=150;y+=15)inject(500,y,1u);
    inject(500,150,0u);                      /* release -> wm_move */
    (void)gfx(GFX_MOUSE_SCRIPT,0,0,0);       /* real IKBD mouse takes over again */
    copytext(status,"DRAG DONE",sizeof(status));paint();
}
static void script_resize(void)
{
    int x,y;
    inject(598,226,1u);                      /* bottom-right corner */
    for(x=590,y=216;x>=540;x-=10,y-=8)inject(x,y,1u);
    inject(540,196,0u);                      /* release -> wm_resize */
    (void)gfx(GFX_MOUSE_SCRIPT,0,0,0);
    copytext(status,"RESIZE DONE",sizeof(status));paint();
}
/* F5: drag the desktop's OWN window (the slow path: the app redraws its
   canvas; region-limited now). */
static void script_move(void)
{
    int y;
    inject(200,55,1u);
    for(y=70;y<=160;y+=15)inject(200,y,1u);
    inject(200,160,0u);
    (void)gfx(GFX_MOUSE_SCRIPT,0,0,0);
    copytext(status,"MOVE DONE",sizeof(status));paint();
}
/* F4: open the FILE pull-down and leave it open, so the menu-above-windows
   overlay can be checked with a window sitting under the list. */
static void script_menu(void)
{
    inject(20,8,1u);inject(20,8,0u);
    (void)gfx(GFX_MOUSE_SCRIPT,0,0,0);
    copytext(status,"MENU OPEN",sizeof(status));paint();
}
static void script_raise(void)
{
    inject(450,150,1u);inject(450,150,0u);   /* click the client -> raise */
    (void)gfx(GFX_MOUSE_SCRIPT,0,0,0);
    copytext(status,"RAISE DONE",sizeof(status));paint();
}
static int tos_program_path(const char *path);
static void spawn_run(void)
{
    gui_spawn_t sp;unsigned i,j;int dot=0;
    run_mode=0;
    if(!run_len){paint();return;}
    /* "Commands" launch point: a TOS program (.PRG/.TOS/.TTP/.GTP,
       case-insensitive) runs through the TOS personality exactly like a
       shortcut or a file-manager double-click.  run_buf is NUL-terminated and
       is used as the path as-is (full or relative), not under \SYSTEM\APP. */
    if(tos_program_path(run_buf)){
        int32_t rc=gui_call(ATOS_CALL_TOS_RUN,1,0,(void *)(uintptr_t)run_buf,0);
        if(rc<0){
            /* Numeric code, as the audit asked: "RUN FAILED" alone cannot
               tell a missing file from a refused TOS domain or a failed spawn. */
            uint32_t n_=(uint32_t)(-rc);char t_[24];unsigned k_=0;
            const char *pf_="RUN FAILED rc=-";
            while(*pf_&&k_<14u)t_[k_++]=*pf_++;
            if(!n_){t_[k_++]='0';}else{char rv_[12];unsigned j_=0;while(n_){rv_[j_++]=(char)('0'+n_%10u);n_/=10u;}while(j_)t_[k_++]=rv_[--j_];}
            t_[k_]=0;show_notice(t_);
        }else show_notice("STARTED");
        paint();
        return;
    }
    zero(&sp,sizeof sp);
    i=0;{const char *p="\\SYSTEM\\APP\\";for(j=0;p[j]&&i<60u;++j)sp.path[i++]=p[j];}
    for(j=0;j<run_len&&i<60u;++j){sp.path[i++]=(char)run_buf[j];if(run_buf[j]=='.')dot=1;}
    if(!dot){const char *e=".APP";for(j=0;e[j]&&i<63u;++j)sp.path[i++]=e[j];}
    sp.path[i]=0;
    {int32_t r=gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
     if(r<0){
         uint32_t n=(uint32_t)(-r);char t[12];unsigned k=0;
         if(!n){t[k++]='0';}else{char rev[12];unsigned j=0;while(n){rev[j++]=(char)('0'+n%10u);n/=10u;}while(j)t[k++]=rev[--j];}
         t[k]=0;
         copytext(status,"RUN E",sizeof(status));
         {unsigned L=0;while(status[L])++L;if(L+ (unsigned)k < sizeof(status)-1u){unsigned j;for(j=0;j<k;++j)status[L+j]=t[j];status[L+k]=0;}}
     }else copytext(status,"STARTED",sizeof(status));}
    paint();
}

static void fb_paint(void);
static void tr_paint(void);
static void ma_paint(void);
/* Defined with the SETUP dialog, after its state is declared. */
static int video_try(uint16_t w,uint16_t h,uint8_t depth)
{
    /* Opcode 36 przekazuje PELNY format: geometria w arg1, glebia w arg2.
       Wczesniej 8 bpp szlo opcodem 34, a KAZDA inna glebia opcodem 30 (mono) -
       wiec 4 i 16 nigdy nie docieraly do jadra, a opcode 30 dla identycznego
       rozmiaru zwracal sukces bez zmiany trybu ("MODE SET" na darmo). */
    int32_t r=gfx(ATOS_GFX_DISPLAY_SET,((uint32_t)w<<16)|(uint32_t)h,(uint32_t)depth,0);
    if(r==0){
        /* Adopt the new canvas size: the kernel reprogrammed the adapter and
           resized the compositor, so the desktop must repaint at the new
           geometry (including the full-screen background). */
        mode_query();
        bg_dirty=1;                    /* repaint the full background */
        copytext(status,"MODE SET",sizeof(status));
        paint();
        /* The file window is a separate WM surface: it was drawn with the OLD
           pitch, so the compositor would read it misaligned after the switch. */
        if(fb_open)fb_paint();
        if(trash_open){
            /* Same reason as the file window: the surface was drawn at the old
               pitch.  Clamp the frame into the new screen, re-declare it and
               redraw the client. */
            if(tr_wx+(int)tr_ww>(int)SCRW)tr_wx=(int16_t)((int)SCRW-(int)tr_ww);
            if(tr_wy+(int)tr_wh>(int)SCRH)tr_wy=(int16_t)((int)SCRH-(int)tr_wh);
            if(tr_wx<0)tr_wx=0;
            if(tr_wy<0)tr_wy=0;
            (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)tr_wx<<16)|(uint16_t)tr_wy,
                      ((uint32_t)tr_ww<<16)|tr_wh,TRW);
            tr_paint();
        }
        /* The SETUP dialog is a separate WM surface too: after the switch its
           client was drawn with the OLD pitch, so the compositor read it
           misaligned and the dialog looked like it had vanished. */
    }
    else if(w==640u||r==-0x3f3)copytext(status,"ONLY 640x400 (SHIFTER)",sizeof(status));
    else if(r==-1)copytext(status,"NO VIDEO SERVICE",sizeof(status));
    else if(r==-2)copytext(status,"SIZE NOT PROVIDED",sizeof(status));
    else if(r==-3)copytext(status,"CARD INIT FAILED",sizeof(status));
    else if(r==-4)copytext(status,"NO MODE INFO",sizeof(status));
    else if(r==-5)copytext(status,"NO VRAM SHADOW",sizeof(status));
    else copytext(status,"N/A: NEEDS ET4000/VDI",sizeof(status));
    paint();
    return (int)r;   /* 0 = tryb ustawiony; wynik jest teraz uzywany, nie ignorowany */
}
static void menu_activate(unsigned item)
{
    menu_open=0;bg_dirty=1;   /* closing the pull-down must erase it */
    (void)gfx(GFX_OVERLAY,0,0,0);
    if(menu_which){
        if(item==0u){
            /* SETTINGS jest teraz OSOBNA aplikacja (system/settings_app.c):
               DESKTOP.APP jest na granicy zasiegu PIC +-32 KB i APPLY/CANCEL
               sie w nim nie miescily.  Uzywamy ISTNIEJACEGO spawn_run(), ktory
               sam dokleja "\SYSTEM\APP\" i ".APP" - dzieki temu nie dodajemy
               ani nowego napisu, ani petli, czyli prawie nic do rozmiaru. */
            copytext(run_buf,"SETTINGS",sizeof(run_buf));run_len=8u;run_mode=0;
            spawn_run();
        }
        else if(item==1u){
            /* THEMES: nowa aplikacja ma wlasna zakladke motywow.  Przekazujemy
               argument "THEMES", zeby otworzyla sie OD RAZU na tej zakladce
               (aplikacja czyta go przez ATOS_CALL_ARGS).  Stare okno SETUP
               zniknelo z pulpitu, wiec miejsce na ten kod sie znalazlo. */
            gui_spawn_t sp;unsigned j;const char *pa="\\SYSTEM\\APP\\SETTINGS.APP";
            const char *ar="THEMES";
            zero(&sp,sizeof(sp));
            for(j=0;pa[j]&&j+1u<sizeof(sp.path);++j)sp.path[j]=pa[j];
            sp.path[j]=0;
            for(j=0;ar[j]&&j+1u<sizeof(sp.args);++j)sp.args[j]=ar[j];
            sp.args[j]=0;
            {int32_t r=gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
             copytext(status,r<0?"SETTINGS E":"THEMES",sizeof(status));}
            paint();
        }
        /* The menu item is gone: repaint the canvas, otherwise the stale menu
           image stays in the canvas AND keeps the overlay rectangle, which
           painted the menu on top of the window we just opened. */
        paint();
        return;
    }
    if(item==0u){(void)gfx(GFX_CONSOLE_SHOW,0,0,0);copytext(status,"TERMINAL OPEN",sizeof(status));}
    else if(item==1u){run_mode=1;run_len=0;run_buf[0]=0;status[0]=0;}
    else if(item==2u)desktop_save();
    else quit=1;
    paint();
}

/* Grid cell hit-test and layout build. */
static int icon_index_at(int mx,int my)
{
    unsigned i;
    for(i=0;i<icon_count;++i){
        int16_t x=(int16_t)(GRID_X+(int)icons[i].gx*CELL_W);
        int16_t y=(int16_t)(GRID_Y+(int)icons[i].gy*CELL_H);
        if(mx>=x+CELL_LEFT&&mx<x+CELL_LEFT+CELL_W&&my>=y&&my<y+CELL_H)return (int)i;
    }
    return -1;
}
static void icon_add(uint8_t res,uint8_t builtin,const char *label,int8_t gx,int8_t gy,int8_t li)
{
    unsigned i;
    if(icon_count>=ICON_MAX)return;
    for(i=0;i<12u&&label[i];++i)icons[icon_count].label[i]=label[i];
    icons[icon_count].label[i]=0;
    icons[icon_count].res=res;icons[icon_count].builtin=builtin;
    icons[icon_count].gx=gx;icons[icon_count].gy=gy;icons[icon_count].li=li;
    ++icon_count;
}
/* Przycienij pozycje ikon do BIEZACEJ siatki i ROZWIAZ KOLIZJE.
   Po zmianie rozmiaru ikon (albo gestosci) komorek jest mniej, a zapisane
   pozycje moga wypasc za ekran - albo dwie ikony trafia w te sama komorke
   (np. przy 64 px wiersze 4 i 5 scinaja sie do 4).  Kolejnosc ikon zostaje,
   przesuwa sie tylko ta, ktora kolizje powoduje: na pierwsze wolne pole. */
static void icons_clamp(void)
{
    int maxc=(int)((SCRW-GRID_X)/CELL_W),maxr=(int)((SCRH-GRID_Y-48)/CELL_H);
    uint8_t used[16*16];unsigned i,j;
    if(maxc<1)maxc=1;
    if(maxc>16)maxc=16;
    if(maxr<1)maxr=1;
    if(maxr>16)maxr=16;
    for(i=0;i<icon_count;++i){
        if(icons[i].gx<0)icons[i].gx=0;
        if(icons[i].gy<0)icons[i].gy=0;
        if((int)icons[i].gx>=maxc)icons[i].gx=(int8_t)(maxc-1);
        if((int)icons[i].gy>=maxr)icons[i].gy=(int8_t)(maxr-1);
    }
    if((unsigned)(maxc*maxr)<icon_count)return;      /* nie ma gdzie przenosic */
    for(j=0;j<sizeof used;++j)used[j]=0;
    for(i=0;i<icon_count;++i){
        int cell=(int)icons[i].gy*16+(int)icons[i].gx;
        if(!used[cell]){used[cell]=1;continue;}
        {int c,r;
         for(r=0;r<maxr;++r)for(c=0;c<maxc;++c)
             if(!used[r*16+c]){
                 icons[i].gx=(int8_t)c;icons[i].gy=(int8_t)r;
                 used[r*16+c]=1;goto placed;}
         placed:;}
    }
}
static void icons_build(void)
{
    unsigned i,pi=0;
    icon_count=0;
    /* The builtins always exist; the two drives live inside "My Atari" now.
       LAUNCH.CFG adds only user programs. */
    icon_add(0,5,"My Atari",0,0,-1);   /* computer: lists C:, D:, HDDs */
    icon_add(3,2,"TRASH",0,1,-1);      /* recycle-bin window */
    icon_add(1,1,"TERMINAL",0,2,-1);   /* shell console window */
    for(i=0;i<launch_count;++i)
        if(launch[i].builtin==0u)      /* real program path */
            icon_add(1,0,launch[i].label,1,(int8_t)(pi++),(int8_t)i);
}

/* ---- File window: directory listing -------------------------------------- */
/* Surface layout (client-relative): header at y=2..16, row 0 = "..", rows
   1..fb_count = the page, optional "MORE >" row.  FB_ROWS pages fit the client
   exactly, so the WM keeps the scrollbar hidden and every row is clickable at a
   fixed offset - the app never needs the compositor's scroll position. */
/* Fixed-width number/date fields so the listing is justified: name 0..11,
   tag 13..17, size 20..29 (right aligned), date a second text item at 32..47
   (gui_draw_t.text is only 32 bytes, so the date is its own item). */
static void fb_num(char *out,unsigned n,unsigned width)
{
    char t[12];unsigned q=0,i;
    if(!n)t[q++]='0';
    else{char r[12];unsigned k=0;while(n&&k<11u){r[k++]=(char)('0'+n%10u);n/=10u;}while(k)t[q++]=r[--k];}
    for(i=0;i<width;++i)out[i]=' ';
    if(q>width)q=width;
    for(i=0;i<q;++i)out[width-q+i]=t[i];
}
static void fb_two(char *p,unsigned v)
{p[0]=(char)('0'+(v/10u)%10u);p[1]=(char)('0'+v%10u);}
static void fb_date(char *p,uint32_t dt)
{
    unsigned y,mo,d,h,mi,i;
    if(!dt){for(i=0;i<16u;++i)p[i]=' ';p[16]=0;return;}
    y=1980u+((dt>>25)&0x7fu);mo=(dt>>21)&0x0fu;d=(dt>>16)&0x1fu;
    h=(dt>>11)&0x1fu;mi=(dt>>5)&0x3fu;
    p[0]=(char)('0'+(y/1000u)%10u);p[1]=(char)('0'+(y/100u)%10u);
    p[2]=(char)('0'+(y/10u)%10u);p[3]=(char)('0'+y%10u);p[4]='-';
    fb_two(p+5,mo);p[7]='-';fb_two(p+8,d);p[10]=' ';
    fb_two(p+11,h);p[13]=':';fb_two(p+14,mi);p[16]=0;
}
static void fb_row(char *row,const dir_ent_t *e)
{
    unsigned k;
    for(k=0;k<31u;++k)row[k]=' ';
    row[31]=0;
    for(k=0;k<12u&&e->name[k];++k)row[k]=e->name[k];
    if(e->attributes&FB_ATTR_DIR){row[13]='<';row[14]='D';row[15]='I';row[16]='R';row[17]='>';}
    else fb_num(row+20,e->size,10u);
}
static void fb_paint(void)
{
    unsigned i;int16_t y=64;uint16_t doc_h;
    fb_scrollmode=(fb_count<=FB_MAXROWS)?1u:0u;
    /* Tlo musi pokryc CALY dokument, nie tylko widoczna czesc: przy przewijaniu
       odsłanialy sie wiersze rysowane na nieodmalowanym (czarnym) obszarze
       powierzchni - dol obrazu byl czarny. */
    doc_h=fb_scrollmode?(uint16_t)(64u+(uint32_t)fb_count*(uint16_t)FB_ROW_H+2u)
                       :(uint16_t)(fb_wh-16);
    if(doc_h<(uint16_t)(fb_wh-16))doc_h=(uint16_t)(fb_wh-16);
    n=0;
    add(GUI_FILL,0,0,fb_ww,doc_h,1,0);
    /* Path bar: sunken, so it reads as a field and not as a stray white strip. */
    add(GUI_BEVEL,4,2,(uint16_t)(fb_ww-10),18,0,0);
    list[n-1u].reserved=1;
    add(GUI_TEXT,8,3,0,0,0,fb_path);
    /* Sort bar, right under the title bar. */
    for(i=0;i<3u;++i){
        const char *lbl=(i==0u)?"NAME":(i==1u)?"DATE":"TYPE";
        int16_t bx=(int16_t)(4+(int)i*58);
        add(GUI_BEVEL,bx,22,54,18,0,0);
        if(fb_sort==(uint8_t)i){
            list[n-1u].reserved=1;                 /* pressed */
            add(GUI_FILL,bx,22,54,18,0,0);
        }
        add(GUI_TEXT,(int16_t)(bx+11),23,0,0,0,lbl);
        if(fb_sort==(uint8_t)i){list[n-1u].color=1;list[n-1u].background=0;}
    }
    add(GUI_FILL,4,42,(uint16_t)(fb_ww-10),1,0,0);
    add(GUI_TEXT,8,45,0,0,0,"NAME");
    add(GUI_TEXT,8+26*8,45,0,0,0,"SIZE");
    add(GUI_TEXT,8+32*8,45,0,0,0,"DATE");
    {char r0[34];unsigned k;
     for(k=0;k<31u;++k)r0[k]=' ';
     r0[31]=0;
     r0[0]='.';r0[1]='.';r0[13]='<';r0[14]='D';r0[15]='I';r0[16]='R';r0[17]='>';
     add(GUI_TEXT,8,y,0,0,0,r0);
     y=(int16_t)(y+FB_ROW_H);}
    for(i=0;i<fb_count;++i){
        char row[34],date[18];
        const dir_ent_t *e=&fb_all[fb_scrollmode?i:(fb_page*FB_ROWS+i)];
        fb_row(row,e);
        fb_date(date,e->datetime);
        if((int)(i+1)==fb_sel)add(GUI_FILL,4,y,(uint16_t)(fb_ww-8),FB_ROW_H-2,0,0);
        add(GUI_TEXT,8,y,0,0,0,row);
        if((int)(i+1)==fb_sel){list[n-1u].color=1;list[n-1u].background=0;}
        add(GUI_TEXT,8+32*8,y,0,0,0,date);
        if((int)(i+1)==fb_sel){list[n-1u].color=1;list[n-1u].background=0;}
        y=(int16_t)(y+FB_ROW_H);
    }
    if(fb_more)add(GUI_TEXT,8,y,0,0,0,"MORE >");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,FBW);
    if(fb_scrollmode){
        /* Cala lista w powierzchni: wysokosc tresci = realna wysokosc listy,
           wiec WM pokazuje pasek, gdy okno jest nizsze niz lista. */
        (void)gfx(29,(uint32_t)(uint16_t)(64u+(uint32_t)fb_count*(uint16_t)FB_ROW_H+2u),0,FBW);
    }else{
        /* Za duzo wierszy na powierzchnie: stronnicowanie "MORE >". */
        (void)gfx(29,(uint32_t)(uint16_t)(fb_wh-16),0,FBW);
    }
}
/* ---- SETUP window: resolution selector -------------------------------- */
/* A normal decorated WM window (handle 5).  The compositor owns the frame and
   the title bar; the app draws the client and reacts to clicks.  The X gadget
   closes it exactly like the file window (WCLS + re-open on demand). */
#define SETW 5u
#define SET_ROW_H 24
#define SET_RES 3u      /* resolutions only: 640x400, 800x600, 1024x768 */
#define SET_COL 3u      /* colour depths only: 16, 256, 65K */
#define SET_HEAD_RES 0u /* section header "RESOLUTION" */
#define SET_RES0 1u     /* first resolution row */
#define SET_HEAD_COL 4u /* section header "COLORS" */
#define SET_COL0 5u     /* first colour row */
/* TOS compatibility options.  Two INDEPENDENT keys on purpose: "at START" is
   the moment, "prefer TT-RAM" is the placement, so "load on demand, but into
   TT RAM" stays expressible.  Persisted in DESKTOP.CFG as TOSTA/TOSTT. */
#define SET_TOS_START 8u /* "Load TOS at START" checkbox */
#define SET_TOS_TT 9u    /* "Prefer TT-RAM" checkbox */
#define SET_SAVE 10u     /* index of the "Save Settings" row on tab 0 */
#define SET_ROWS 11u     /* 2 headers + 3 resolutions + 3 colours + 2 TOS + save */
#define SET_TABS 2u
#define SET_TAB_Y 2
#define SET_TAB_H 18
#define SET_FOOTER_H 60  /* status line + key hint below the last row */
#define SET_TOP 44      /* first content row, below the tab strip */
/* Minimum window height for SET_ROWS content rows plus the footer.  The window
   used to be a fixed 256 px, so after the two TOS rows were added the last rows
   (PREFER TT-RAM, SAVE SETTINGS) were drawn on top of the status line and the
   key hint - the hint covered SAVE SETTINGS exactly. */
static uint8_t tos_cfg_at_start, tos_cfg_prefer_tt;  /* in DESKTOP.CFG */
/* TOS compatibility options, persisted in \SYSTEM\DESKTOP.CFG (with RES/THEME)
   by SAVE SETTINGS and read back by the kernel at START. */
/* Rozdzielczosc i liczbe kolorow wybiera sie OSOBNO (jak w oknie ustawien
   ekranu): 3 rozdzielczosci, 3 glebie, kazda grupa ma wlasny znacznik [x].
   Kolejnosc wierszy tab 0: RESOLUTION, 3 rozdzielczosci, COLORS, 3 glebie,
   opcje TOS, SAVE SETTINGS. */
typedef struct {uint16_t w,h;} setup_res_t;
/* UWAGA: tablica NIE moze trzymac wskaznikow na napisy.  DESKTOP.APP jest
   kompilowany z -mpcrel i ladowany pod dowolnym adresem BEZ relokacji, wiec
   wskaznik zapisany na etapie linkowania wskazywalby na smieci - dokladnie to
   widac bylo jako trzy puste ramki pod COLORS.  Napisy zwracamy funkcja
   (literaly sa PC-wzgledne), tak jak dziala res_label(). */
typedef struct {uint8_t depth;} setup_col_t;
static const setup_res_t setup_res[SET_RES]={{640u,480u},{800u,600u},{1024u,768u}};
static const setup_col_t setup_col[SET_COL]={{4u},{8u},{16u}};
/* Biezacy wybor.  Trzymane osobno, bo to dwa niezalezne ustawienia. */
static uint8_t setup_res_sel=1u;  /* 800x600 */
static uint8_t setup_col_sel=1u;  /* 256 colours (8 bpp) - jedyne zaimplementowane */
/* Czy kombinacja (rozdzielczosc, glebia) jest obslugiwana?  Wlasne tabele
   maja tylko 640x480x4 (16 kolorow) i 800x600x16 (65K).  Pozostale kombinacje
   sterownik zmapowalby na inny tryb - czyli po cichu zmienil rozdzielczosc.
   Dlatego sprawdzamy to PRZED wywolaniem video_try, w OBU galeziach (wybor
   rozdzielczosci i wybor kolorow), i mowimy o tym wprost. */
/* Rows in the active tab: the resolution list, or one row per theme plus save. */
/* Tab strip hit test: 0/1 for a tab, 0xFF for none. */
/* Okno ma 16-pikselowy pasek tytulu (kernel_wm rysuje klienta od frame.y+16),
   a malowanie liczy wiersze WZGLEDEM KLIENTA.  Hit-test musi odjac to samo,
   inaczej kazdy wiersz trafia o jeden w dol (audyt r2: "16 COLORS wskazuje na
   256 COLORS"). */
#define SET_TITLE_H 16
/* Re-declare and redraw the dialog at the new pitch after a mode change. */

/* Row under a screen y, or -1.  Layout: path 2..17, sort bar 20..33, column
   header 40, rows from 52 (row 0 = ".."). */
static int fb_row_at(int my)
{
    int sy=my-(int)fb_wy-16;
    if(!fb_open||sy<64)return -1;
    if(fb_scrollmode){
        /* Lista rysowana od poczatku powierzchni, a WM przesuwa widok o src_y. */
        int32_t sc=gfx(38,0,0,FBW);
        if(sc<0)sc=0;
        return (int)((sy-64+(int)sc)/FB_ROW_H);
    }
    return (sy-64)/FB_ROW_H;
}
/* Sort button under the pointer: 0=NAME 1=DATE 2=TYPE, else -1. */
static int fb_sort_at(int mx,int my)
{
    int sx=mx-(int)fb_wx,sy=my-(int)fb_wy-16;
    if(sy<22||sy>=40)return -1;
    if(sx>=4&&sx<58)return 0;
    if(sx>=62&&sx<116)return 1;
    if(sx>=120&&sx<174)return 2;
    return -1;
}
static void fb_copy_ent(dir_ent_t *d,const dir_ent_t *s)
{unsigned i;uint8_t *dp=(uint8_t *)d;const uint8_t *sp=(const uint8_t *)s;
 for(i=0;i<sizeof(*d);++i)dp[i]=sp[i];}
static int fb_is_dir(const dir_ent_t *e){return (e->attributes&FB_ATTR_DIR)!=0;}
static int fb_ci(const char *a,const char *b)
{
    unsigned i;
    for(i=0;;++i){
        char ca=a[i],cb=b[i];
        if(ca>='a'&&ca<='z')ca=(char)(ca-32);
        if(cb>='a'&&cb<='z')cb=(char)(cb-32);
        if(ca!=cb)return ca<cb?-1:1;
        if(!ca)return 0;
    }
}
static const char *fb_ext(const char *n)
{const char *e=0;unsigned i;for(i=0;n[i];++i)if(n[i]=='.')e=&n[i+1];return e?e:"";}
static int fb_cmp(const dir_ent_t *a,const dir_ent_t *b)
{
    int da=fb_is_dir(a),db=fb_is_dir(b);
    if(da!=db)return da?-1:1;                 /* directories first */
    if(fb_sort==1u){                          /* date: newest first */
        if(a->datetime!=b->datetime)return a->datetime>b->datetime?-1:1;
    }else if(fb_sort==2u){                    /* extension, then name */
        int c=fb_ci(fb_ext(a->name),fb_ext(b->name));
        if(c)return c;
    }
    return fb_ci(a->name,b->name);
}
static void fb_sort_all(void)
{
    unsigned i,j;
    for(i=1;i<fb_total;++i){
        dir_ent_t k;
        fb_copy_ent(&k,&fb_all[i]);
        j=i;
        while(j&&fb_cmp(&k,&fb_all[j-1u])<0){fb_copy_ent(&fb_all[j],&fb_all[j-1u]);--j;}
        fb_copy_ent(&fb_all[j],&k);
    }
}
static void fb_page_calc(void)
{
    if(fb_total<=FB_MAXROWS){
        /* Cala lista naraz: przewija WM (pasek), aplikacja nie stronnicuje. */
        fb_page=0;fb_count=fb_total;fb_more=0;
    }else{
        if(fb_page*FB_ROWS>=fb_total)fb_page=0;
        fb_count=(fb_total-fb_page*FB_ROWS>FB_ROWS)?FB_ROWS:(fb_total-fb_page*FB_ROWS);
        fb_more=((fb_page+1u)*FB_ROWS<fb_total);
    }
    fb_sel=-1;
}
/* Read up to FB_TOTAL entries (the kernel caps one call at 32), sort them and
   show the first page.  Sorting the whole directory, not just the page, is why
   the app keeps them all. */
static int fb_load(const char *path)
{
    uint32_t got=0,i;
    for(i=0;i<63u;++i){fb_path[i]=path[i];if(!path[i])break;}
    fb_path[i]=0;
    fb_total=0;fb_page=0;
    while(got<FB_TOTAL){
        uint32_t want=FB_TOTAL-got;int32_t r;
        if(want>FB_CHUNK)want=FB_CHUNK;
        r=gui_call(SYS_DIR_PAGE,got,want,(void *)fb_path,(void *)&fb_all[got]);
        if(r<=0)break;
        got+=(uint32_t)r;
        if((uint32_t)r<want)break;
    }
    fb_total=got;
    fb_sort_all();
    fb_page_calc();
    return fb_total!=0u;
}
static void fb_open_at(const char *path,const char *title)
{
    unsigned i;
    for(i=0;i<23u&&title[i];++i)fb_title[i]=title[i];
    fb_title[i]=0;
    fb_page=0;
    (void)fb_load(path);
    /* Rozmiar z konfiguracji (albo domyslny) musi pasowac do biezacego ekranu. */
    win_clamp(&fb_ww,&fb_wh,200u,120u);
    /* Always (re)declare the geometry: the compositor may have dropped the
       window (its X gadget) and would otherwise register a full-screen canvas
       for a handle it has no rectangle for. */
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)fb_wx<<16)|(uint16_t)fb_wy,
              ((uint32_t)fb_ww<<16)|fb_wh,FBW);
    (void)gfx(21,(uint32_t)(uintptr_t)fb_title,0,FBW);
    fb_open=1;
    fb_paint();
}
/* ---- Explorer activation: single click selects, double click opens -------- */
#define DBL_TICKS 12000u    /* ~0.98 s at 12288 hires counts/s (forgiving) */
static uint32_t fb_click_tick;
static int fb_click_row=-1;
/* True when this press completes a double-click on the same list row. */
static int fb_double(int row)
{
    uint32_t now=(uint32_t)hires();
    int dbl=(fb_click_row==row)&&((uint32_t)(now-fb_click_tick)<DBL_TICKS);
    fb_click_row=row;fb_click_tick=now;
    return dbl;
}
/* Case-insensitive 3-letter extension test. */
static int fb_ext_is(const char *e,const char *want)
{
    unsigned i;
    for(i=0;i<3u;++i){
        char c=e[i];
        if(!c)return 0;
        if(c>='a'&&c<='z')c=(char)(c-32);
        if(c!=want[i])return 0;
    }
    return e[3]==0;
}
/* Programs the explorer may launch. */
static int fb_runnable(const char *name)
{
    const char *dot=0;unsigned i;
    for(i=0;name[i]&&i<13u;++i)if(name[i]=='.')dot=&name[i+1];
    if(!dot)return 0;
    return fb_ext_is(dot,"PRG")||fb_ext_is(dot,"APP")||fb_ext_is(dot,"TOS")||
           fb_ext_is(dot,"TTP")||fb_ext_is(dot,"GTP");
}
/* Full path of a file-window row (row 1..fb_count). */
static void fb_full_path(char *out,int row)
{
    unsigned i,j;const dir_ent_t *e;
    out[0]=0;
    if(row<1||row>(int)fb_count)return;
    e=&fb_all[fb_page*FB_ROWS+(unsigned)(row-1)];
    for(i=0;fb_path[i]&&i<60u;++i)out[i]=fb_path[i];
    if(i&&out[i-1]!='\\')out[i++]='\\';
    for(j=0;j<12u&&e->name[j]&&i<62u;++j)out[i++]=e->name[j];
    out[i]=0;
}
static int tos_program_path(const char *path);
/* Launch the current directory's file by its full path. */
static void fb_run(const char *name)
{
    gui_spawn_t sp;unsigned i=0,j;int32_t r;
    zero(&sp,sizeof sp);
    for(j=0;fb_path[j]&&i<60u;++j)sp.path[i++]=fb_path[j];
    if(i&&sp.path[i-1]!='\\')sp.path[i++]='\\';
    for(j=0;name[j]&&i<62u;++j)sp.path[i++]=(char)name[j];
    sp.path[i]=0;
    if(tos_program_path(sp.path))r=gui_call(ATOS_CALL_TOS_RUN,1,0,(void *)(uintptr_t)sp.path,0);
    else r=gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
    show_notice(r<0?"RUN FAILED":"STARTED");
}
/* Open the row: ".." goes up, a directory is entered, a program is launched.
   A plain file that is not runnable is only selected (single click already did
   that, so nothing more happens). */
static void fb_activate(int row)
{
    char next[64];unsigned i,j;
    if(row<0)return;
    if(row==0){
        for(i=0;fb_path[i];++i){}
        while(i>1&&fb_path[i-1]!='\\')--i;
        for(j=0;j<i&&j<63u;++j)next[j]=fb_path[j];
        next[j]=0;
        if(!next[0]){next[0]='\\';next[1]=0;}
    }else if(row<=(int)fb_count){
        const dir_ent_t *e=&fb_all[fb_page*FB_ROWS+row-1];
        if(!(e->attributes&FB_ATTR_DIR)){
            fb_sel=row;fb_paint();
            if(fb_runnable(e->name))fb_run(e->name);
            return;
        }
        for(i=0;fb_path[i]&&i<60u;++i)next[i]=fb_path[i];
        if(i&&next[i-1]!='\\')next[i++]='\\';
        for(j=0;j<12u&&e->name[j]&&i<62u;++j)next[i++]=e->name[j];
        next[i]=0;
    }else{
        if(fb_more&&row==(int)fb_count+1){++fb_page;fb_page_calc();fb_paint();}
        return;
    }
    (void)fb_load(next);
    fb_paint();
}

/* ---- TRASH window: restore / delete / empty ------------------------------- */
/* Bucket path \TRASH\<bucket> (bucket names come straight from the listing). */
static unsigned tr_bucket_path(char *out,unsigned i)
{
    const char *p="\\TRASH\\";unsigned k=0,j;
    for(j=0;p[j];++j)out[k++]=p[j];
    for(j=0;j<12u&&tr_bucket[i][j];++j)out[k++]=tr_bucket[i][j];
    out[k]=0;
    return k;
}
/* Item path \TRASH\<bucket>\<name>. */
static void tr_item_path(char *out,unsigned i)
{
    unsigned k=tr_bucket_path(out,i),j;
    out[k++]='\\';
    for(j=0;j<12u&&tr_name[i][j];++j)out[k++]=tr_name[i][j];
    out[k]=0;
}
/* Re-read \TRASH.  kfat_trash_path() moves <name> into a fresh bucket, so every
   bucket holds exactly one entry - the item to show. */
static void tr_load(void)
{
    unsigned i,j;uint32_t got=0;
    tr_count=0;
    for(;;){
        int32_t r=gui_call(SYS_DIR_PAGE,got,TR_MAX,(void *)"\\TRASH",(void *)tr_ents);
        if(r<=0)break;
        for(i=0;i<(unsigned)r&&tr_count<TR_MAX;++i){
            if(!(tr_ents[i].attributes&FB_ATTR_DIR))continue;
            for(j=0;j<12u&&tr_ents[i].name[j];++j)tr_bucket[tr_count][j]=tr_ents[i].name[j];
            tr_bucket[tr_count][j]=0;
            tr_name[tr_count][0]=0;tr_size[tr_count]=0;tr_isdir[tr_count]=0;
            {char path[48];
             tr_bucket_path(path,tr_count);
             /* RESTORE moves the item out but leaves the bucket on the disk
                until it is purged.  Do not show such an empty bucket as a
                phantom item named after itself. */
             if(gui_call(SYS_DIR_PAGE,0,2,(void *)path,(void *)tr_inner)<=0)continue;
             for(j=0;j<12u&&tr_inner[0].name[j];++j)tr_name[tr_count][j]=tr_inner[0].name[j];
             tr_name[tr_count][j]=0;
             tr_size[tr_count]=tr_inner[0].size;
             tr_isdir[tr_count]=(tr_inner[0].attributes&FB_ATTR_DIR)?1u:0u;
             if(!tr_name[tr_count][0])continue;}
            ++tr_count;
        }
        got+=(uint32_t)r;
        if((uint32_t)r<TR_MAX||tr_count>=TR_MAX)break;
    }
    if(tr_sel>=(int)tr_count)tr_sel=-1;
}
/* Row index under a screen y, or -1 (rows start at client y=24). */
static int tr_row_at(int my)
{
    int sy=my-(int)tr_wy-16;
    if(!trash_open||sy<24)return -1;
    return (sy-24)/TR_ROW_H;
}
static void tr_paint(void)
{
    unsigned i;int16_t y=24;
    n=0;
    {uint16_t bg_h=(uint16_t)((uint32_t)(tr_wh-16)>TR_DOC_H?(uint32_t)(tr_wh-16):TR_DOC_H);
     add(GUI_FILL,0,0,tr_ww,bg_h,1,0);}
    add(GUI_TEXT,8,4,0,0,0,"TRASH");
    if(tr_msg[0])add(GUI_TEXT,92,4,0,0,0,tr_msg);
    if(!tr_count)add(GUI_TEXT,16,32,0,0,0,"EMPTY - NOTHING TO RESTORE");
    for(i=0;i<tr_count;++i){
        char row[34];unsigned k;
        for(k=0;k<31u;++k)row[k]=' ';
        row[31]=0;
        for(k=0;k<12u&&tr_name[i][k];++k)row[k]=tr_name[i][k];
        if(tr_isdir[i]){row[13]='<';row[14]='D';row[15]='I';row[16]='R';row[17]='>';}
        else fb_num(row+20,tr_size[i],10u);
        if((int)i==tr_sel)add(GUI_FILL,4,y,(uint16_t)(tr_ww-8),(uint16_t)(TR_ROW_H-2),0,0);
        add(GUI_TEXT,8,y,0,0,0,row);
        if((int)i==tr_sel){list[n-1u].color=1;list[n-1u].background=0;}
        add(GUI_TEXT,8+14*8,y,0,0,0,tr_bucket[i]);
        if((int)i==tr_sel){list[n-1u].color=1;list[n-1u].background=0;}
        y=(int16_t)(y+TR_ROW_H);
    }
    add(GUI_BEVEL,8,240,104,20,0,0);
    add(GUI_TEXT,20,241,0,0,0,"RESTORE");
    add(GUI_BEVEL,120,240,104,20,0,0);
    add(GUI_TEXT,132,241,0,0,0,"DELETE");
    add(GUI_BEVEL,232,240,104,20,0,0);
    add(GUI_TEXT,244,241,0,0,0,"EMPTY ALL");
    add(GUI_TEXT,8,266,0,0,0,"DBL-CLICK ROW = RESTORE TO C:");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,TRW);
    (void)gfx(29,(uint32_t)(uint16_t)TR_DOC_H,0,TRW);
}
static void tr_restore(void)
{
    char src[48],tgt[16];unsigned j;int32_t r;
    if(tr_sel<0||(unsigned)tr_sel>=tr_count){
        copytext(tr_msg,"SELECT AN ITEM",sizeof(tr_msg));tr_paint();return;}
    tr_item_path(src,(unsigned)tr_sel);
    tgt[0]='\\';
    for(j=0;j<12u&&tr_name[tr_sel][j];++j)tgt[1+j]=tr_name[tr_sel][j];
    tgt[1+j]=0;
    r=gui_call(SYS_MOVE,0,0,(void *)src,(void *)tgt);
    copytext(tr_msg,r<0?"RESTORE FAILED (NAME IN USE?)":"RESTORED TO C:",sizeof(tr_msg));
    if(r>=0){char path[48];                  /* drop the bucket the move emptied */
        tr_bucket_path(path,(unsigned)tr_sel);
        (void)gui_call(SYS_PURGE,0,0,(void *)path,0);}
    tr_load();tr_sel=-1;tr_paint();
}
static void tr_delete(void)
{
    char path[48];int32_t r;
    if(tr_sel<0||(unsigned)tr_sel>=tr_count){
        copytext(tr_msg,"SELECT AN ITEM",sizeof(tr_msg));tr_paint();return;}
    tr_bucket_path(path,(unsigned)tr_sel);
    r=gui_call(SYS_PURGE,0,0,(void *)path,0);
    copytext(tr_msg,r<0?"DELETE FAILED":"DELETED",sizeof(tr_msg));
    tr_load();tr_sel=-1;tr_paint();
}
static void tr_empty(void)
{
    unsigned i;int32_t r=0;char path[48];
    if(!tr_count){copytext(tr_msg,"ALREADY EMPTY",sizeof(tr_msg));tr_paint();return;}
    for(i=0;i<tr_count;++i){
        tr_bucket_path(path,i);
        if(gui_call(SYS_PURGE,0,0,(void *)path,0)<0)r=-1;
    }
    copytext(tr_msg,r<0?"PURGE INCOMPLETE":"TRASH EMPTIED",sizeof(tr_msg));
    tr_load();tr_sel=-1;tr_paint();
}
static void tr_open_it(void)
{
    win_clamp(&tr_ww,&tr_wh,360u,300u);
    tr_wx=(int16_t)(((int)SCRW-(int)tr_ww)/2);if(tr_wx<16)tr_wx=16;
    tr_wy=(int16_t)(((int)SCRH-(int)tr_wh)/2);if(tr_wy<16)tr_wy=16;
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)tr_wx<<16)|(uint16_t)tr_wy,
              ((uint32_t)tr_ww<<16)|tr_wh,TRW);
    (void)gfx(21,(uint32_t)(uintptr_t)"TRASH",0,TRW);
    trash_open=1;tr_sel=-1;tr_msg[0]=0;
    tr_load();
    tr_paint();
}
static void tr_click(int mx,int my)
{
    int sy=my-(int)tr_wy-16;
    if(sy>=240&&sy<260){
        int sx=mx-(int)tr_wx;
        if(sx>=8&&sx<112)tr_restore();
        else if(sx>=120&&sx<224)tr_delete();
        else if(sx>=232&&sx<336)tr_empty();
        return;
    }
    {int row=tr_row_at(my);
     if(row>=0&&(unsigned)row<tr_count){
         if(fb_double(row+TR_MAX))tr_restore();   /* double click = restore */
         else{tr_sel=row;tr_paint();}
     }}
}

/* ---- My Atari window: list the drives and open one ---------------------- */
static int ma_refresh(void)
{
    int r=gui_call(56,sizeof(ma_info),0,&ma_info,0);
    if(r||ma_info.magic!=SYSREPORT_MAGIC||ma_info.version!=SYSREPORT_VERSION||
       ma_info.size!=sizeof(ma_info)||ma_info.volumes>SYSREPORT_VOLUMES)return -1;
    ma_count=ma_info.volumes;
    return 0;
}
static void ma_path(char *out,unsigned i)
{
    char letter=(char)('C'+(int)i);
    if(letter=='C'){out[0]='\\';out[1]=0;}
    else{out[0]=letter;out[1]=':';out[2]='\\';out[3]=0;}
}
static void ma_paint(void)
{
    unsigned i;int16_t y=28;
    n=0;
    add(GUI_FILL,0,0,ma_ww,(uint16_t)(ma_wh-16),1,0);
    add(GUI_TEXT,8,4,0,0,0,"MY ATARI - DRIVES");
    if(ma_msg[0])add(GUI_TEXT,152,4,0,0,0,ma_msg);
    if(!ma_count)add(GUI_TEXT,16,40,0,0,0,"NO DRIVES FOUND");
    for(i=0;i<ma_count&&i<6u;++i){
        char row[26];unsigned k;
        uint8_t icon=(i==0u)?(uint8_t)ICON_DYSK:(uint8_t)ICON_DYSKIETKA;
        row[0]=(char)('C'+(int)i);row[1]=':';
        for(k=2;k<24u;++k)row[k]=' ';
        for(k=0;k<4u&&ma_info.volume[i].id[k];++k)row[2+k]=ma_info.volume[i].id[k];
        row[24]=0;
        if((int)i==ma_sel){add(GUI_FILL,4,y,(uint16_t)(ma_ww-8),(uint16_t)(MA_ROW_H-2),0,0);}
        add(GUI_ICON_RGB,8,y,(uint16_t)icon_px,(uint16_t)icon_px,icon,"");
        list[n-1u].reserved=0;
        add(GUI_TEXT,48,(int16_t)(y+8),0,0,0,row);
        if((int)i==ma_sel){list[n-1u].color=1;list[n-1u].background=0;}
        y=(int16_t)(y+MA_ROW_H);
    }
    add(GUI_TEXT,8,(int16_t)(ma_wh-28),0,0,0,"DBL-CLICK = OPEN DRIVE");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,MAW);
    (void)gfx(29,(uint32_t)(uint16_t)(ma_wh-16),0,MAW);
}
static int ma_row_at(int my)
{
    int sy=my-(int)ma_wy-16;
    if(!ma_open||sy<28)return -1;
    return (sy-28)/MA_ROW_H;
}
static void ma_open_it(void)
{
    /* Wysokosc okna zalezy od rozmiaru ikony (wiersz = icon_px+4), inaczej przy
       64 px trzy ostatnie napedz znikalyby pod dolna krawedzia. */
    ma_wh=(uint16_t)(28+(int)MA_ROW_H*6+56);
    if((int)ma_wh>(int)SCRH-32)ma_wh=(uint16_t)((int)SCRH-32);
    ma_wx=(int16_t)(((int)SCRW-(int)ma_ww)/2);if(ma_wx<16)ma_wx=16;
    ma_wy=(int16_t)(((int)SCRH-(int)ma_wh)/2);if(ma_wy<16)ma_wy=16;
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)ma_wx<<16)|(uint16_t)ma_wy,
              ((uint32_t)ma_ww<<16)|ma_wh,MAW);
    (void)gfx(21,(uint32_t)(uintptr_t)"MY ATARI",0,MAW);
    ma_open=1;ma_sel=-1;ma_msg[0]=0;
    if(ma_refresh())copytext(ma_msg,"NO DRIVE INFO",sizeof(ma_msg));
    ma_paint();
}
static void ma_click(int mx,int my)
{
    int row=ma_row_at(my);(void)mx;
    /* A drive list has nothing to "select": one click opens the drive. */
    if(row>=0&&(unsigned)row<ma_count){
        char path[8],title[13];
        ma_path(path,(unsigned)row);
        title[0]=(char)('C'+(int)row);title[1]=':';
        {unsigned k;for(k=0;k<4u&&ma_info.volume[row].id[k];++k)title[2+k]=ma_info.volume[row].id[k];title[2+k]=0;}
        fb_open_at(path,title);
    }
}

/* ---- right-click context menu + info popup (Windows-style) -------------- */
static void ctx_arrange(void)
{
    unsigned i,idx=0;int maxc=(int)((SCRW-GRID_X)/CELL_W);
    if(maxc<1)maxc=1;
    for(i=0;i<icon_count;++i){
        icons[i].gx=(int8_t)(idx%(unsigned)maxc);
        icons[i].gy=(int8_t)(idx/(unsigned)maxc);
        ++idx;
    }
    bg_dirty=1;
    paint();
}

/* INFO: a small one-shot popup; dismissed by the next click anywhere. */
static uint8_t info_open;
static int16_t info_x,info_y;
static char info_l[4][40];
static void info_show(int mx,int my,const char *a,const char *b,const char *c,const char *d)
{
    copytext(info_l[0],a,40);copytext(info_l[1],b,40);
    copytext(info_l[2],c,40);copytext(info_l[3],d,40);
    info_x=(int16_t)mx;if(info_x+212>(int)SCRW)info_x=(int16_t)((int)SCRW-212);if(info_x<0)info_x=0;
    info_y=(int16_t)my;if(info_y+88>(int)SCRH)info_y=(int16_t)((int)SCRH-88);if(info_y<0)info_y=0;
    info_open=1;
}
static void info_paint(void)
{
    unsigned i;n=0;
    add(GUI_FILL,info_x,info_y,212,88,1,0);
    add(GUI_FRAME,info_x,info_y,212,88,0,0);
    for(i=0;i<4u;++i)if(info_l[i][0])add(GUI_TEXT,(int16_t)(info_x+6),(int16_t)(info_y+4+(int)i*16),0,0,0,info_l[i]);
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)info_x<<16)|(uint16_t)info_y,
              ((uint32_t)212u<<16)|88u,1u);
}
static void ctx_paint(void)
{
    unsigned i;uint16_t h=(uint16_t)(ctx_items*CTX_ROW+4);
    n=0;
    add(GUI_FILL,ctx_x,ctx_y,CTX_W,h,1,0);
    add(GUI_FRAME,ctx_x,ctx_y,CTX_W,h,0,0);
    for(i=0;i<(unsigned)ctx_items;++i){
        int16_t y=(int16_t)(ctx_y+2+(int)i*CTX_ROW);
        if((int)i==ctx_sel){
            add(GUI_FILL,(int16_t)(ctx_x+2),y,(uint16_t)(CTX_W-4),(uint16_t)(CTX_ROW-2),0,0);
            add(GUI_TEXT,(int16_t)(ctx_x+6),y,0,0,1,ctx_label[i]);
            list[n-1u].background=0;
        }else
            add(GUI_TEXT,(int16_t)(ctx_x+6),y,0,0,0,ctx_label[i]);
    }
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    (void)gfx(GFX_OVERLAY,((uint32_t)(uint16_t)ctx_x<<16)|(uint16_t)ctx_y,
              ((uint32_t)CTX_W<<16)|h,1u);
    if(info_open)info_paint();
}

/* File-window entry for the context menu (row 1..fb_count). */
static const dir_ent_t *ctx_entry(void)
{
    if(ctx_what!=2u||ctx_file_row<1||ctx_file_row>(int)fb_count)return 0;
    return &fb_all[fb_page*FB_ROWS+(unsigned)(ctx_file_row-1)];
}
static int ctx_is_dir(void){const dir_ent_t *e=ctx_entry();return e?(e->attributes&FB_ATTR_DIR)!=0:0;}

static void ctx_copy(void)
{
    if(ctx_what==2u){
        char p[64];const dir_ent_t *e=ctx_entry();
        fb_full_path(p,ctx_file_row);
        copytext(clip_src,p,64);
        if(e){unsigned j;for(j=0;j<12u;++j)clip_name[j]=e->name[j];clip_name[j]=0;}
        clip_set=1;
        copytext(status,"COPIED",sizeof(status));
    }
}
static void ctx_paste(void)
{
    if(!clip_set)return;
    {char tgt[64];unsigned i,j;
     for(i=0;fb_path[i]&&i<60u;++i)tgt[i]=fb_path[i];
     if(i&&tgt[i-1]!='\\')tgt[i++]='\\';
     for(j=0;j<12u&&clip_name[j]&&i<62u;++j)tgt[i++]=clip_name[j];
     tgt[i]=0;
     {int32_t r=gui_call(SYS_COPY,0,0,(void *)clip_src,(void *)tgt);
      copytext(status,r<0?"PASTE FAILED":"PASTED",sizeof(status));
      if(r>=0&&fb_open){(void)fb_load(fb_path);fb_paint();}}}
    paint();
}
static void ctx_delete(void)
{
    if(ctx_what==1u){
        /* Desktop program shortcut: drop it from the launcher list. */
        if(ctx_icon>=0&&icons[ctx_icon].li>=0){
            launch[icons[ctx_icon].li].builtin=0;   /* mark removable? no-op: keep simple */
        }
        copytext(status,"DELETE (DESKTOP)",sizeof(status));
    }else if(ctx_what==2u){
        char p[64];fb_full_path(p,ctx_file_row);
        {int32_t r=gui_call(SYS_TRASH,0,0,(void *)p,0);
         copytext(status,r<0?"DELETE FAILED":"MOVED TO TRASH",sizeof(status));
         if(r>=0&&fb_open){(void)fb_load(fb_path);fb_paint();}}
    }
    paint();
}
static void ctx_info(void)
{
    if(ctx_what==1u&&ctx_icon>=0){
        char a[40],b[40],c[40];
        copytext(a,icons[ctx_icon].label,40);
        copytext(b,icons[ctx_icon].builtin==5u?"FOLDER (COMPUTER)":
                  icons[ctx_icon].res==3u?"SYSTEM TRASH":
                  icons[ctx_icon].builtin==1u?"SYSTEM APP":
                  icons[ctx_icon].builtin==0u?"APPLICATION":"ICON",40);
        c[0]=0;copytext(c,"POSITION: ",40);c[9]=(char)('0'+icons[ctx_icon].gx);c[10]=',';c[11]=(char)('0'+icons[ctx_icon].gy);c[12]=0;
        info_show(ctx_x,ctx_y,a,b,c,"");
    }else if(ctx_what==2u){
        const dir_ent_t *e=ctx_entry();
        char a[40],b[40],c[40];
        if(e){
            copytext(a,e->name,40);
            copytext(b,ctx_is_dir()?"TYPE: DIRECTORY":"TYPE: FILE",40);
            c[0]=0;if(!ctx_is_dir()){copytext(c,"SIZE: ",40);fb_num(c+6,e->size,10u);}
            info_show(ctx_x,ctx_y,a,b,c,"");
        }
    }
}
static void icp_open_it(int icon,int file_row);
static void ctx_activate(void)
{
    ctx_open=0;bg_dirty=1;
    (void)gfx(GFX_OVERLAY,0,0,0);
    if(ctx_sel<0||ctx_sel>=ctx_items){paint();return;}
    if(ctx_what==0u){
        if(ctx_sel==0){bg_dirty=1;}                          /* refresh */
        else if(ctx_sel==1)ctx_arrange();                    /* arrange */
        else if(ctx_sel==2)desktop_save();                   /* save */
        else if(ctx_sel==3)ctx_paste();                      /* paste */
    }else if(ctx_what==1u){
        if(ctx_sel==0&&ctx_icon>=0)icon_activate(ctx_icon);  /* open */
        else if(ctx_sel==1)ctx_info();                       /* info */
        else if(ctx_sel==2)ctx_delete();                     /* delete */
        else if(ctx_sel==3)icp_open_it(ctx_icon,-1);            /* icon change */
    }else if(ctx_what==2u){
        if(ctx_sel==0&&ctx_file_row>0)fb_activate(ctx_file_row);      /* open */
        else if(ctx_sel==1)ctx_info();                       /* info */
        else if(ctx_sel==2)ctx_copy();                       /* copy */
        else if(ctx_sel==3)ctx_delete();                     /* delete */
        else if(ctx_sel==4)icp_open_it(-1,ctx_file_row);        /* icon change */
    }
    ctx_open=0;
    paint();
    if(info_open)info_paint();
}
static void ctx_open_at(int mx,int my,uint8_t what,int icon,int file_row)
{
    unsigned k=0;
    ctx_what=what;ctx_icon=icon;ctx_file_row=file_row;
    if(what==0u){
        ctx_label[k++]="REFRESH";
        ctx_label[k++]="ARRANGE ICONS";
        ctx_label[k++]="SAVE DESKTOP";
        if(clip_set)ctx_label[k++]="PASTE";
    }else if(what==1u){
        ctx_label[k++]="OPEN";
        ctx_label[k++]="INFO";
        ctx_label[k++]="DELETE";
        ctx_label[k++]="ICON CHANGE";
    }else{
        ctx_label[k++]="OPEN";
        ctx_label[k++]="INFO";
        ctx_label[k++]="COPY";
        ctx_label[k++]="DELETE";
        ctx_label[k++]="ICON CHANGE";
    }
    ctx_items=(int)k;
    ctx_x=(int16_t)mx;ctx_y=(int16_t)my;
    if(ctx_x+CTX_W>(int)SCRW)ctx_x=(int16_t)((int)SCRW-CTX_W);
    if(ctx_x<0)ctx_x=0;
    if(ctx_y+(int)ctx_items*CTX_ROW+4>(int)SCRH)ctx_y=(int16_t)((int)SCRH-(int)ctx_items*CTX_ROW-4);
    if(ctx_y<0)ctx_y=0;
    ctx_sel=-1;ctx_open=1;
    ctx_paint();
}
/* Close the popup and restore only its rectangle (no full-background repaint):
   clear the popup rect, then repaint the chrome + icons over it. */
static void ctx_close_popup(void)
{
    uint16_t h=(uint16_t)(ctx_items*CTX_ROW+4);
    (void)gfx(GFX_OVERLAY,0,0,0);
    ctx_open=0;
    n=0;
    add(GUI_BG,ctx_x,ctx_y,CTX_W,h,0,0);
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    paint();
}

/* ---- icon picker (ICON CHANGE) + type->icon mapping ---------------------- */
/* Stable, prefixed keys so a user assignment never collides with a default:
     builtin:my-atari | builtin:trash | builtin:terminal | builtin:disk
     app:<full path>           (program shortcut)
     ext:<EXT>                 (a file type, all files)
     dir:<NAME>                (a directory)
   The value is a decimal icon id (not a single '0'+id char). */
#define IC_MAP_MAX 16
#define IC_KEY_LEN 32
static char ic_key[IC_MAP_MAX][IC_KEY_LEN];
static uint8_t ic_id[IC_MAP_MAX];
static unsigned ic_map_count;

static int ic_find(const char *key)
{
    unsigned i;
    for(i=0;i<ic_map_count;++i)if(text_eq(ic_key[i],key))return (int)i;
    return -1;
}
static uint8_t ic_id_for(const char *key)
{
    int i=ic_find(key);
    if(i>=0)return ic_id[i];
    if(key[0]=='d')return (uint8_t)ICON_FOLDER;                /* dir: */
    if(key[0]=='e'){                                          /* ext: */
        /* The set has a semantic icon per file kind; giving every extension the
           document icon wasted most of it. */
        const char *x=key+4;
        if(text_eq(x,"TXT")||text_eq(x,"DOC"))return (uint8_t)ICON_PLIK_TEKSTOWY;
        /* TOS executables get the window-with-disk icon; .APP stays the native
           application gear.  Every .PRG used to look like an unidentified app. */
        if(text_eq(x,"PRG")||text_eq(x,"TOS")||text_eq(x,"TTP")||
           text_eq(x,"GTP"))return (uint8_t)ICON_TOS_PRG;
        if(text_eq(x,"APP"))return (uint8_t)ICON_SYSTEM;
        if(text_eq(x,"JPG")||text_eq(x,"JPEG")||text_eq(x,"PNG")||text_eq(x,"GIF")||
           text_eq(x,"BMP")||text_eq(x,"PCX"))return (uint8_t)ICON_OBRAZ;
        if(text_eq(x,"MOD")||text_eq(x,"SND")||text_eq(x,"WAV")||
           text_eq(x,"YM"))return (uint8_t)ICON_MUZYKA;
        if(text_eq(x,"ZIP")||text_eq(x,"LZH")||text_eq(x,"ARJ")||
           text_eq(x,"RAR")||text_eq(x,"GZ"))return (uint8_t)ICON_KOMPRESJA;
        if(text_eq(x,"ARC")||text_eq(x,"LHA"))return (uint8_t)ICON_ARCHIWUM;
        if(text_eq(x,"INF"))return (uint8_t)ICON_INFORMACJE;
        if(text_eq(x,"CFG")||text_eq(x,"SYS")||text_eq(x,"INI"))return (uint8_t)ICON_USTAWIENIA;
        if(text_eq(x,"HLP"))return (uint8_t)ICON_POMOC;
        if(text_eq(x,"FNT"))return (uint8_t)ICON_GRAFIKA;
        return (uint8_t)ICON_DOKUMENT;
    }
    if(key[0]=='b'){                                          /* builtin: */
        if(text_eq(key,"builtin:trash"))return (uint8_t)ICON_KOSZ;
        if(text_eq(key,"builtin:terminal"))return (uint8_t)ICON_TERMINAL;
        if(text_eq(key,"builtin:my-atari"))return (uint8_t)ICON_KOMPUTER;
        return (uint8_t)ICON_DYSK;
    }
    /* Known desktop programs get the icon that says what they are; the rest
       fall back to one generic entry, which is why everything looked the
       same gear before. */
    if(text_eq(key,"app:\\SYSTEM\\APP\\AC.APP"))return (uint8_t)ICON_NC;   /* file manager */
    if(text_eq(key,"app:\\SYSTEM\\APP\\SYSINFO.APP"))return (uint8_t)ICON_INFORMACJE;
    if(text_eq(key,"app:\\SYSTEM\\APP\\TASKMGR.APP"))return (uint8_t)ICON_KONFIGURACJA;
    if(text_eq(key,"app:\\SYSTEM\\APP\\PLASMA.APP"))return (uint8_t)ICON_OBRAZ;
    if(text_eq(key,"app:\\SYSTEM\\APP\\SETUP.APP"))return (uint8_t)ICON_USTAWIENIA;
    if(text_eq(key,"app:\\SYSTEM\\APP\\NOTE.APP"))return (uint8_t)ICON_EDYTOR;
    if(text_eq(key,"app:\\SYSTEM\\APP\\FLASHBK.APP"))return (uint8_t)ICON_PLATFORMOWE; /* REminiscence/Flashback (platformer) */
    if(text_eq(key,"app:\\SYSTEM\\APP\\COMMAND.APP"))return (uint8_t)ICON_TERMINAL;
    if(text_eq(key,"app:\\SYSTEM\\APP\\GFXBENCH.APP"))return (uint8_t)ICON_GRAFIKA;
    if(text_eq(key,"app:\\SYSTEM\\APP\\GFXTEST.APP"))return (uint8_t)ICON_GRAFIKA;
    if(text_eq(key,"app:\\SYSTEM\\APP\\MEMTEST.APP"))return (uint8_t)ICON_SYSTEM;
    if(text_eq(key,"app:\\SYSTEM\\APP\\DIRTEST.APP"))return (uint8_t)ICON_FOLDER;
    if(text_eq(key,"app:\\SYSTEM\\APP\\FILETEST.APP"))return (uint8_t)ICON_DOKUMENT;
    if(text_eq(key,"app:\\SYSTEM\\APP\\HW.APP"))return (uint8_t)ICON_MONITOR;
    if(text_eq(key,"app:\\SYSTEM\\APP\\GUITEST.APP"))return (uint8_t)ICON_USTAWIENIA;
    if(text_eq(key,"app:\\SYSTEM\\APP\\DESKTEST.APP"))return (uint8_t)ICON_KOMPUTER;
    if(text_eq(key,"app:\\SYSTEM\\APP\\MONODESK.APP"))return (uint8_t)ICON_KOMPUTER;
    if(text_eq(key,"app:\\SYSTEM\\APP\\HELLO.APP"))return (uint8_t)ICON_O_ATOS;
    /* LDW Power is a spreadsheet: give its desktop shortcut a fitting icon
       instead of the generic gear. */
    if(text_eq(key,"app:\\SYSTEM\\TEST\\LDW.PRG"))return (uint8_t)ICON_KALKULATOR;
    /* Any other TOS program reached from the desktop keeps the PRG icon instead
       of falling back to the generic application gear. */
    {int L=0;while(key[L])++L;
     if(L>4&&key[0]=='a'&&text_eq(key+L-4,".PRG"))return (uint8_t)ICON_TOS_PRG;}
    return (uint8_t)ICON_SYSTEM;                              /* other app: */
}
/* Stable identity key for a desktop icon (never grid index or label). */
static void icon_key(char *out,int i)
{
    if(icons[i].builtin==5u){copytext(out,"builtin:my-atari",IC_KEY_LEN);}
    else if(icons[i].res==3u){copytext(out,"builtin:trash",IC_KEY_LEN);}
    else if(icons[i].builtin==1u){copytext(out,"builtin:terminal",IC_KEY_LEN);}
    else if(icons[i].builtin==0u){
        const char *p=(icons[i].li>=0)?launch[icons[i].li].path:icons[i].label;
        unsigned k=0;out[k++]='a';out[k++]='p';out[k++]='p';out[k++]=':';
        while(p&&*p&&k<IC_KEY_LEN-1u)out[k++]=*p++;
        out[k]=0;
    }
    else copytext(out,"builtin:disk",IC_KEY_LEN);
}
static uint32_t cfg_len(const char *s){uint32_t n=0u;while(s&&s[n])++n;return n;}
/* Read a small config file WHOLE.  A single read() into a fixed buffer silently
   truncated anything longer than that buffer, and the damage was invisible: the
   ATOS LAUNCH.CFG here is 510 bytes while the reader used a 384-byte buffer, so
   the last line arrived as "TOSCOL=\SYSTEM".  The icon appeared on the desktop
   but could never start (the path was not a file), which is exactly the "I cannot
   run TOSCOL" report.  Loop until EOF, always NUL terminate, >=0 = bytes read. */
#define CFG_BUF_BYTES 2048u
static char *cfg_buf;                  /* heap: shared by the config readers */
static char *read_all(const char *path)      /* NULL when the file/heap is missing */
{
    uint32_t capacity=CFG_BUF_BYTES;
    char *buf;
    int32_t fd;uint32_t total=0u;
    if(!cfg_buf)cfg_buf=(char *)(uintptr_t)(uint32_t)gui_call(16,CFG_BUF_BYTES,1u,0,0);
    buf=cfg_buf;
    if(!buf)return 0;
    fd=gui_call(2,0,0,(void *)(uintptr_t)path,0);
    if(fd<0)return 0;
    while(total+1u<capacity){
        int32_t r=gui_call(3,(uint32_t)fd,capacity-1u-total,(void *)(buf+total),0);
        if(r<=0)break;
        total+=(uint32_t)r;
    }
    (void)gui_call(4,(uint32_t)fd,0,0,0);
    buf[total]=0;
    return buf;                            /* NUL terminated, whole file */
}
static void ic_load(void)
{
    char *buf;int32_t r;unsigned i=0;
    ic_map_count=0;
    buf=read_all("\\SYSTEM\\ICONS.CFG");
    if(!buf)return;
    r=(int32_t)cfg_len(buf);
    if(r<7)return;
    buf[r]=0;
    if(!(buf[0]=='I'&&buf[1]=='C'&&buf[2]=='O'&&buf[3]=='N'&&buf[4]=='S'&&buf[5]=='2'))return;
    while(i<(unsigned)r&&buf[i]!='\n')++i;
    if(i<(unsigned)r)++i;
    while(i<(unsigned)r&&ic_map_count<IC_MAP_MAX){
        char line[IC_KEY_LEN+8];unsigned l=0,eq=0;
        while(i<(unsigned)r&&buf[i]!='\n'&&l<sizeof(line)-1u)line[l++]=buf[i++];
        while(i<(unsigned)r&&buf[i]!='\n')++i;
        if(i<(unsigned)r)++i;
        line[l]=0;
        for(eq=0;eq<l&&line[eq]!='=';++eq){}
        if(eq<1||eq>=IC_KEY_LEN)continue;
        line[eq]=0;
        {unsigned v=0,valid=0,k;
         for(k=eq+1;k<l&&line[k]>='0'&&line[k]<='9';++k){v=v*10u+(unsigned)(line[k]-'0');valid=1;}
         if(!valid||v>=ICON_COLOR_COUNT)continue;
         copytext(ic_key[ic_map_count],line,IC_KEY_LEN);
         ic_id[ic_map_count]=(uint8_t)v;
         ++ic_map_count;}
    }
}
static void ic_save(void)
{
    char buf[512];unsigned i,len=0,k;
    buf[len++]='I';buf[len++]='C';buf[len++]='O';buf[len++]='N';buf[len++]='S';buf[len++]='2';buf[len++]='\n';
    for(i=0;i<ic_map_count&&len<480u;++i){
        for(k=0;ic_key[i][k]&&len<480u;++k)buf[len++]=ic_key[i][k];
        if(len<480u)buf[len++]='=';
        {char num[4];unsigned v=ic_id[i],d=0;
         if(v>=100u){num[d++]=(char)('0'+v/100u);v%=100u;}
         if(v>=10u||d)num[d++]=(char)('0'+v/10u);
         num[d++]=(char)('0'+v%10u);num[d]=0;
         for(k=0;num[k]&&len<480u;++k)buf[len++]=num[k];}
        if(len<480u)buf[len++]='\n';
    }
    (void)gui_call(60,(uint32_t)len,0,(void *)"\\SYSTEM\\ICONS.CFG",(void *)buf);
}

/* ---- dynamic per-app icons (foo.png next to foo.app) -------------------- */
#define DYN_ICONS 4u
static uint8_t dyn_li[DYN_ICONS];   /* launch index of each dynamic icon */
static unsigned dyn_icon_count;

/* Load foo.png for an app, convert to the shared palette, register it with the
   kernel and return its dynamic id (ICON_COLOR_COUNT + slot), or -1. */
static int load_app_icon(const char *app_path,uint8_t li)
{
    char png_path[64];unsigned i,lastdot=0;
    int32_t fd,r;uint8_t *png;int ret=-1;
    for(i=0;app_path[i]&&i<60u;++i){png_path[i]=app_path[i];if(app_path[i]=='.')lastdot=i;}
    if(lastdot){png_path[lastdot+1]='P';png_path[lastdot+2]='N';png_path[lastdot+3]='G';png_path[lastdot+4]=0;}
    else{png_path[i]='.';png_path[i+1]='P';png_path[i+2]='N';png_path[i+3]='G';png_path[i+4]=0;}
    fd=gui_call(2,0,0,(void *)png_path,0);
    if(fd<0)return -1;
    png=(uint8_t *)(uintptr_t)(uint32_t)gui_call(16,16384u,0,0,0);
    if(png){
        r=gui_call(3,(uint32_t)fd,16384u,(void *)png,0);
        if(r>0&&gfx(36,(uint32_t)(uintptr_t)png,(uint32_t)r,(uint32_t)dyn_icon_count)>=0){
            dyn_li[dyn_icon_count]=li;
            ret=(int)(ICON_COLOR_COUNT+dyn_icon_count);++dyn_icon_count;
        }
        (void)gui_call(17,0,0,(void *)png,0);
    }
    (void)gui_call(4,(uint32_t)fd,0,0,0);
    return ret;
}
static void load_dyn_icons(void)
{
    unsigned i;
    dyn_icon_count=0;
    for(i=0;i<launch_count&&dyn_icon_count<DYN_ICONS;++i)
        if(launch[i].builtin==0u)(void)load_app_icon(launch[i].path,(uint8_t)i);
}

#define ICPW 7u
#define ICP_COLS 7u
#define ICP_ROWS 7u   /* 48 icons in 7 columns = 7 rows */
static uint8_t icp_open;
static int16_t icp_wx=140,icp_wy=60;
/* Picker cell = icon + 8px margin; the window follows the icon size. */
#define ICP_CELL ((int)icon_px+8)
static uint16_t icp_ww,icp_wh;   /* liczone przy otwarciu: zaleza od icon_px */
static char icp_key[IC_KEY_LEN];       /* stable key being changed */
static uint8_t icp_has_target;
static int icp_sel=-1;                 /* selected NORMAL icon id (odd) */
static int icp_dbl_id=-1;              /* icon of the previous click */
static uint32_t icp_dbl_tick;          /* double click on it saves straight away */
static void icp_paint(void)
{
    /* The desktop's draw list holds LIST_MAX items and add() DROPS whatever goes
       past it.  The picker needs 1 background + 1 title + one entry per icon +
       the two buttons, which is more than the list holds: the icon grid ran off
       the end and the SAVE/CANCEL row was never emitted at all.  Draw in chunks
       that each stay inside the limit rather than growing the list, because the
       desktop is close to its -mpcrel budget. */
    unsigned done=0,total=ICON_COLOR_COUNT/2u,per=ICP_COLS*3u;
    int16_t x,y;
    n=0;
    add(GUI_FILL,0,0,icp_ww,(uint16_t)(icp_wh-16),1,0);
    add(GUI_TEXT,8,4,0,0,0,"CHOOSE ICON");
    while(done<total){
        unsigned k;
        for(k=0;k<per&&done<total;++k,++done){
            unsigned col=done%ICP_COLS,row=done/ICP_COLS;
            uint8_t id=(uint8_t)(done*2u);
            x=(int16_t)(4+(int)col*ICP_CELL);y=(int16_t)(24+(int)row*ICP_CELL);
            add(GUI_ICON_RGB,x,y,(uint16_t)icon_px,(uint16_t)icon_px,id,"");
            list[n-1u].reserved=0;
            if((int)id==icp_sel)
                add(GUI_FRAME,(int16_t)(x-2),(int16_t)(y-2),
                    (uint16_t)(icon_px+4),(uint16_t)(icon_px+4),0,0);
        }
        (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,ICPW);
        n=0;
    }
    /* SAVE / CANCEL in their own batch: the grid can never push them out. */
    add(GUI_BEVEL,8,(int16_t)(icp_wh-44),92,20,0,0);
    add(GUI_TEXT,22,(int16_t)(icp_wh-41),0,0,0,"SAVE");
    add(GUI_BEVEL,112,(int16_t)(icp_wh-44),92,20,0,0);
    add(GUI_TEXT,122,(int16_t)(icp_wh-41),0,0,0,"CANCEL");
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,ICPW);
    (void)gfx(29,(uint32_t)(uint16_t)(icp_wh-16),0,ICPW);
}
static void icp_open_it(int icon,int file_row)
{
    /* Okno pickera rosnie z rozmiarem ikony - przelicz PRZED rysowaniem. */
    icp_ww=(uint16_t)(4+ICP_COLS*ICP_CELL+8);
    icp_wh=(uint16_t)(24+ICP_ROWS*ICP_CELL+48);
    icp_has_target=0;
    if(icon>=0){
        icon_key(icp_key,icon);icp_has_target=1;
    }else if(file_row>0&&file_row<=(int)fb_count){
        const dir_ent_t *e=&fb_all[fb_page*FB_ROWS+(unsigned)(file_row-1)];
        int isdir=(e->attributes&FB_ATTR_DIR)!=0;
        if(isdir){
            unsigned k=0,j=0;copytext(icp_key,"dir:",IC_KEY_LEN);k=4;
            while(e->name[j]&&k<IC_KEY_LEN-1u)icp_key[k++]=e->name[j++];
            icp_key[k]=0;
        }else{
            const char *x=fb_ext(e->name);unsigned k=0,j=0;
            copytext(icp_key,"ext:",IC_KEY_LEN);k=4;
            while(x[j]&&k<IC_KEY_LEN-1u)icp_key[k++]=x[j++];
            icp_key[k]=0;
        }
        icp_has_target=1;
    }
    icp_wx=(int16_t)(((int)SCRW-(int)icp_ww)/2);if(icp_wx<16)icp_wx=16;
    icp_wy=(int16_t)(((int)SCRH-(int)icp_wh)/2);if(icp_wy<16)icp_wy=16;
    (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)icp_wx<<16)|(uint16_t)icp_wy,
              ((uint32_t)icp_ww<<16)|icp_wh,ICPW);
    (void)gfx(21,(uint32_t)(uintptr_t)"ICON CHANGE",0,ICPW);
    icp_open=1;icp_sel=-1;
    if(icp_has_target){int mi=ic_find(icp_key);if(mi>=0)icp_sel=(int)ic_id[mi];}
    icp_paint();
}
static void icp_assign(uint8_t id)
{
    if(!icp_has_target)return;
    {int idx=ic_find(icp_key);
     if(idx<0&&ic_map_count<IC_MAP_MAX){idx=(int)ic_map_count++;copytext(ic_key[idx],icp_key,IC_KEY_LEN);}
     if(idx>=0){ic_id[idx]=id;ic_save();}}
}
static void icp_close(void)
{
    (void)gfx(GFX_WINDOW,0u,0u,ICPW);
    icp_open=0;
    paint();
}
static void icp_click(int mx,int my)
{
    int sx=mx-(int)icp_wx,sy=my-(int)icp_wy-16;
    /* Grid first, then the button row - no overlap between the two rects. */
    if(sy>=24&&sy<24+(int)ICP_ROWS*ICP_CELL&&sx>=4&&sx<4+(int)ICP_COLS*ICP_CELL){
        int col=(sx-4)/ICP_CELL,row=(sy-24)/ICP_CELL;
        if(col<(int)ICP_COLS&&row<(int)ICP_ROWS){
            unsigned slot=(unsigned)row*ICP_COLS+(unsigned)col;
            /* Only real icons: an empty cell in the last row must not become a
               selection, or SAVE would write an id past the end of the set. */
            if(slot<ICON_COLOR_COUNT/2u){
                uint8_t id=(uint8_t)(slot*2u);
                uint32_t now=(uint32_t)hires();
                /* A second click on the same icon IS the save: pick and commit in
                   one gesture instead of hunting for the button. */
                if(icp_dbl_id==(int)id&&(uint32_t)(now-icp_dbl_tick)<DBL_TICKS){
                    icp_sel=(int)id;icp_assign(id);icp_close();return;
                }
                icp_dbl_id=(int)id;icp_dbl_tick=now;
                icp_sel=(int)id;
                icp_paint();
            }
        }
        return;
    }
    if(sy>=(int)icp_wh-48&&sy<(int)icp_wh-24){
        if(sx>=8&&sx<100){if(icp_sel>=0)icp_assign((uint8_t)icp_sel);icp_close();}
        else if(sx>=112&&sx<204)icp_close();
    }
}


/* ---- launcher bar ---------------------------------------------------------- */
static void launch_add(const char *label,const char *path,uint8_t builtin)
{
    unsigned i;
    if(launch_count>=LBAR_MAX)return;
    for(i=0;i<12u&&label[i];++i)launch[launch_count].label[i]=label[i];
    launch[launch_count].label[i]=0;
    for(i=0;i<63u&&path[i];++i)launch[launch_count].path[i]=path[i];
    launch[launch_count].path[i]=0;
    launch[launch_count].builtin=builtin;
    launch[launch_count].pid=0u;
    ++launch_count;
}
static void launch_defaults(void)
{
    launch_count=0;
    launch_add("COMMANDER","\\SYSTEM\\APP\\AC.APP",0u);
    launch_add("TERMINAL","",1u);
    launch_add("SYSINFO","\\SYSTEM\\APP\\SYSINFO.APP",0u);
    launch_add("TRASH","",2u);
}
static void launch_load(void)
{
    char *buf;unsigned i=0;
    launch_count=0;
    buf=read_all("\\SYSTEM\\LAUNCH.CFG");
    if(!buf||!buf[0]){launch_defaults();launch_loaded=1;return;}
    {uint32_t n=cfg_len(buf);
    while(i<n){
        char text[96];unsigned len=0,eq,a,b,v;
        while(i<n&&buf[i]!='\n'&&len<95u)text[len++]=buf[i++];
        while(i<n&&buf[i]!='\n')++i;
        if(i<n)++i;
        text[len]=0;
        while(len&&(text[len-1]=='\r'||text[len-1]==' '))text[--len]=0;
        a=0;while(a<len&&(text[a]==' '||text[a]=='\t'))++a;
        if(a>=len||text[a]==';'||text[a]=='#')continue;
        eq=a;while(eq<len&&text[eq]!='=')++eq;
        if(eq>=len)continue;
        b=eq;while(b>a&&(text[b-1]==' '||text[b-1]=='\t'))--b;
        text[b]=0;
        v=eq+1;while(v<len&&(text[v]==' '||text[v]=='\t'))++v;
        if(v>=len)continue;
        launch_add(text+a,text+v,(uint8_t)(text[v]=='*'?((text[v+1]=='T'||text[v+1]=='t')?1u:2u):0u));
    }}
    if(!launch_count)launch_defaults();
    launch_loaded=1;
}
/* Per-button state: 0 = not running, 1 = running (visible), 2 = minimised. */
static uint8_t launch_query(unsigned i)
{
    int32_t st;uint32_t st2[2];
    if(i>=launch_count||!launch[i].pid)return 0u;
    st=gfx(GFX_WINDOW_STATE,0,0,launch[i].pid);
    if(st>0)return (uint8_t)(st>2?2:st);          /* 1 visible, 2 minimised */
    /* No window: fall back to process liveness so a running app is never
       shown as "not running" if the state query is unavailable. */
    if(gui_call(32,launch[i].pid,0,st2,0)==0&&st2[0]!=5u)return 1u;
    return 0u;
}
static int launch_refresh(void)
{
    unsigned i;int changed=0;
    for(i=0;i<LBAR_MAX;++i){
        uint8_t s=launch_query(i);
        if(s!=launch_last[i]){launch_last[i]=s;changed=1;}
    }
    return changed;
}
/* Throttled poll from the idle tick (~5 Hz): the bar must follow minimise and
   restore without waiting for the next click. */
static int launch_poll(void)
{
    if(++launch_tick<10u)return 0;
    launch_tick=0;
    return launch_refresh();
}
/* A TOS program (.PRG/.TOS) is NOT an ATOS application: it runs through the
   TOS personality (syscall 65), which gives it its own window on this desktop.
   Handing it to the normal spawn path would fail, because it has no ASYSBOOT
   header.  This is the lazy entry point: the window is created by the kernel
   exactly here, when the user double-clicks the program. */
static int tos_program_path(const char *path)
{
    /* One shared, case-insensitive decision for every launch point: .PRG/.TOS
       (and the parameter-taking .TTP/.GTP) run in the TOS personality, while a
       bare name or .APP stays on the native spawn path. */
    return tos_program_path_ext(path);
}

/* "TOS RC <n>" with the program's own name, so a failed launch says WHICH
   file failed and with what code (a bare "START FAILED" told the user nothing). */
/* Komunikat o NIEUDANYM spawnie: "NAZWA SPAWN RC -NNN".  Ten sam styl co
   tos_rc_notice(), zeby dalo sie od razu odczytac przyczyne (audyt: test 2 -
   pulpit pokazywal "TOS STARTED" dla pliku .APP, ktory w ogole nie wstawal). */
static void spawn_rc_notice(const char *path,int32_t rc)
{
    char msg[34];unsigned k=0,i=0;
    const char *base=path;
    for(i=0;path[i];++i)if(path[i]=='\\')base=path+i+1;
    for(i=0;base[i]&&k<10u;++i)msg[k++]=base[i];
    {const char *w=" SPAWN RC ";for(i=0;w[i]&&k<sizeof(msg)-12u;++i)msg[k++]=w[i];}
    {uint32_t a=(uint32_t)rc;char d[12];unsigned m=0u;
     if(rc<0)a=0u-a;
     do{d[m++]=(char)('0'+(a%10u));a/=10u;}while(a&&m<11u);
     if(rc<0)msg[k++]='-';
     while(m&&k<sizeof(msg)-1u)msg[k++]=d[--m];}
    msg[k]=0;show_notice(msg);
}
static void tos_rc_notice(const char *path,int32_t rc)
{
    char msg[32];unsigned k=0,i=0;
    const char *base=path;
    for(i=0;path[i];++i)if(path[i]=='\\')base=path+i+1;
    for(i=0;base[i]&&k<10u;++i)msg[k++]=base[i];
    msg[k++]=' ';
    msg[k++]='T';msg[k++]='O';msg[k++]='S';msg[k++]=' ';
    msg[k++]='R';msg[k++]='C';msg[k++]=' ';
    {uint32_t a=(uint32_t)rc;char d[12];unsigned n=0u;
     if(rc<0)a=0u-a;                     /* unsigned negation: safe for INT32_MIN */
     do{d[n++]=(char)('0'+(a%10u));a/=10u;}while(a&&n<11u);
     if(rc<0)msg[k++]='-';
     while(n&&k<sizeof(msg)-1u)msg[k++]=d[--n];}
    msg[k]=0;
    show_notice(msg);
}
static void launch_run(unsigned index)
{
    gui_spawn_t sp;unsigned j;int32_t r;uint32_t st2[2];int32_t a=0;int32_t app_chk=0;uint32_t where[2]={0,0};
    if(index>=launch_count)return;
    if(launch[index].builtin==1u){
        (void)gfx(GFX_CONSOLE_SHOW,0,0,0);
        copytext(status,"TERMINAL",sizeof(status));paint();return;
    }
    if(launch[index].builtin==2u){tr_open_it();return;}
    /* Queue the TOS domain; the desktop must keep serving native windows.
       The immediate result reports whether the launch request was accepted. */
    if(tos_program_path(launch[index].path)){
        int32_t rc;
        show_notice("TOS WINDOW");
        rc=gui_call(ATOS_CALL_TOS_RUN,1,0,(void *)(uintptr_t)launch[index].path,0);
        if(rc<0)tos_rc_notice(launch[index].path,rc);
        else show_notice("TOS STARTED");
        return;
    }
    /* One instance per desktop: if the application is already running, only
       SHOW it - restore a minimised window or raise one that is behind.  Never
       start a second copy. */
    /* SYSTEM-WIDE single instance.  The check below only covers the copy THIS
       desktop started; two desktops could therefore launch the same application
       twice and the second copy fought the first over slots and the screen.  Ask
       the kernel first: 0 = not running, 1 = running here, 2 = running on another
       desktop, which the kernel has just brought forward. */
    {app_chk=gui_call(ATOS_CALL_APP_ACTIVATE,0,0,(void *)(uintptr_t)launch[index].path,(void *)where);a=app_chk;
     if(a==2){
         copytext(status,"SWITCHED TO RUNNING",sizeof(status));
         launch[index].pid=where[0];
         (void)launch_refresh();paint();return;
     }
     if(a==1)launch[index].pid=where[0];   /* shown by the block below */
    }
    if(launch[index].pid){
        int32_t st=gfx(GFX_WINDOW_STATE,0,0,launch[index].pid);
        if(st>0){
            int32_t a=gfx(GFX_WINDOW_ACTIVATE,0,0,launch[index].pid);
            copytext(status,a<0?"SHOW FAILED":"SHOWN",sizeof(status));
            (void)launch_refresh();paint();return;
        }
        if(gui_call(32,launch[index].pid,0,st2,0)==0&&st2[0]!=5u){
            copytext(status,"ALREADY RUNNING",sizeof(status));
            (void)launch_refresh();paint();return;
        }
    }
    zero(&sp,sizeof sp);
    for(j=0;j<63u&&launch[index].path[j];++j)sp.path[j]=launch[index].path[j];
    sp.path[j]=0;
    r=gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
    if(r>=0)launch[index].pid=sp.pid;
    (void)launch_refresh();
    if(r<0){
        /* Program TOS, ktory przesliznal sie przez routing rozszerzen: wolno
           sprobowac persony TOS (jej loader sam sprawdza magic 0x601A).
           ALE dla pliku .APP to bylo MYLACE: persona TOS przyjmowala zlecenie
           i pulpit pokazywal "TOS STARTED", choc aplikacja nie wstawala
           (audyt: test 2 - brak wolnego slotu procesu).  Dla .APP pokazujemy
           KOD bledu spawnu, zeby przyczyna byla widoczna od razu. */
        if(tos_program_path(launch[index].path)){
            int32_t rc=gui_call(ATOS_CALL_TOS_RUN,1,0,(void *)(uintptr_t)launch[index].path,0);
            if(rc<0)tos_rc_notice(launch[index].path,rc);
            else show_notice("TOS STARTED");
        }else spawn_rc_notice(launch[index].path,r);
        return;
    }
    show_notice("STARTED");
    paint();
}
/* Icon colour: mono is black; colour mode keeps the white disks, the grey
   DISK D and the red trash bin so the desktop stays recognisable. */
static uint8_t icon_ink(int i)
{
    if(!chunky)return 0u;                 /* mono: black on the dither */
    if(icons[i].res==3u)return 11u;       /* trash bin: red */
    if(icons[i].builtin==4u)return 3u;    /* DISK D: grey */
    if(icons[i].builtin==0u)return NC_BRIGHT; /* application shortcuts: light blue */
    return 1u;                             /* DISK C + TERMINAL: white */
}

static int text_eq(const char *a,const char *b);
/* Colour-icon id (icons_color.h) for a desktop icon; hover picks the lit
   variant.  Mono never reaches this path (paint_icons keeps the ink/paper
   GUI_ICON fallback). */
static uint8_t icon_rgb_id(int i,int hover)
{
    char key[IC_KEY_LEN];uint8_t base;
    icon_key(key,i);
    if(icons[i].builtin==0u){   /* program shortcut: prefer a loaded foo.png icon */
        unsigned d;
        for(d=0;d<dyn_icon_count;++d)if(dyn_li[d]==(uint8_t)icons[i].li)return (uint8_t)(ICON_COLOR_COUNT+d);
    }
    base=ic_id_for(key);
    /* clicked/selected = odd neighbour (enum is normal,clicked per icon). */
    return hover?(uint8_t)(base|1u):base;
}

/* Draw every icon at its grid cell.  Chunky uses white icons over the teal
   desktop; mono uses the classic black icons on the dither. */
static void draw_icon(unsigned i)
{
    int16_t x=(int16_t)(GRID_X+(int)icons[i].gx*CELL_W);
    int16_t y=(int16_t)(GRID_Y+(int)icons[i].gy*CELL_H);
    if(chunky){
        /* Also highlight the KEYBOARD selection: with the colour icons this
           path used to look only at hover_icon, so arrows moved an invisible
           selection (and Enter then ran an icon the user could not see). */
        int hov=((int)i==hover_icon||(int)i==sel_icon)&&(int)i!=drag_icon;
        add(GUI_ICON_RGB,x,y,(uint16_t)icon_px,(uint16_t)(icon_px+16),icon_rgb_id((int)i,hov),icons[i].label);
    }else{
        uint8_t ink=((int)i==drag_icon)?1u
                   :((int)i==sel_icon)?1u
                   :icon_ink((int)i);
        add(GUI_ICON,x,y,GUI_ICON_MONO_SIZE,GUI_ICON_MONO_H,ink,icons[i].label);
        list[n-1u].background=1u;
        list[n-1u].reserved=icons[i].res;
    }
}
static void draw_icons(void)
{
    unsigned i;
    for(i=0;i<icon_count;++i)draw_icon(i);
}
/* Prostokat, ktory NAPRAWDE zajmuje ikona: bitmapa icon_px x icon_px plus
   etykieta do 12 znakow (96 px) wysrodkowana pod nia.  Przy malych ikonach
   etykieta jest szersza od komorki, wiec zmazanie samej komorki zostawialo
   resztki po hover/drag (i to niezaleznie od rozmiaru ikon). */
static void cell_rect(int i,int16_t *x,int16_t *y,uint16_t *w,uint16_t *h)
{
    int16_t cx=(int16_t)(GRID_X+(int)icons[i].gx*CELL_W);
    int16_t cy=(int16_t)(GRID_Y+(int)icons[i].gy*CELL_H);
    int16_t lx=(int16_t)(cx+(icon_px-96)/2);
    int16_t x0=(lx<cx)?lx:cx;
    int16_t x1=(int16_t)(((lx+96)>(cx+icon_px))?(lx+96):(cx+icon_px));
    *x=x0;*y=cy;*w=(uint16_t)(x1-x0);*h=(uint16_t)(icon_px+16);
}
static void clear_cell(int i)
{
    int16_t x,y;uint16_t w,h;
    cell_rect(i,&x,&y,&w,&h);
    add(GUI_BG,x,y,w,h,0,0);
}
/* Hover repaint: clear and redraw ONLY the old and new cells.  Redrawing every
   icon made the batch damage span the whole icon area on every pointer move,
   which is exactly the refresh cost that delays the cursor. */
static void paint_hover(int oldi,int newi)
{
    n=0;
    if(oldi>=0&&(unsigned)oldi<icon_count)clear_cell(oldi);
    if(newi>=0&&(unsigned)newi<icon_count&&newi!=oldi)clear_cell(newi);
    if(oldi>=0&&(unsigned)oldi<icon_count)draw_icon((unsigned)oldi);
    if(newi>=0&&(unsigned)newi<icon_count)draw_icon((unsigned)newi);
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
}
static void paint_icons(int clear_cells)
{
    unsigned i;
    /* Erase cells an icon left (drag). */
    for(i=0;clear_cells&&i<dirty_count;++i){
        /* Komorka, z ktorej ikona ZJECHALA - geometria jak clear_cell(). */
        int16_t dx=(int16_t)(GRID_X+(int)dirty_gx[i]*CELL_W);
        int16_t dy=(int16_t)(GRID_Y+(int)dirty_gy[i]*CELL_H);
        int16_t lx=(int16_t)(dx+(icon_px-96)/2);
        int16_t x0=(lx<dx)?lx:dx;
        int16_t x1=(int16_t)(((lx+96)>(dx+icon_px))?(lx+96):(dx+icon_px));
        add(GUI_BG,x0,dy,(uint16_t)(x1-x0),(uint16_t)(icon_px+16),0,0);
    }
    dirty_count=0;
    /* Clear ALL cells first: a long label can cross the grid cell boundary. */
    for(i=0;clear_cells&&i<icon_count;++i)clear_cell((int)i);
    draw_icons();
}

/* Run the action behind an icon.  The two disks are hardcoded builtins; the
   launcher entries (program / terminal / trash) reuse launch_run(). */
static void icon_activate(int idx)
{
    if(idx<0)return;
    if(icons[idx].builtin==1u){(void)gfx(GFX_CONSOLE_SHOW,0,0,0);
        copytext(status,"TERMINAL",sizeof(status));paint();return;}
    if(icons[idx].builtin==2u){tr_open_it();return;}
    if(icons[idx].builtin==5u){ma_open_it();return;}    /* My Atari: drives */
    if(icons[idx].li>=0)launch_run((unsigned)icons[idx].li);
}

/* Saved resolution (from a RES= line), applied after the first paint. */
static uint16_t save_w,save_h;static uint8_t save_depth,save_res_valid;
/* Liczba dziesietna do bufora konfiguracji; zwraca nowa dlugosc. */
static unsigned put_u16(char *buf,unsigned len,unsigned v)
{
    char d[6];int dn=0;
    if(!v)d[dn++]='0';
    while(v){d[dn++]=(char)('0'+(v%10u));v/=10u;}
    while(dn)buf[len++]=d[--dn];
    return len;
}
/* Persist the desktop config to \SYSTEM\DESKTOP.CFG: RES= mode index,
   THEME= palette index, TOSTA/TOSTT, FILEWIN/TRASHWIN (rozmiary okien),
   then one "LABEL=gx,gy" line per icon.
   Returns 0 when written, -1 on failure, so the caller can report the real
   outcome instead of claiming success.
   Rdzen (core) nie dotyka UI: rozmiar okna zapisujemy przy KAZDYM resize,
   a pasek statusu ma wtedy milczec (inaczej kazde przeciagnie ramki
   mruga "CONFIG SAVED"). */
static int desktop_save_core(void)
{
    char buf[448];unsigned i,len=0,k;
    buf[len++]='R';buf[len++]='E';buf[len++]='S';buf[len++]='=';
    /* Pelny tryb WxHxD: rozdzielczosc i glebia sa wybierane osobno, wiec sam
       indeks nie opisuje ustawienia. */
    {unsigned v=SCRW;char d[6];int dn=0;
     if(!v)
         d[dn++]='0';
     while(v){
         d[dn++]=(char)('0'+(v%10u));
         v/=10u;
     }
     while(dn)buf[len++]=(char)d[--dn];}
    buf[len++]='x';
    {unsigned v=SCRH;char d[6];int dn=0;
     if(!v)
         d[dn++]='0';
     while(v){
         d[dn++]=(char)('0'+(v%10u));
         v/=10u;
     }
     while(dn)buf[len++]=(char)d[--dn];}
    buf[len++]='x';
    /* Zapisujemy glebie KARTY (card_bpp: 1/4/8/16), a NIE format RAM
       (cur_bpp = 8 dla 4 i 16 bpp).  Wczesniej 65K zapisywalo sie jako
       "RES=800x600x8", wiec po restarcie wracalo 256 kolorow - to byla
       wlasnie "redukcja glebi" z audytu. */
    {unsigned v=(unsigned)card_bpp;char d[4];int dn=0;
     if(!v)
         d[dn++]='0';
     while(v){
         d[dn++]=(char)('0'+(v%10u));
         v/=10u;
     }
     while(dn)buf[len++]=(char)d[--dn];}
    buf[len++]='\n';
    buf[len++]='T';buf[len++]='H';buf[len++]='E';buf[len++]='M';buf[len++]='E';buf[len++]='=';
    buf[len++]=(char)('0'+theme_index);buf[len++]='\n';
    /* TOS compatibility options, read back by the kernel at START. */
    buf[len++]='T';buf[len++]='O';buf[len++]='S';buf[len++]='T';buf[len++]='A';buf[len++]='=';
    buf[len++]=(char)(tos_cfg_at_start?'1':'0');buf[len++]='\n';
    buf[len++]='T';buf[len++]='O';buf[len++]='S';buf[len++]='T';buf[len++]='T';buf[len++]='=';
    buf[len++]=(char)(tos_cfg_prefer_tt?'1':'0');buf[len++]='\n';
    /* Rozmiary okien PLIKI i KOSZ - zapisywane przy zmianie rozmiaru, czytane
       przy otwarciu (z clampem do ekranu).  Bez tego kazdy restart wracal do
       420x300 / 440x300 i uzytkownik ustawial je od nowa. */
    buf[len++]='F';buf[len++]='I';buf[len++]='L';buf[len++]='E';
    buf[len++]='W';buf[len++]='I';buf[len++]='N';buf[len++]='=';
    len=put_u16(buf,len,(unsigned)fb_ww);buf[len++]='x';
    len=put_u16(buf,len,(unsigned)fb_wh);buf[len++]='\n';
    buf[len++]='T';buf[len++]='R';buf[len++]='A';buf[len++]='S';
    buf[len++]='H';buf[len++]='W';buf[len++]='I';buf[len++]='N';buf[len++]='=';
    len=put_u16(buf,len,(unsigned)tr_ww);buf[len++]='x';
    len=put_u16(buf,len,(unsigned)tr_wh);buf[len++]='\n';
    for(i=0;i<icon_count;++i){
        for(k=0;icons[i].label[k]&&len<360u;++k)buf[len++]=icons[i].label[k];
        if(len<360u)buf[len++]='=';
        if(len<360u)buf[len++]=(char)('0'+icons[i].gx);
        if(len<360u)buf[len++]=',';
        if(len<360u)buf[len++]=(char)('0'+icons[i].gy);
        if(len<360u)buf[len++]='\n';
    }
    {int32_t r=gui_call(60,(uint32_t)len,0,(void *)"\\SYSTEM\\DESKTOP.CFG",(void *)buf);
     return r<0?-1:0;}
}
/* Widoczny zapis (menu/kontekst): komunikat i odswiezenie paska. */
static int desktop_save(void)
{
    int r=desktop_save_core();
    copytext(status,r<0?"SAVE FAILED":"CONFIG SAVED",sizeof(status));
    paint();
    return r;
}
/* Cichy zapis przy resize okna - bez mrugania statusu. */
static void desktop_save_quiet(void){(void)desktop_save_core();}

static int text_eq(const char *a,const char *b)
{while(*a&&*b){if(*a!=*b)return 0;++a;++b;}return *a==*b;}

/* "WxH" -> dwie liczby (bez glebi).  0 = tekst nie jest rozmiarem.
   Uzywane przez FILEWIN=/TRASHWIN= z DESKTOP.CFG. */
static int parse_wh(const char *s,unsigned *w,unsigned *h)
{
    unsigned a=0,b=0,i=0;
    if(s[i]<'0'||s[i]>'9')return 0;
    while(s[i]>='0'&&s[i]<='9'){a=a*10u+(unsigned)(s[i++]-'0');if(a>65535u)return 0;}
    if(s[i++]!='x')return 0;
    if(s[i]<'0'||s[i]>'9')return 0;
    while(s[i]>='0'&&s[i]<='9'){b=b*10u+(unsigned)(s[i++]-'0');if(b>65535u)return 0;}
    if(s[i]&&s[i]!='\r')return 0;
    if(!a||!b)return 0;
    *w=a;*h=b;return 1;
}

/* "WALL=" -> numer wzoru tapety.  0..3 = wzory proceduralne (SOLID, DITHER,
   CHECKER, DIAGONAL), 4 = WLASNY KAFELEK 16x16 z edytora SETTINGS -> TILE.
   Wczesniej parser bral JEDNA cyfre i obcinal wszystko powyzej 3 do 0, wiec
   zapisane przez edytor WALL=4 wczytywalo sie jako SOLID i tapeta CUSTOM nie
   rysowala sie po restarcie - mimo poprawnego kafelka w jadrze. */
#define WALL_PATTERN_MAX 4u
static unsigned parse_wall(const char *s)
{
    unsigned v=0;int any=0;
    while(*s>='0'&&*s<='9'){v=v*10u+(unsigned)(*s++-'0');any=1;if(v>99u)return 0u;}
    if(!any||v>WALL_PATTERN_MAX)return 0u;
    return v;
}

/* Restore the config saved by desktop_save(); a missing file keeps defaults. */
static void desktop_load(void)
{
    char *buf;int32_t r;unsigned i=0;
    buf=read_all("\\SYSTEM\\DESKTOP.CFG");
    if(!buf)return;
    r=(int32_t)cfg_len(buf);
    if(r<=0)return;
    buf[r]=0;
    /* Po wczytaniu configu WYMUS pelne przemalowanie tla: kafelek (TILE=) i
       kolory sa wgrywane do jadra tutaj, a pierwsze malowanie moglo juz byc
       wykonane - bez tego tapeta CUSTOM zostawala jednolita do nastepnej
       zmiany wygladu. */
    bg_dirty=1;
    while(i<(unsigned)r){
        /* 96 znakow, bo TILE= to 69 (64 hex) + "TILE="; przy 32 linia byla
           obcinana i kafelek wczytywal sie jako same zera (tapeta jednolita). */
        char line[96],*val;unsigned l=0,eq;
        while(i<(unsigned)r&&buf[i]!='\n'&&l<95u)line[l++]=buf[i++];
        while(i<(unsigned)r&&buf[i]!='\n')++i;
        if(i<(unsigned)r)++i;
        line[l]=0;
        for(eq=0;eq<l&&line[eq]!='=';++eq){}
        if(eq>=l)continue;
        line[eq]=0;
        val=&line[eq+1];
        if(text_eq(line,"RES")){
            /* Nowy format WxHxD; STARY (jedna cyfra = indeks dawnej listy
               5 trybow) nadal dziala, zeby istniejacy DESKTOP.CFG nie
               przelaczyl pulpitu w zly tryb po aktualizacji. */
            unsigned w=0,h=0,dp=0;
            (void)settings_parse_mode(val,&w,&h,&dp);
            if(w&&h&&dp){
                unsigned i2;
                save_w=(uint16_t)w;save_h=(uint16_t)h;save_depth=(uint8_t)dp;
                save_res_valid=1u;
                for(i2=0;i2<SET_RES;++i2)
                    if(setup_res[i2].w==save_w&&setup_res[i2].h==save_h)setup_res_sel=(uint8_t)i2;
                for(i2=0;i2<SET_COL;++i2)
                    if(setup_col[i2].depth==save_depth)setup_col_sel=(uint8_t)i2;
            }
        }else if(text_eq(line,"BG")){
            /* Kolor tla wybrany w SETTINGS -> DESKTOP (3 cyfry hex, 12-bit).
               Ustawiany w palecie, zeby przetrwal restart. */
            unsigned v=0,z=0,okv=1;
            for(;z<3u;++z){
                char c=val[z];
                if(c>='0'&&c<='9')v=(v<<4)|(unsigned)(c-'0');
                else if(c>='A'&&c<='F')v=(v<<4)|(unsigned)(c-'A'+10);
                else if(c>='a'&&c<='f')v=(v<<4)|(unsigned)(c-'a'+10);
                else{okv=0;break;}
            }
            if(okv)(void)gfx(GFX_THEME,v,0,5u);
        }else if(text_eq(line,"BAR")){
            unsigned v=0,z=0,okv=1;
            for(;z<3u;++z){
                char c=val[z];
                if(c>='0'&&c<='9')v=(v<<4)|(unsigned)(c-'0');
                else if(c>='A'&&c<='F')v=(v<<4)|(unsigned)(c-'A'+10);
                else if(c>='a'&&c<='f')v=(v<<4)|(unsigned)(c-'a'+10);
                else{okv=0;break;}
            }
            if(okv)(void)gfx(GFX_THEME,v,0,7u);
        }else if(text_eq(line,"TILE")){
            /* Wlasny kafelek: 64 znaki hex = 32 B (16x16, 1 bpp). */
            uint8_t tile[32];unsigned b=0,okv=1;
            for(;b<32u;++b){
                unsigned v=0,z=0;
                for(;z<2u;++z){
                    char c=val[b*2u+z];
                    if(!c){okv=0;break;}
                    if(c>='0'&&c<='9')v=(v<<4)|(unsigned)(c-'0');
                    else if(c>='A'&&c<='F')v=(v<<4)|(unsigned)(c-'A'+10);
                    else if(c>='a'&&c<='f')v=(v<<4)|(unsigned)(c-'a'+10);
                    else{okv=0;break;}
                }
                if(!okv)break;
                tile[b]=(uint8_t)v;
            }
            if(okv){unsigned b;for(b=0;b<32u;++b)(void)gfx(45,(b<<8)|(uint32_t)tile[b],0,16u);}
        }else if(text_eq(line,"TILEFG")){
            unsigned v=0;unsigned z=0,okf=1;
            for(;z<3u;++z){char c=val[z];
                if(c>='0'&&c<='9')v=(v<<4)|(unsigned)(c-'0');
                else if(c>='A'&&c<='F')v=(v<<4)|(unsigned)(c-'A'+10);
                else if(c>='a'&&c<='f')v=(v<<4)|(unsigned)(c-'a'+10);
                else{okf=0;break;}}
            if(okf)(void)gfx(45,v&0xfffu,0,4u);
        }else if(text_eq(line,"WALL")){
            wall_sel=(uint8_t)parse_wall(val);
        }else if(text_eq(line,"THEME")){
            unsigned ti=0;
            if(val[0]>='0'&&val[0]<='9')ti=(unsigned)(val[0]-'0');
            if(ti<theme_count)theme_apply_index((uint8_t)ti);
        }else if(text_eq(line,"FILEWIN")){
            unsigned w=0,h=0;
            if(parse_wh(val,&w,&h)){fb_ww=(uint16_t)w;fb_wh=(uint16_t)h;}
        }else if(text_eq(line,"TRASHWIN")){
            unsigned w=0,h=0;
            if(parse_wh(val,&w,&h)){tr_ww=(uint16_t)w;tr_wh=(uint16_t)h;}
        }else if(text_eq(line,"TOSTA")){
            tos_cfg_at_start=(uint8_t)(val[0]=='1');
        }else if(text_eq(line,"TOSTT")){
            tos_cfg_prefer_tt=(uint8_t)(val[0]=='1');
        }else{
            unsigned gx=0,gy=0,k=0;
            if(val[0]>='0'&&val[0]<='9'){gx=(unsigned)(val[0]-'0');++k;}
            if(val[k]==',')++k;
            if(val[k]>='0'&&val[k]<='9')gy=(unsigned)(val[k]-'0');
            {unsigned n;for(n=0;n<icon_count;++n)
                if(text_eq(line,icons[n].label)){icons[n].gx=(int8_t)gx;icons[n].gy=(int8_t)gy;break;}}
        }
    }
    /* Wczytany wzor oddajemy do JADRA (setter op 37 sub 9).  Getter jest
       zrodlem prawdy dla paint() i dla SETTINGS, wiec po restarcie jadro musi
       znac CUSTOM (4), a nie domyslne 0 - inaczej zakladka DESK w SETTINGS
       pokazywala SOLID i APPLY nadpisywal WALL=4 zerem. */
    (void)gfx(GFX_THEME,(uint32_t)wall_sel,0,9u);
}

int app_main(void)
{
    gfx_pid=lookup("SYSTEM.GFX");
    gui_pid=lookup("SYSTEM.GUI");
    if(gfx_pid<0||gui_pid<0)return 1;
    /* Adopt the active video mode: the canvas, the menu bar and the window
       defaults follow it (640x400 on the Shifter, larger on an ET4000). */
    mode_query();
    (void)gui(GUI_REG);
    (void)gui(GUI_BEGIN_KEYS);       /* register + become the keyboard owner */
    list=(gui_draw_t *)(uintptr_t)(uint32_t)gui_call(16,LIST_MAX*(uint32_t)sizeof(gui_draw_t),1u,0,0);
    if(!list)return 1;               /* no draw list, no desktop */
    fb_all=(dir_ent_t *)(uintptr_t)(uint32_t)gui_call(16,FB_TOTAL*(uint32_t)sizeof(dir_ent_t),1u,0,0);
    if(!fb_all)return 1;
    launch=(launch_t *)(uintptr_t)(uint32_t)gui_call(16,LBAR_MAX*(uint32_t)sizeof(*launch),1u,0,0);
    icons=(icon_t *)(uintptr_t)(uint32_t)gui_call(16,ICON_MAX*(uint32_t)sizeof(*icons),1u,0,0);
    if(!launch||!icons)return 1;
    launch_load();                   /* \SYSTEM\LAUNCH.CFG, or the built-in list */
    theme_sync();                    /* kernel owns the themes: learn count/current */
    /* Rozmiar ikon i gestosc siatki trzyma JADRO (wybiera je SETTINGS): opcode 49
       zwraca jedno i drugie - (px<<8)|tryb - wiec nie zgadujemy z configu. */
    {int32_t v=gfx(49,0,0,0);
     if(v>0){
         unsigned px=(unsigned)((uint32_t)v>>8)&0xffu,mode=(unsigned)v&0xffu;
         if(px>=32u&&px<=64u)icon_px=(int16_t)px;
         if(mode<=2u)grid_mode=(uint8_t)mode;
     }
     layout_cells();}
    icons_build();                   /* DISK C/D + the launcher shortcuts */
    desktop_load();                  /* layout/theme/resolution + TOS options */
    icons_clamp();                   /* zapisane pozycje moga nie pasowac do siatki */


    ic_load();                       /* type->icon mapping (\SYSTEM\ICONS.CFG) */
    load_dyn_icons();                /* per-app foo.png icons */

    /* Load resources before taking the screen: keep the boot logo visible. */
    if(gfx(GFX_DESKTOP,0,0,0)){
        /* Exactly one desktop owns a session.  A second 'start desktop.app'
           is not an error: say so and exit cleanly instead of stacking a
           second screen (opcode 13 already refuses the enrolment). */
        (void)gui_call(1,0,0,(void *)(uintptr_t)"DESKTOP ALREADY RUNNING\n",0);
        return 0;
    }

    if(save_res_valid&&!(SCRW==save_w&&SCRH==save_h&&cur_bpp==save_depth))
        video_try(save_w,save_h,save_depth);   /* repaints at the saved mode */
    else
        paint();
    /* The kernel already showed real startup progress. The first paint above
       is the ready desktop; do not cover it with a second timed splash. */
    for(;;){
#ifdef ARK_DESKTOP_AUTORUN_MODPLAY
        /* Test-only: start the MOD player ~12 s after the shell's loop begins.
           Time based via hires(), exactly like the TOS autorun hook above: this
           loop blocks in receive(), so an iteration counter is meaningless. */
        {static uint32_t mp_next;
         uint32_t mp_now=hires();
         if(!mp_next)mp_next=mp_now+12288u*12u;
         if(mp_next!=(uint32_t)-1&&(int32_t)(mp_now-mp_next)>=0){
             /* Decisive marker: proves THIS desktop build is the one running.
                The absolute store needs the local warning suppression below. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
             *(volatile uint32_t *)(uintptr_t)0x8F0u=0x5aa55aa5u;
#pragma GCC diagnostic pop
             gui_spawn_t sp;zero(&sp,sizeof(sp));
             copytext(sp.path,"\\SYSTEM\\APP\\MODPLAY.APP",sizeof(sp.path));
             (void)gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
             mp_next=(uint32_t)-1;}}
#endif

#ifdef ARK_DESKTOP_AUTORUN_SETTINGS
        /* HOOK TESTOWY (tylko z -DARK_DESKTOP_AUTORUN_SETTINGS, nigdy w wydaniu):
           po ~8 s otwiera SETTINGS.APP, zeby dalo sie zrobic zrzut okna bez
           klikanie.  Wzorowany na ARK_DESKTOP_AUTORUN_MODPLAY. */
        {static uint32_t st_next;
         uint32_t st_now=hires();
         if(!st_next)st_next=st_now+12288u*8u;
         if(st_next!=(uint32_t)-1&&(int32_t)(st_now-st_next)>=0){
             gui_spawn_t sp;unsigned j;const char *pa="\\SYSTEM\\APP\\SETTINGS.APP";
             zero(&sp,sizeof(sp));
             for(j=0;pa[j]&&j+1u<sizeof(sp.path);++j)sp.path[j]=pa[j];
             sp.path[j]=0;
             (void)gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
             st_next=(uint32_t)-1;}}
#endif
        int mx,my;unsigned buttons;uint32_t ev_tick=0;
        /* An empty-mailbox error is not a reason to tear the desktop down:
           retry.  Only the explicit CLOSE/Escape path may leave. */
#ifdef ARK_TOS_AUDIO_AUTORUN
        /* Test-only harness (never compiled into a shipped desktop): starts the
           audio acceptance probe every ~8 s, so start/exit/owner-release can be
           repeated 20 times without a mouse or the flaky RUN dialog. */
        {static uint32_t hook_next;
         uint32_t hook_now=hires();
         /* Time based, not iteration based: this loop blocks in receive(), so a
            counter of iterations would depend on how often a message arrives. */
         if(!hook_next)hook_next=hook_now+12288u*8u;          /* first after ~8 s */
         if((int32_t)(hook_now-hook_next)>=0){
             int32_t rc;
             hook_next=hook_now+12288u*8u;
             rc=gui_call(ATOS_CALL_TOS_RUN,1,0,
                         (void *)(uintptr_t)"\\SYSTEM\\APP\\TOSAUDIO.PRG",0);
             show_notice(rc<0?"AUTORUN FAILED":"AUTORUN STARTED");paint();
         }}
#endif
#ifdef ARK_TOS_AUDIO_CONFLICT
        /* Test-only: a NATIVE application becomes the sound owner first, then the
           TOS probe runs.  Every TOS audio entry must be refused (the counters
           kdbg_tos_audio_refused / kdbg_tos_xbios28w tell the story). */
        {static uint32_t stage,next;
         uint32_t now=hires();
         if(!next)next=now+12288u*8u;
         if((int32_t)(now-next)>=0){
             next=now+12288u*14u;
             if(stage==0u){
                 gui_spawn_t sp;zero(&sp,sizeof(sp));
                 copytext(sp.path,"\\SYSTEM\\APP\\ARKANOID.APP",sizeof(sp.path));
                 (void)gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
                 show_notice("CONFLICT: native owner");paint();stage=1u;
             }else if(stage==1u){
                 (void)gui_call(ATOS_CALL_TOS_RUN,1,0,
                                (void *)(uintptr_t)"\\SYSTEM\\APP\\TOSAUDIO.PRG",0);
                 show_notice("CONFLICT: TOS probe");paint();stage=2u;
             }
         }}
#endif
#ifdef ARK_TOS_AUDIO_B
        /* Test-only, minimal: keep returning the keyboard focus to the DESKTOP so
           the RUN dialog can be reached while a game window is on top.  GUI opcode
           24 is the kernel's own "show and focus the console" call.  The native
           game and the serving are done by the harness from the keyboard. */
        {static uint32_t next;
         uint32_t now=hires();
         if(!next)next=now+12288u*22u;             /* after the harness serves */
         if((int32_t)(now-next)>=0){
             (void)gui_call(24u,0,0,0,0);
             next=now+12288u*3u;
         }}
#endif
        /* WYGLAD PULPITU: SETTINGS zmienia tlo/pasek/tapeete, a powierzchnia
           pulpitu trzyma stare piksele.  Ten polling MUSI dzialac takze wtedy,
           gdy zadne okno nie przysyla wiadomosci - dlatego receive() jest
           NIEBLOKUJACE, a bezczynnosc oddaje jeden tick (inaczej petla pali
           CPU).  Wczesniej receive(...,1) blokowalo sie na zawsze i ani
           polling, ani WMOV z jadra nie byly obslugiwane. */
        /* Wyglad ikon: SETTINGS zmienia rozmiar (32/48/64) i gestosc siatki, a my
           przeliczamy komorki i przemalowujemy caly pulpit - inaczej stare
           komorki zostalyby z poprzedniego ukladu. */
        {int32_t v=gfx(49,0,0,0);
         kdbg_icon_q=(uint32_t)v;
         if(v>0){
             unsigned px=(unsigned)((uint32_t)v>>8)&0xffu,mode=(unsigned)v&0xffu;
             int changed=0;
             if(px>=32u&&px<=64u&&(int)px!=icon_px){icon_px=(int16_t)px;changed=1;}
             if(mode<=2u&&mode!=grid_mode){grid_mode=(uint8_t)mode;changed=1;}
             if(changed){++kdbg_icon_changed;layout_cells();icons_clamp();bg_dirty=1;paint();}
         }
         kdbg_icon_px=(uint32_t)icon_px;}
        {int32_t bg_now=gfx(GFX_THEME,0,0,6u),bar_now=gfx(GFX_THEME,0,0,8u);
         int32_t wall_now=gfx(GFX_THEME,0,0,10u);
         ++kdbg_dbg_poll;
         if(wall_now!=painted_wall)++kdbg_dbg_wall_diff;
         if(bg_now!=painted_bg||bar_now!=painted_bar||wall_now!=painted_wall){
             bg_dirty=1;paint();}}
        if(!pending_pop(&m)&&receive(&m,0)){pause_tick();
#ifdef ARK_TOS_AUDIO_ACCEPT
        /* Test-only acceptance harness.  It is called from the EVENT path, right
           where the desktop handles a message - the same context as its RUN
           dialog - because calling ATOS_CALL_TOS_RUN from the background of the
           loop produced a session whose kernel-side counters never moved. */
        {static uint32_t stage,next;
         uint32_t now=hires();
         if(!next)next=now+12288u*8u;
         if((int32_t)(now-next)>=0){
             if(stage==0u){
                 gui_spawn_t sp;zero(&sp,sizeof(sp));
                 copytext(sp.path,"\\SYSTEM\\APP\\ARKANOID.APP",sizeof(sp.path));
                 (void)gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
                 show_notice("ACCEPT: native owner");stage=1u;next=now+12288u*16u;
             }else{
                 (void)gui_call(ATOS_CALL_TOS_RUN,1,0,
                                (void *)(uintptr_t)"\\SYSTEM\\APP\\TOSAUDIO.PRG",0);
                 show_notice("ACCEPT: TOS probe");next=now+12288u*12u;
             }
         }}
#endif
            if(launch_poll())paint();
            /* SETTINGS -> DESKTOP zmienia kolor tla w palecie; powierzchnia
               pulpitu ma stare piksele, wiec trzeba ja PRZEMALOWAC, inaczej
               zmiana bylaby widoczna tylko pod oknami.  Jeden odczyt na tick. */
            continue;}
        else pause_tick();      /* brak wiadomosci: oddaj tick i sprawdz wyglad */
        /* Kernel-injected input (KKEY/KMOU) arrives with sender=0; service
           replies carry the GUI pid.  Dropping sender 0 is what silently
           killed every click and key on the desktop. */
        if(!m.v[0])continue;
        if(m.sender!=0u&&m.sender!=(uint32_t)gui_pid)continue;
        if(m.v[0]==KKEY){
            uint32_t type=(m.v[1]>>24)&0xffu,scan=m.v[1]&0x7fu,ch=m.v[2];
            if(type!=1u)continue;                       /* make only */
            if(run_mode){
                if(scan==0x1cu||ch==0x0du)spawn_run();
                else if((scan==0x0eu||ch==0x08u)&&run_len){run_buf[--run_len]=0;paint_run();}
                else if(ch>=32u&&ch<127u&&run_len+5u<sizeof(run_buf)){run_buf[run_len++]=(char)ch;run_buf[run_len]=0;paint_run();}
            }else if(ctx_open){
                if(scan==0x48u&&ctx_sel>0){ctx_sel--;ctx_paint();}          /* up */
                else if(scan==0x50u&&ctx_sel+1<ctx_items){ctx_sel++;ctx_paint();} /* down */
                else if(scan==0x1cu)ctx_activate();                          /* enter */
                else if(scan==0x01u){ctx_close_popup();}  /* escape */
            }else if(menu_open){
                unsigned items=menu_which?OPT_ITEMS:MENU_ITEMS;
                if(scan==0x48u&&menu_sel)menu_sel--;         /* up */
                else if(scan==0x50u&&menu_sel+1u<items)menu_sel++;  /* down */
                else if(scan==0x1cu)menu_activate(menu_sel);
                else if(scan==0x01u){menu_open=0;bg_dirty=1;(void)gfx(GFX_OVERLAY,0,0,0);}
                if(menu_open)paint_menu();else paint();
            }else if(trash_open&&(scan==0x48u||scan==0x50u||scan==0x1cu||scan==0x53u||scan==0x01u)){
                /* TRASH window: keyboard mirrors the mouse.  Only these keys
                   belong to it; every other key falls through so F-keys and the
                   menu keep working (the same trap SETUP fell into). */
                if(scan==0x48u){if(tr_sel>0)--tr_sel;else if(tr_count)tr_sel=0;tr_paint();}  /* up */
                else if(scan==0x50u){if(tr_sel<(int)tr_count-1)++tr_sel;tr_paint();}          /* down */
                else if(scan==0x1cu)tr_restore();                                            /* enter */
                else if(scan==0x53u)tr_delete();                                             /* delete */
                else{trash_open=0;(void)gfx(GFX_WINDOW,0u,0u,TRW);}                          /* escape hides */
            }else if(ma_open&&(scan==0x48u||scan==0x50u||scan==0x1cu||scan==0x01u)){
                if(scan==0x48u){if(ma_sel>0)--ma_sel;else if(ma_count)ma_sel=0;ma_paint();}   /* up */
                else if(scan==0x50u){if(ma_sel<(int)ma_count-1)++ma_sel;ma_paint();}          /* down */
                else if(scan==0x1cu&&ma_sel>=0){char p[8],t[13];
                    ma_path(p,(unsigned)ma_sel);
                    t[0]=(char)('C'+ma_sel);t[1]=':';t[2]=0;
                    fb_open_at(p,t);}                                                         /* enter */
                else{ma_open=0;(void)gfx(GFX_WINDOW,0u,0u,MAW);}                              /* escape */
            }else if(icp_open&&scan==0x01u){icp_open=0;(void)gfx(GFX_WINDOW,0u,0u,ICPW);paint();}
            /* Keyboard launcher.  A double click needs precision AND speed: on a
               coarse grid, or with an awkward pointer, the first click lands
               outside the icon cell and nothing happens (observed: clicks at
               y=161 while the TOSCOL cell starts at y=164).  Left/Up and
               Right/Down walk the icon list and Enter runs the highlighted one
               through the very same icon_activate() the double click uses, so a
               TOS .PRG, a native .APP and the builtins all behave identically. */
            /* Stronnicowanie okna plikow: DOTAD tylko "MORE >" mysza, bez powrotu
               (trzeba bylo zamykac i otwierac okno od nowa).  Teraz Right/Down =
               nastepna strona, Left/Up = poprzednia; przy malych katalogach
               (fb_scrollmode) przewija WM, wiec klawisze nic nie psuja. */
            else if(fb_open&&!fb_scrollmode&&(scan==0x4du||scan==0x50u)){
                if(fb_more){++fb_page;fb_sel=-1;fb_page_calc();fb_paint();}
            }
            else if(fb_open&&!fb_scrollmode&&(scan==0x4bu||scan==0x48u)){
                if(fb_page){--fb_page;fb_sel=-1;fb_page_calc();fb_paint();}
            }
            else if(!fb_open&&!run_mode&&(scan==0x4du||scan==0x50u||scan==0x4bu||scan==0x48u)){
                if(icon_count){
                    /* Repaint BOTH cells: moving the highlight without clearing
                       the old one left a trail of highlighted icons (the icons
                       have no background of their own). */
                    if(sel_icon>=0)dirty_add(icons[sel_icon].gx,icons[sel_icon].gy);
                    if(scan==0x4du||scan==0x50u)sel_icon=(sel_icon+1)%(int)icon_count;
                    else sel_icon=(sel_icon<=0)?(int)icon_count-1:sel_icon-1;
                    dirty_add(icons[sel_icon].gx,icons[sel_icon].gy);
                    show_notice(icons[sel_icon].label);   /* co jest zaznaczone */
                }
            }else if(!fb_open&&!run_mode&&scan==0x1cu){
                if(sel_icon>=0)icon_activate(sel_icon);
            }
            else if(scan==0x3bu)script_drag();                       /* F1 test */
            else if(scan==0x3cu)script_resize();                      /* F2 test */
            else if(scan==0x3du)script_raise();
            else if(scan==0x3eu)script_menu();
            else if(scan==0x3fu)script_move();
            else if(scan==0x40u)(void)gfx(28,0,0,0);   /* F6: kernel autodrag test */
            else if(scan==0x41u)fb_open_at("\\","DISK C:");   /* F7: file window (same as DISK C) */
            /* F9: TEST - przelacz rozmiar ikon przez JADRO (opcode 48), dokladnie
               tak jak robi to SETTINGS.  Pulpit NIE zmienia tu icon_px sam:
               nowy rozmiar musi przyjsc z pollingii (opcode 49), wiec ten hook
               sprawdza CALA sciezke SETTINGS -> jadro -> pulpit. */
            else if(scan==0x42u){
                unsigned next=(unsigned)((icon_px==32)?48:((icon_px==48)?64:32));
                (void)gfx(48,next,0,0);
            }
            /* F8: park the pointer over the DATE sort button of the file window,
               so a real click can be tested against the sort bar. */
            else if(scan==0x42u){inject((int)(fb_wx+62+27),(int)(fb_wy+16+22+9),0u);
                                 copytext(status,"PTR DATE",sizeof(status));paint();}
            else if(scan==0x44u){menu_which=1;menu_open=1;menu_sel=0;paint_menu();}   /* F10: OPTIONS */
            /* Escape no longer quits the desktop: a stray key from the IKBD
               mouse/keyboard mux used to drop the user back to the shell.
               Leaving the desktop is FILE -> CLOSE only. */
            /* F9: park the pointer over the DISK C icon (no buttons) so a real
               host click can be tested against the actual IKBD path. */
            else if(scan==0x43u){inject(30,50,0u);
                                 copytext(status,"PTR C:",sizeof(status));paint();}
            if(quit)break;
            continue;
        }
        if(m.v[0]==ATOS_DISPLAY_CHANGED){
            mode_query();bg_dirty=1;paint();
            if(fb_open)fb_paint();
            if(trash_open)tr_paint();
            if(ma_open)ma_paint();
            continue;
        }
        if(m.v[0]==WMOV){
            /* The WM committed a move or a resize.  A pure MOVE only changes
               where the compositor places the client surface - the pixels are
               unchanged - so the application must NOT relayout or repaint.
               Only a size change needs a fresh layout; the WM already redrew the
               chrome and whatever the old position uncovered.  WMOV carries the
               window handle (like XaAES's WM_SIZED). */
            int16_t nx=(int16_t)(m.v[1]>>16),ny=(int16_t)(m.v[1]&0xffffu);
            uint16_t nw=(uint16_t)(m.v[2]>>16),nh=(uint16_t)m.v[2];
            uint32_t wh=m.v[3];
            if(wh==FBW&&fb_open){
                int resized=(nw&&nh&&(nw!=fb_ww||nh!=fb_wh));
                fb_wx=nx;fb_wy=ny;
                if(resized){fb_ww=nw;fb_wh=nh;fb_page=0;fb_page_calc();fb_paint();
                            desktop_save_quiet();}
            }else if(wh==TRW&&trash_open){
                int resized=(nw&&nh&&(nw!=tr_ww||nh!=tr_wh));
                tr_wx=nx;tr_wy=ny;
                if(resized){tr_ww=nw;tr_wh=nh;tr_paint();desktop_save_quiet();}
            }else if(wh==MAW&&ma_open){
                int resized=(nw&&nh&&(nw!=ma_ww||nh!=ma_wh));
                ma_wx=nx;ma_wy=ny;
                if(resized){ma_ww=nw;ma_wh=nh;ma_paint();}
            }else if(wh==ICPW&&icp_open){
                int resized=(nw&&nh&&(nw!=icp_ww||nh!=icp_wh));
                icp_wx=nx;icp_wy=ny;
                if(resized){icp_ww=nw;icp_wh=nh;icp_paint();}
            }
            continue;
        }
        if(m.v[0]==WCLS){
            /* The compositor closed one of our windows (X gadget). */
            if(m.v[1]==FBW)fb_open=0;
            else if(m.v[1]==TRW)trash_open=0;
            else if(m.v[1]==MAW)ma_open=0;
            else if(m.v[1]==ICPW)icp_open=0;
            continue;
        }
        if(m.v[0]!=KMOU)continue;
        mx=(int)(uint16_t)(m.v[1]>>16);
        my=(int)(uint16_t)(m.v[1]&0xffffu);
        buttons=(m.v[2]>>24)&7u;
        /* Timestamp recorded when the edge happened (0 = clock unavailable). */
        ev_tick=m.v[3];
        /* Info popup: any click dismisses it (and is consumed). */
        if(info_open&&((buttons&1u&&!prev_left)||(buttons&2u&&!prev_right))){
            info_open=0;bg_dirty=1;(void)gfx(GFX_OVERLAY,0,0,0);paint();
            if(buttons&1u)prev_left=1;
            if(buttons&2u)prev_right=1;
            continue;
        }
        /* Right mouse button: Windows-style context menu. */
        if((buttons&2u)&&!prev_right){
            prev_right=1;
            if(ctx_open){ctx_close_popup();}  /* erase the old popup */
            if(fb_open&&mx>=fb_wx&&mx<fb_wx+(int)fb_ww&&
               my>=fb_wy+16&&my<fb_wy+(int)fb_wh){
                int row=fb_row_at(my);
                if(row>=1&&row<=(int)fb_count)ctx_open_at(mx,my,2u,-1,row);
                else if(row==0)ctx_open_at(mx,my,0u,-1,-1);   /* ".." row: desktop ops */
            }else{
                int ic=icon_index_at(mx,my);
                ctx_open_at(mx,my,ic>=0?1u:0u,ic,-1);
            }
            continue;
        }
        if(!(buttons&2u))prev_right=0;
        /* Navigate the open popup: hover highlights, left click activates,
           a click outside (or Escape, handled below) closes it. */
        if(ctx_open){
            if(!(buttons&1u))prev_left=0;
            if((buttons&1u)&&!prev_left){
                prev_left=1;
                int hit=(mx>=ctx_x&&mx<ctx_x+CTX_W&&
                         my>=ctx_y&&my<ctx_y+(int)ctx_items*CTX_ROW+4);
                if(hit){ctx_sel=(my-(int)ctx_y-2)/CTX_ROW;ctx_activate();}
                else{ctx_close_popup();}
                continue;
            }
            if(!buttons){
                int sel=(mx>=ctx_x&&mx<ctx_x+CTX_W&&
                         my>=ctx_y&&my<ctx_y+(int)ctx_items*CTX_ROW+4)
                         ?(my-(int)ctx_y-2)/CTX_ROW:-1;
                if(sel!=ctx_sel){ctx_sel=sel;ctx_paint();}
            }
            continue;
        }
        /* Colour-icon hover: repaint only when the icon under the pointer
           changes (and no button, menu or prompt is active). */
        if(chunky&&!buttons&&!menu_open&&!run_mode&&drag_icon<0&&!icp_open&&!info_open){
            int hic=icon_index_at(mx,my);
            if(hic!=hover_icon){int old=hover_icon;hover_icon=hic;paint_hover(old,hic);}
        }
        if((buttons&1u)&&!prev_left){
            int ic;
            prev_left=1;
            if(run_mode){/* clicks do not dismiss the prompt */}
            else if(menu_open){
                int16_t bx=(int16_t)(menu_which?OPT_X:MENU_X);
                uint16_t bw=(uint16_t)(menu_which?OPT_W:MENU_W);
                unsigned items=menu_which?OPT_ITEMS:MENU_ITEMS;
                if(mx>=bx&&mx<bx+(int)bw&&my>=MENU_Y&&my<MENU_Y+(int)items*MENU_ROW)
                    menu_activate((unsigned)((my-MENU_Y)/MENU_ROW));
                else {menu_open=0;bg_dirty=1;(void)gfx(GFX_OVERLAY,0,0,0);paint();}
            }else if(mx>=8&&mx<48&&my<16){menu_which=0;menu_open=1;menu_sel=0;paint_menu();}
            else if(mx>=64&&mx<152&&my<16){menu_which=1;menu_open=1;menu_sel=0;paint_menu();}
            /* TRASH is the newest of the app's windows: test it first. */
            else if(trash_open&&mx>=tr_wx&&mx<tr_wx+(int)tr_ww&&
                    my>=tr_wy+16&&my<tr_wy+(int)tr_wh){tr_click(mx,my);}
            /* My Atari: the drive list. */
            else if(ma_open&&mx>=ma_wx&&mx<ma_wx+(int)ma_ww&&
                    my>=ma_wy+16&&my<ma_wy+(int)ma_wh){ma_click(mx,my);}
            /* Icon picker (ICON CHANGE). */
            else if(icp_open&&mx>=icp_wx&&mx<icp_wx+(int)icp_ww&&
                    my>=icp_wy+16&&my<icp_wy+(int)icp_wh){icp_click(mx,my);}
            /* File window: path bar (2..17), sort bar (20..33), column header
               (40) and rows from 52.  A sort button re-sorts the whole
               directory, not just the visible page. */
            else if(fb_open&&mx>=fb_wx&&mx<fb_wx+(int)fb_ww&&
                    my>=fb_wy+16&&my<fb_wy+(int)fb_wh){
                int srt=fb_sort_at(mx,my);
                if(srt>=0){
                    if(fb_sort!=(uint8_t)srt){
                        fb_sort=(uint8_t)srt;
                        fb_sort_all();fb_page=0;fb_page_calc();fb_paint();
                    }
                }else{
                    int row=fb_row_at(my);
                    if(row>=0&&row<=(int)fb_count+1){
                        if(fb_double(row))fb_activate(row);   /* double click opens */
                        else{fb_sel=row;fb_paint();}          /* single click selects */
                    }
                }
            }
            /* Desktop icon: pick it up; release = click, move = drag.
               (After the window tests so a window overlapping an icon cell
               - the picker sits over grid columns - still gets the click.) */
            else if((ic=icon_index_at(mx,my))>=0){
                drag_icon=ic;drag_moved=0;
                drag_ox=icons[ic].gx;drag_oy=icons[ic].gy;
                drag_gx=mx-(GRID_X+(int)icons[ic].gx*CELL_W);
                drag_gy=my-(GRID_Y+(int)icons[ic].gy*CELL_H);
            }
            /* Empty desktop: a stray click clears the icon selection. */
            else if(sel_icon>=0){sel_icon=-1;paint();}
        }else if((buttons&1u)&&drag_icon>=0){
            /* Dragging: snap the picked icon to the grid cell under the pointer,
               swapping with whatever already occupies that cell. */
            int maxc=(int)((SCRW-GRID_X)/CELL_W),maxr=(int)((SCRH-GRID_Y-48)/CELL_H);
            int gx=mx-GRID_X-drag_gx+CELL_W/2,gy=my-GRID_Y-drag_gy+CELL_H/2;
            if(gx<0)gx=0;else if(gx/CELL_W>=maxc)gx=maxc*CELL_W;
            if(gy<0)gy=0;else if(gy/CELL_H>=maxr)gy=maxr*CELL_H;
            gx/=CELL_W;gy/=CELL_H;
            if(gx!=icons[drag_icon].gx||gy!=icons[drag_icon].gy){
                unsigned j;int tj=-1;
                for(j=0;j<icon_count;++j)
                    if((int)j!=drag_icon&&icons[j].gx==(int8_t)gx&&icons[j].gy==(int8_t)gy){tj=(int)j;break;}
                dirty_add(icons[drag_icon].gx,icons[drag_icon].gy);   /* old cell */
                icons[drag_icon].gx=(int8_t)gx;icons[drag_icon].gy=(int8_t)gy;
                if(tj>=0){icons[tj].gx=(int8_t)drag_ox;icons[tj].gy=(int8_t)drag_oy;}
                drag_ox=gx;drag_oy=gy;drag_moved=1;
                paint();
            }
        }else if(!(buttons&1u)){
            if(drag_icon>=0){
                if(!drag_moved){
                    /* Release without moving = click: one click selects the
                       icon, a second click on the same icon launches it. */
                    uint32_t now=ev_tick?ev_tick:(uint32_t)hires();
                    int dbl=(dbl_icon==drag_icon)&&((uint32_t)(now-dbl_tick)<DBL_TICKS);
                    if(dbl){icon_activate(drag_icon);dbl_icon=-1;}
                    else{dbl_icon=drag_icon;dbl_tick=now;}
                    sel_icon=drag_icon;
                    paint();
                }
                drag_icon=-1;
            }
            prev_left=0;
        }
        if(quit)break;
    }
    (void)gui(GUI_END_KEYS);
    (void)gui(GUI_UNREG);
    (void)gfx(GFX_RESTORE,0,0,0);
    return 0;
}
