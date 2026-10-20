/* WINDEMO.APP - a plain application window for the kernel compositor.
 *
 * It does NOT enter desktop mode: the desktop (DESKTOP.APP or the WM
 * background) already owns the screen.  This process only asks the WM for a
 * bounded, decorated window (GFX opcode 20) and draws window-relative through
 * the gui_draw_t batch (opcode 18).  Start it in the background with
 * 'start windemo.app' while the desktop runs; 'run' (foreground) cannot attach
 * a second GUI owner and reports PROCESS START FAILED.
 */
#include "gui_app.h"

#define WINX 296
#define WINY 44
#define WINW 304
#define WINH 184
#define CLIENTH (WINH-16)

static gui_message_t m;
static int32_t gfx_pid;
static gui_draw_t list[10];
static unsigned n;

static int32_t gfx(uint32_t op,uint32_t a,uint32_t b,uint32_t c)
{
    if(send((uint32_t)gfx_pid,op,a,b,c))return -1;
    /* Wait for the reply from SYSTEM.GFX; a stray input event must not eat it. */
    for(;;){if(receive(&m,1))return -1;if(m.sender==(uint32_t)gfx_pid)return (int32_t)m.v[1];}
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
static void paint(void)
{
    n=0;
    add(GUI_FILL,0,0,WINW,CLIENTH,1,0);
    add(GUI_TEXT,8,6,0,0,0,"APP WINDOW");
    add(GUI_TEXT,8,24,0,0,0,"SEPARATE FROM DESKTOP");
    add(GUI_FRAME,8,46,(uint16_t)(WINW-16),(uint16_t)(CLIENTH-58),0,0);
    add(GUI_TEXT,16,54,0,0,0,"KERNEL WM OWNS THE FRAME");
    (void)gfx(18,(uint32_t)(uintptr_t)list,5,0);
}
int app_main(void)
{
    gfx_pid=lookup("SYSTEM.GFX");
    if(gfx_pid<0)return 1;
    /* Ask for a decorated window; client (0,0) maps to (WINX, WINY+title). */
    gfx(20,((uint32_t)WINX<<16)|WINY,((uint32_t)WINW<<16)|WINH,0);
    paint();
    /* Set the title after the window exists so the WM draws it on the title
       bar (opcode 21 also works before the first draw, but this is explicit). */
    gfx(21,(uint32_t)(uintptr_t)"WINDEMO",0,0);
    for(;;)pause_tick();
}
