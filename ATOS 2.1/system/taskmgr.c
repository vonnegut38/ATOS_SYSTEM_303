/* TASKMGR 0.1 - process manager.  Lists the six user process slots with their
   name, pid, state, CPU ticks and private memory, and can kill / suspend /
   resume a selected process.  Runs as an ordinary compositor window. */
#include "gui_app.h"

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

#define WW 480u
#define WH 320u
#define CLIENT_H (WH-16u)

/* process-manager syscalls (added alongside call 67) */
#define SYS_NAME 68u
#define SYS_RES  69u
#define SYS_CPU  70u
#define SYS_KILL 71u
#define SYS_SUSP 72u
#define SYS_RSME 73u

#define SLOTS 12u   /* == USER_PROCESS_SLOTS w kernel_exception.c */
#define ROW_H 16u   /* 12 wierszy miesci sie w oknie 480x320 */

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static gui_draw_t list[160];
static unsigned n;
static uint8_t quit,fullscreen,chunky;
static unsigned prev_left;
static int16_t wx=100,wy=80;
static uint16_t scrw=640,scrh=400;
static char status[79];
static int sel=-1;

typedef struct {uint32_t pid;uint8_t state;char name[13];
                uint32_t cpu,priv;} proc_row;   /* cpu = %, priv = total bytes */
static proc_row rows[SLOTS];
static uint32_t prev_cpu[SLOTS],prev_wall;
static uint8_t first_snap=1;

/* ---- service plumbing (same as SYSINFO) --------------------------------- */
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
static const char *state_name(uint8_t s)
{
    switch(s){
    case 0u:return "FREE";case 1u:return "READY";case 2u:return "RUN";
    case 3u:return "SLEEP";case 4u:return "WAIT";case 5u:return "ZOMBIE";
    case 6u:return "INPUT";case 7u:return "SUSP";case 8u:return "MSG";
    default:return "?";
    }
}

/* ---- process table snapshot --------------------------------------------- */
static void snapshot(void)
{
    unsigned i;
    uint32_t wall_now=(uint32_t)gui_call(58,0,0,0,0);   /* 50 Hz wall clock */
    uint32_t elapsed=wall_now-prev_wall;
    prev_wall=wall_now;
    for(i=0;i<SLOTS;++i){
        uint32_t info[4];
        info[0]=info[1]=info[2]=info[3]=0;
        rows[i].pid=0;rows[i].state=0;rows[i].name[0]=0;rows[i].cpu=0;rows[i].priv=0;
        if(gui_call(20,i,0,info,0))continue;
        if(!info[0]){prev_cpu[i]=0;continue;}
        rows[i].pid=info[0];rows[i].state=(uint8_t)info[1];
        (void)gui_call(SYS_NAME,i,0,rows[i].name,0);
        {uint32_t res[4];
         res[0]=res[1]=res[2]=res[3]=0;
         if(!gui_call(SYS_RES,i,0,res,0))rows[i].priv=res[0]+res[1]+res[2];} /* image+stack+heap */
        {uint32_t c=(uint32_t)gui_call(SYS_CPU,i,0,0,0);
         if(!first_snap&&elapsed&&c>=prev_cpu[i])rows[i].cpu=(c-prev_cpu[i])*100u/elapsed;
         prev_cpu[i]=c;}
    }
    first_snap=0;
    if(sel>=(int)SLOTS)sel=-1;
    if(sel>=0&&!rows[sel].pid)sel=-1;
}

