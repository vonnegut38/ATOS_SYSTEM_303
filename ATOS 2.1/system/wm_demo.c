/* WMDEMO.APP - minimal batch-based desktop client.
 *
 * It follows the DESKTOP.APP entry protocol (register with SYSTEM.GUI, become
 * the keyboard owner, ask SYSTEM.GFX for the desktop) but draws through the
 * gui_draw_t batch opcode (18) instead of the legacy direct primitives.  That
 * is the path kernel_wm_app_surface() + the kernel compositor expect, so with
 * CONFIG_WM it paints into a WM window surface that the compositor shows.
 */
#include "gui_app.h"

static gui_message_t reply;
static int32_t gfx_pid,gui_pid;
static gui_draw_t list[8];

static void print(const char *s){gui_call(1,0,0,(void *)s,0);}

/* One 4-long message and its authenticated reply; returns reply status. */
static int32_t call_wait(int32_t dest,uint32_t a,uint32_t b,uint32_t c,uint32_t d)
{
    if(send((uint32_t)dest,a,b,c,d))return -1;
    for(;;){if(receive(&reply,1))return -1;if(reply.sender==(uint32_t)dest)return (int32_t)reply.v[1];}
}

static void item(gui_draw_t *d,uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,
                 uint16_t color,const char *text)
{
    unsigned i;uint8_t *p=(uint8_t *)d;
    for(i=0;i<sizeof(*d);i++)p[i]=0;
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=1;
    if(text)copytext(d->text,text,sizeof(d->text));
}

int app_main(void)
{
    gfx_pid=lookup("SYSTEM.GFX");
    gui_pid=lookup("SYSTEM.GUI");
    if(gfx_pid<0||gui_pid<0){print("WMDEMO: NO SERVICES\n");return 1;}
    call_wait(gui_pid,1,0,0,0);          /* register with the GUI service */
    call_wait(gui_pid,6,0,0,0);          /* become the keyboard owner */
    if(call_wait(gfx_pid,13,0,0,0)){print("WMDEMO: DESKTOP FAILED\n");return 2;}
    item(&list[0],GUI_BG,0,0,640,400,0,0);
    item(&list[1],GUI_WINDOW,80,40,300,180,1,"ATOS WM DEMO");
    item(&list[2],GUI_FILL,96,80,268,24,0,0);
    item(&list[3],GUI_TEXT,104,84,0,0,1,"BATCH DRAW VIA KERNEL WM");
    item(&list[4],GUI_FRAME,96,120,268,60,0,0);
    item(&list[5],GUI_TEXT,104,132,0,0,1,"HELLO FROM WMDEMO.APP");
    if(call_wait(gfx_pid,18,(uint32_t)(uintptr_t)list,6,0)){
        print("WMDEMO: BATCH FAILED\n");return 3;}
    print("WMDEMO: PAINTED\n");
    for(;;)pause_tick();                 /* stay alive as the desktop owner */
}