/* ---- drawing -------------------------------------------------------------- */
static void button(int x,int y,int w,const char *label,int active)
{
    add(GUI_BEVEL,(int16_t)x,(int16_t)y,(uint16_t)w,20,0,0);
    list[n-1u].background=1u;   /* bevel highlight stays white */
    if(active){list[n-1u].reserved=1u;add(GUI_FILL,(int16_t)(x+1),(int16_t)(y+1),(uint16_t)(w-2),18,chunky?3u:0u,0);}
    add(GUI_TEXT,(int16_t)(x+6),(int16_t)(y+3),0,0,chunky?0u:0u,label);
    if(active){list[n-1u].color=chunky?0u:1u;list[n-1u].background=chunky?3u:0u;}
}
static void paint(void)
{
    unsigned i;int y;
    n=0;
    add(GUI_FILL,0,0,WW,CLIENT_H,chunky?15u:1u,0);
    add(GUI_TEXT,8,0,0,0,chunky?0u:0u,"PROCESS MANAGER");
    add(GUI_TEXT,16,22,0,0,chunky?0u:0u,"NAME");
    add(GUI_TEXT,160,22,0,0,chunky?0u:0u,"PID");
    add(GUI_TEXT,220,22,0,0,chunky?0u:0u,"STATE");
    add(GUI_TEXT,300,22,0,0,chunky?0u:0u,"CPU%");
    add(GUI_TEXT,380,22,0,0,chunky?0u:0u,"RAM");
    add(GUI_FRAME,8,38,WW-16,SLOTS*ROW_H+8u,0,0);
    for(i=0;i<SLOTS;++i){
        char s[32];y=44+(int)i*(int)ROW_H;
        if((int)i==sel){add(GUI_FILL,12,y-2,WW-24,14,chunky?3u:0u,0);}
        if(!rows[i].pid){
            add(GUI_TEXT,16,y,0,0,chunky?0u:0u,"(free)");
            continue;
        }
        add(GUI_TEXT,16,y,0,0,chunky?0u:0u,rows[i].name);
        s[0]=0;addnum(s,rows[i].pid);add(GUI_TEXT,160,y,0,0,chunky?0u:0u,s);
        add(GUI_TEXT,220,y,0,0,chunky?0u:0u,state_name(rows[i].state));
        s[0]=0;addnum(s,rows[i].cpu);append(s,"%");add(GUI_TEXT,300,y,0,0,chunky?0u:0u,s);
        s[0]=0;addnum(s,rows[i].priv/1024u);append(s,"K");add(GUI_TEXT,380,y,0,0,chunky?0u:0u,s);
    }
    button(8,(int)(CLIENT_H-24),90,"KILL",sel>=0);
    button(104,(int)(CLIENT_H-24),90,"SUSPEND",sel>=0);
    button(200,(int)(CLIENT_H-24),90,"RESUME",sel>=0);
    button(296,(int)(CLIENT_H-24),90,"REFRESH",0);
    button((int)(WW-98),(int)(CLIENT_H-24),90,"QUIT",0);
    if(status[0])add(GUI_TEXT,8,(int)(CLIENT_H-44),0,0,chunky?0u:0u,status);
    flush();
    (void)gfx(GFX_CONTENT,CLIENT_H,0,0);
}

/* ---- actions -------------------------------------------------------------- */
static void act_kill(void)
{
    int r;
    if(sel<0||!rows[sel].pid)return;
    r=gui_call(SYS_KILL,rows[sel].pid,0,0,0);
    if(r<=0){                      /* running/waiting: suspend first */
        (void)gui_call(SYS_SUSP,rows[sel].pid,0,0,0);
        r=gui_call(SYS_KILL,rows[sel].pid,0,0,0);
    }
    copytext(status,r>0?"KILLED":"KILL FAILED",79);
    sel=-1;snapshot();paint();
}
static void act_suspend(void)
{
    int r;
    if(sel<0||!rows[sel].pid)return;
    r=gui_call(SYS_SUSP,rows[sel].pid,0,0,0);
    copytext(status,r>0?"SUSPENDED":r<0?"NOT SUSPENDABLE":"NO PROC",79);
    snapshot();paint();
}
static void act_resume(void)
{
    int r;
    if(sel<0||!rows[sel].pid)return;
    r=gui_call(SYS_RSME,rows[sel].pid,0,0,0);
    copytext(status,r>0?"RESUMED":r<0?"NOT SUSPENDED":"NO PROC",79);
    snapshot();paint();
}
static void mouse_click(int mx,int my)
{
    int cx=fullscreen?mx:mx-(int)wx,cy=fullscreen?my:my-(int)wy-16;
    int i;
    if(cy<0||cx<0||cx>=(int)(fullscreen?scrw:WW))return;
    /* process rows */
    if(cy>=42&&cy<42+(int)SLOTS*(int)ROW_H){
        i=(cy-42)/(int)ROW_H;
        if(cx>=8&&cx<(int)WW-8){sel=(rows[i].pid)?i:-1;paint();}
        return;
    }
    /* bottom buttons */
    if(cy>=(int)(fullscreen?scrh:CLIENT_H)-24&&cy<(int)(fullscreen?scrh:CLIENT_H)-4){
        if(cx>=8&&cx<98)act_kill();
        else if(cx>=104&&cx<194)act_suspend();
        else if(cx>=200&&cx<290)act_resume();
        else if(cx>=296&&cx<386){snapshot();copytext(status,"REFRESHED",79);paint();}
        else if(cx>=(int)(WW-98)&&cx<(int)(WW-8)){quit=1;}
    }
}
static void key_event(uint32_t scan)
{
    if(scan==1||scan==0x44){quit=1;return;}
    if(scan==0x48u){if(sel>0)--sel;else sel=(int)SLOTS-1;paint();return;}       /* up */
    if(scan==0x50u){if(sel<(int)SLOTS-1)++sel;else sel=0;paint();return;}        /* down */
    if(scan==0x3eu){act_kill();return;}                                          /* F4 */
    if(scan==0x3fu){act_suspend();return;}                                       /* F5 */
    if(scan==0x40u){act_resume();return;}                                        /* F6 */
    if(scan==0x3bu){snapshot();copytext(status,"REFRESHED",79);paint();return;}  /* F1 */
}

int app_main(void)
{
    uint32_t last_refresh=0;
    gfx_pid=lookup("SYSTEM.GFX");if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");if(gui_pid<0)return 1;
    {int32_t v=gfx(GFX_VIDEO_MODE,0,0,0);
     if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
     chunky=(uint8_t)(gfx(35u,0,0,0)==8u);}
    if(gui_op(GUI_BEGIN_KEYS)==0){
        if(gfx(GFX_DESKTOP,0,0,0)==0)fullscreen=1;
        else (void)gui_op(GUI_END_KEYS);
    }
    wx=(int16_t)(((int)scrw-(int)WW)/2);if(wx<8)wx=8;
    wy=(int16_t)(((int)scrh-(int)WH)/2);if(wy<8)wy=8;
    if(!fullscreen){
        (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,((uint32_t)WW<<16)|WH,0u);
        (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"PROCESSES",0,0);
    }
    snapshot();paint();
    for(;;){
        uint32_t now;int got=pend_pop(&m);
        if(!got){got=receive(&m,1);if(got)pause_tick();}
        if(got&&m.sender){
            if(m.v[0]==KKEY){if(((m.v[1]>>24)&0xffu)==1u)key_event(m.v[1]&0x7fu);}
            else if(m.v[0]==KMOU){int mx=(int)(uint16_t)(m.v[1]>>16),my=(int)(uint16_t)(m.v[1]&0xffffu);
                unsigned buttons=(m.v[2]>>24)&7u;
                if((buttons&1u)&&!prev_left)mouse_click(mx,my);
                prev_left=buttons&1u;}
            else if(m.v[0]==WMOV){wx=(int16_t)(m.v[1]>>16);wy=(int16_t)(m.v[1]&0xffffu);}
            else if(m.v[0]==WCLS)quit=1;
        }
        m.sender=0;
        if(quit)break;
        /* live refresh ~2 Hz (takze po komunikacie; wczesniej "continue"
           potrafil wiecznie pomijac odswiezanie przy zaleglym senderze) */
        now=(uint32_t)hires();
        if((uint32_t)(now-last_refresh)>=6144u){last_refresh=now;snapshot();paint();}
    }
    if(fullscreen){(void)gui_op(GUI_END_KEYS);(void)gfx(GFX_RESTORE,0,0,0);}
    return 0;
}
