/* AC.APP - Atari Commander: a graphical two-panel file manager for the ATOS
 * desktop.
 *
 * It is an ordinary application window: GFX opcode 20 asks the compositor for a
 * decorated frame, opcode 18 draws the client with the gui_draw_t batch, and
 * opcode 29 declares the document height.  It never calls GFX 13 and never
 * takes the keyboard owner, so the desktop keeps the screen and the kernel
 * focus manager hands AC the keyboard and mouse while its window is the active
 * one.  The same body can later run full screen in its own session by asking
 * for the whole screen instead of a window.
 */
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
#define SYS_DIR_PAGE 52u
#define SYS_SPAWN_ASYNC 34u
#define SYS_COPY 49u
#define SYS_MKDIR 50u
#define SYS_TRASH 53u
#define SYS_MOVE 54u
#define SYS_OPEN 2u
#define SYS_READ 3u
#define SYS_CLOSE 4u
#define SYS_ALLOC 16u

#define GUI_REG 1u
#define GUI_UNREG 2u
#define GUI_BEGIN_KEYS 6u
#define GUI_END_KEYS 7u
#define GUI_FILL 2u
#define GUI_FRAME 3u
#define GUI_TEXT 4u
#define GUI_BEVEL 6u

#define GAP 4
#define TB_H 22
#define TB_BW 74
#define TB_GAP 4
#define TB_N 7u
#define BAR_Y (TB_H+2)
#define BAR_H 18
#define ROW_H 16
#define ROW_TOP (BAR_Y+BAR_H)
#define DIR_ATTR 0x10u
#define AC_ENTRIES 160u
#define AC_CHUNK 32u
#define DBL_TICKS 5000u
#define VIEW_CAP 2048u
#define VIEW_COLS 31u

typedef struct {char name[13];uint8_t attributes,padding[2];
                uint32_t size,datetime;} dir_ent_t;

typedef struct {
    char path[64];
    dir_ent_t ent[AC_ENTRIES];
    unsigned total,top,sel;
} ac_panel_t;

static ac_panel_t pnl[2];
static dir_ent_t chunk[AC_CHUNK];
static unsigned active,rows;
static int16_t wx=8,wy=24;
static uint16_t ww=624,wh=360,client_h,pw;
static int16_t px[2];
static uint16_t scrw=640,scrh=400;
static uint8_t chunky;   /* 1 = 8 bpp colour desktop (Norton-blue theme) */
static uint8_t quit;
static unsigned prev_left;
static uint32_t click_tick;
static int click_row=-1;
static char status[40];

/* toolbar / modal state */
/* Labels come from a switch, not an array of pointers: apps are loaded at a
   runtime address and absolute pointers in initialised data are never
   relocated, so a pointer table would read garbage. */
static const char *tb_label(unsigned i)
{
    switch(i){
    case 0u:return "COPY";
    case 1u:return "MOVE";
    case 2u:return "MKDIR";
    case 3u:return "RENAME";
    case 4u:return "TRASH";
    case 5u:return "VIEW";
    default:return "QUIT";
    }
}
static uint8_t prompt,confirm;
static char prompt_buf[13],pending_name[13];
static unsigned prompt_len,last_action;
static uint8_t viewing;
static char *viewbuf;static unsigned viewlen,viewrows,viewtop;static char viewname[13];

static gui_message_t m;
static int32_t gfx_pid,gui_pid;
static uint8_t fullscreen;
static gui_draw_t list[80];
static unsigned n;

/* ---- service plumbing ----------------------------------------------------- */
static gui_message_t pend[16];
static unsigned pend_head,pend_tail;
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

/* The GUI broker (register / keyboard ownership / desktop entry) is an IPC
   service, unlike the direct-trap gui_call(). */
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
    d->kind=kind;d->x=x;d->y=y;d->w=w;d->h=h;d->color=color;d->background=1;
    if(text)copytext(d->text,text,sizeof(d->text));
}
static void flush(void)
{
    if(!n)return;
    (void)gfx(GFX_BATCH,(uint32_t)(uintptr_t)list,(uint32_t)n,0);
    n=0;
}

/* ---- geometry ------------------------------------------------------------- */
/* The layout is derived from the window size, so it follows the screen mode
   (GFX 30) and a user resize through the window frame alike. */
static void apply_size(void)
{
    client_h=(uint16_t)(fullscreen?wh:(wh-16u));
    pw=(uint16_t)((ww-3u*GAP)/2u);
    px[0]=GAP;
    px[1]=(int16_t)(GAP+pw+GAP);
    if(client_h>ROW_TOP+18u)rows=(client_h-ROW_TOP-18u)/ROW_H;else rows=1u;
}
static void layout_full(void)
{
    if(scrw<320u)scrw=320u;
    if(scrh<200u)scrh=200u;
    if(fullscreen){ww=scrw;wh=scrh;wx=0;wy=0;}
    else{
        ww=(uint16_t)(scrw>32u?scrw-16u:304u);
        wh=(uint16_t)(scrh>48u?scrh-40u:300u);
        wx=8;wy=24;
    }
    apply_size();
}

/* ---- directory model ------------------------------------------------------ */
static int ent_is_dir(const dir_ent_t *e){return (e->attributes&DIR_ATTR)!=0;}
static int ci(const char *a,const char *b)
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
static int ent_cmp(const dir_ent_t *a,const dir_ent_t *b)
{
    int da=ent_is_dir(a),db=ent_is_dir(b);
    if(da!=db)return da?-1:1;
    return ci(a->name,b->name);
}
static void copy_ent(dir_ent_t *d,const dir_ent_t *s)
{unsigned i;uint8_t *dp=(uint8_t *)d;const uint8_t *sp=(const uint8_t *)s;
 for(i=0;i<sizeof(*d);++i)dp[i]=sp[i];}
static void load_panel(unsigned p)
{
    ac_panel_t *q=&pnl[p];uint32_t got=0;
    q->total=0;q->top=0;q->sel=0;
    while(got<AC_ENTRIES){
        uint32_t want=AC_CHUNK;int32_t r;unsigned i,j;
        if(want>AC_ENTRIES-got)want=AC_ENTRIES-got;
        r=gui_call(SYS_DIR_PAGE,got,want,(void *)q->path,(void *)chunk);
        if(r<=0)break;
        for(i=0;i<(unsigned)r&&q->total<AC_ENTRIES;++i)copy_ent(&q->ent[q->total++],&chunk[i]);
        got+=(uint32_t)r;
        if((uint32_t)r<want)break;
        (void)j;
    }
    {unsigned i,k;for(i=1;i<q->total;++i){dir_ent_t x;copy_ent(&x,&q->ent[i]);k=i;
        while(k&&ent_cmp(&x,&q->ent[k-1u])<0){copy_ent(&q->ent[k],&q->ent[k-1u]);--k;}
        copy_ent(&q->ent[k],&x);}}
    q->sel=q->total?1u:0u;
}
static void enter_path(unsigned p,const char *path)
{
    unsigned i;
    for(i=0;i<63u&&path[i];++i)pnl[p].path[i]=path[i];
    pnl[p].path[i]=0;
    load_panel(p);
}
static void panel_parent(unsigned p)
{
    char next[64];unsigned i,j;const char *cur=pnl[p].path;
    for(i=0;cur[i];++i){}
    while(i>1u&&cur[i-1u]!='\\')--i;
    for(j=0;j<i&&j<63u;++j)next[j]=cur[j];
    next[j]=0;
    if(!next[0]){next[0]='\\';next[1]=0;}
    enter_path(p,next);
}
static void panel_child(unsigned p,const char *name)
{
    char next[64];unsigned i=0,j;
    for(j=0;pnl[p].path[j]&&i<60u;++j)next[i++]=pnl[p].path[j];
    if(i&&next[i-1u]!='\\')next[i++]='\\';
    for(j=0;j<12u&&name[j]&&i<62u;++j)next[i++]=name[j];
    next[i]=0;
    enter_path(p,next);
}
static int runnable(const char *nm)
{
    const char *d=0;unsigned i;char e[4];
    for(i=0;nm[i]&&i<13u;++i)if(nm[i]=='.')d=&nm[i+1];
    if(!d)return 0;
    for(i=0;i<3u;++i){char c=d[i];if(c>='a'&&c<='z')c=(char)(c-32);e[i]=c;}
    e[3]=0;
    return (e[0]=='P'&&e[1]=='R'&&e[2]=='G')||(e[0]=='A'&&e[1]=='P'&&e[2]=='P')||
           (e[0]=='T'&&e[1]=='O'&&e[2]=='S')||(e[0]=='T'&&e[1]=='T'&&e[2]=='P')||
           (e[0]=='G'&&e[1]=='T'&&e[2]=='P');
}
static void run_file(unsigned p,const char *nm)
{
    gui_spawn_t sp;unsigned i=0,j;int32_t r;
    zero(&sp,sizeof sp);
    for(j=0;pnl[p].path[j]&&i<60u;++j)sp.path[i++]=pnl[p].path[j];
    if(i&&sp.path[i-1u]!='\\')sp.path[i++]='\\';
    for(j=0;j<12u&&nm[j]&&i<62u;++j)sp.path[i++]=(char)nm[j];
    sp.path[i]=0;
    r=gui_call(SYS_SPAWN_ASYNC,0,0,&sp,0);
    copytext(status,r<0?"RUN FAILED":"STARTED",sizeof(status));
}
static unsigned row_count(void){return pnl[0].total+1u;}
static void clamp_sel(unsigned p)
{
    ac_panel_t *q=&pnl[p];
    unsigned last=row_count();
    if(!q->total)q->sel=0;
    else if(q->sel>=last)q->sel=last-1u;
    else if(q->sel>q->top+rows-1u)q->top=q->sel-(rows-1u);
    else if(q->sel<q->top)q->top=q->sel;
}
static void move_sel(unsigned p,int delta)
{
    ac_panel_t *q=&pnl[p];int last=(int)row_count();
    int s=(int)q->sel+delta;
    if(s<0)s=0;
    if(s>=last)s=last-1;
    if(s<0)s=0;
    q->sel=(unsigned)s;
    clamp_sel(p);
}
static void activate(unsigned p)
{
    ac_panel_t *q=&pnl[p];
    if(!q->total||q->sel==0u){panel_parent(p);return;}
    {const dir_ent_t *e=&q->ent[q->sel-1u];
     if(ent_is_dir(e)){panel_child(p,e->name);return;}
     if(runnable(e->name))run_file(p,e->name);}
}

/* ---- file operations ------------------------------------------------------ */
static void paint(void);
static void join_path(char *out,const char *dir,const char *name)
{
    unsigned i=0,j;
    for(j=0;dir[j]&&i<60u;++j)out[i++]=dir[j];
    if(i&&out[i-1u]!='\\')out[i++]='\\';
    for(j=0;j<12u&&name[j]&&i<63u;++j)out[i++]=name[j];
    out[i]=0;
}
static const dir_ent_t *sel_ent(unsigned p)
{
    ac_panel_t *q=&pnl[p];
    if(!q->total||q->sel==0u||q->sel>q->total)return 0;
    return &q->ent[q->sel-1u];
}
static void status_err(const char *prefix,int32_t r)
{
    unsigned n2=0;
    copytext(status,prefix,sizeof(status));
    while(status[n2])++n2;
    if(n2<sizeof(status)-6u){
        uint32_t v=(uint32_t)(-r);char rev[10];unsigned k=0;
        status[n2++]=' ';status[n2++]='-';
        if(!v)status[n2++]='0';
        else{while(v&&k<9u){rev[k++]=(char)('0'+v%10u);v/=10u;}while(k)status[n2++]=rev[--k];}
        status[n2]=0;
    }
}
static void refresh_both(void){load_panel(0);load_panel(1);paint();}
static void do_transfer(int copy)
{
    const dir_ent_t *e=sel_ent(active);char src[64],dst[64];int32_t r;unsigned other=active^1u;
    if(!e){copytext(status,"SELECT A FILE",sizeof(status));paint();return;}
    join_path(src,pnl[active].path,e->name);
    join_path(dst,pnl[other].path,e->name);
    r=gui_call(copy?SYS_COPY:SYS_MOVE,0,0,(void *)src,(void *)dst);
    if(r<0)status_err(copy?"COPY":"MOVE",r);
    else copytext(status,copy?"COPIED":"MOVED",sizeof(status));
    refresh_both();
}
static void do_trash(void)
{
    const dir_ent_t *e=sel_ent(active);char src[64];int32_t r;
    if(!e)return;
    join_path(src,pnl[active].path,e->name);
    r=gui_call(SYS_TRASH,0,0,(void *)src,0);
    copytext(status,r<0?"TRASH FAILED":"MOVED TO TRASH",sizeof(status));
    refresh_both();
}
static void do_mkdir(void)
{
    char path[64];int32_t r;
    if(!prompt_len){copytext(status,"EMPTY NAME",sizeof(status));paint();return;}
    join_path(path,pnl[active].path,prompt_buf);
    r=gui_call(SYS_MKDIR,0,0,(void *)path,0);
    copytext(status,r<0?"MKDIR FAILED":"CREATED",sizeof(status));
    refresh_both();
}
static void do_rename(void)
{
    const dir_ent_t *e=sel_ent(active);char oldp[64],newp[64];int32_t r;
    if(!e)return;
    if(!prompt_len){copytext(status,"EMPTY NAME",sizeof(status));paint();return;}
    join_path(oldp,pnl[active].path,e->name);
    join_path(newp,pnl[active].path,prompt_buf);
    r=gui_call(SYS_MOVE,0,0,(void *)oldp,(void *)newp);
    copytext(status,r<0?"RENAME FAILED":"RENAMED",sizeof(status));
    refresh_both();
}
static void do_view(void)
{
    const dir_ent_t *e=sel_ent(active);char path[64];int32_t fd,r;
    if(!e){copytext(status,"SELECT A FILE",sizeof(status));paint();return;}
    if(ent_is_dir(e)){copytext(status,"IS A DIRECTORY",sizeof(status));paint();return;}
    if(!viewbuf){viewbuf=(char *)(uintptr_t)(uint32_t)gui_call(SYS_ALLOC,VIEW_CAP,0,0,0);if(!viewbuf)return;}
    join_path(path,pnl[active].path,e->name);
    fd=gui_call(SYS_OPEN,0,0,(void *)path,0);
    if(fd<0){copytext(status,"OPEN FAILED",sizeof(status));paint();return;}
    viewlen=0;
    while(viewlen<VIEW_CAP){
        r=gui_call(SYS_READ,(uint32_t)fd,(uint32_t)(VIEW_CAP-viewlen),(void *)(viewbuf+viewlen),0);
        if(r<=0)break;
        viewlen+=(unsigned)r;
    }
    (void)gui_call(SYS_CLOSE,(uint32_t)fd,0,0,0);
    copytext(viewname,e->name,sizeof(viewname));
    viewrows=(viewlen+VIEW_COLS-1u)/VIEW_COLS;
    viewtop=0;viewing=1;
    paint();
}

/* ---- painting ------------------------------------------------------------- */
static int16_t tb_x(unsigned i){return (int16_t)(2+(int)i*(TB_BW+TB_GAP));}
static void tb_icon(unsigned i,int16_t x,int16_t y)
{
    uint8_t ink=chunky?1u:0u;   /* white glyphs on the blue bar, black in mono */
    switch(i){
    case 0:add(GUI_FRAME,x,y,10,10,ink,0);add(GUI_FRAME,(int16_t)(x+4),(int16_t)(y+4),10,10,ink,0);break;
    case 1:add(GUI_FRAME,x,y,9,10,ink,0);add(GUI_FILL,(int16_t)(x+10),(int16_t)(y+4),5,2,ink,0);
           add(GUI_FILL,(int16_t)(x+12),(int16_t)(y+2),3,6,ink,0);break;
    case 2:add(GUI_FILL,x,(int16_t)(y+3),12,8,ink,0);add(GUI_FILL,(int16_t)(x+1),y,5,4,ink,0);break;
    case 3:add(GUI_FILL,(int16_t)(x+1),(int16_t)(y+8),8,2,ink,0);add(GUI_FILL,(int16_t)(x+7),(int16_t)(y+1),3,8,ink,0);break;
    case 4:add(GUI_FILL,(int16_t)(x+1),y,10,2,ink,0);add(GUI_FRAME,(int16_t)(x+2),(int16_t)(y+3),8,8,ink,0);break;
    case 5:add(GUI_FRAME,x,y,9,9,ink,0);add(GUI_FILL,(int16_t)(x+8),(int16_t)(y+8),4,2,ink,0);break;
    default:add(GUI_FRAME,x,y,9,11,ink,0);add(GUI_FILL,(int16_t)(x+10),(int16_t)(y+4),5,2,ink,0);break;
    }
}
/* Norton-blue palette in 8 bpp; classic black/white in mono. */
static uint8_t ac_bg(void){return chunky?NC_BG:1u;}        /* panel background */
static uint8_t ac_frame(void){return chunky?NC_CYAN:0u;}   /* panel border */
static uint8_t ac_bar(void){return chunky?NC_BLUE:0u;}     /* toolbar/path/bottom bars */
static uint8_t ac_bar_text(void){return chunky?NC_CYAN:0u;}/* text on the bars */
static uint8_t ac_text(void){return chunky?1u:0u;}         /* row text */
static uint8_t ac_sel(void){return chunky?NC_BRIGHT:0u;}   /* selected row */
static uint8_t ac_sel_text(void){return chunky?0u:1u;}     /* selected row text */
static void paint_toolbar(void)
{
    unsigned i;
    n=0;
    for(i=0;i<TB_N;++i){
        int16_t x=tb_x(i);
        add(GUI_BEVEL,x,2,TB_BW,18,ac_bar(),0);
        list[n-1u].background=ac_bg();
        tb_icon(i,(int16_t)(x+4),5);
        add(GUI_TEXT,(int16_t)(x+24),3,0,0,ac_text(),tb_label(i));
        list[n-1u].background=ac_bg();
    }
    flush();
}
/* Lay a directory entry out for the CURRENT panel width.  A name that does not
   fit is shortened to "XXX..." instead of being silently cut by the window
   edge, and the size column is right-aligned to the panel (justification), so
   the list stays readable when the window is narrowed.  No font scaling: the
   grid is fixed 8x16, we only choose how many characters fit. */
static void row_text(char *out,const dir_ent_t *e)
{
    unsigned k,cols=(pw>8u)?(unsigned)((pw-8u)/8u):24u;
    unsigned name_w,namelen=0;
    if(cols>31u)cols=31u;
    if(cols<16u)cols=16u;
    for(k=0;k<31u;++k)out[k]=' ';
    out[31]=0;
    /* 15 columns to the right of the name: gap, "<DIR>" (5), gap, size (10). */
    name_w=(cols>15u)?(cols-15u):1u;
    if(name_w>12u)name_w=12u;
    for(k=0;k<12u&&e->name[k];++k)namelen=k+1;
    if(namelen>name_w&&name_w>3u){
        for(k=0;k<name_w-3u;++k)out[k]=e->name[k];
        out[name_w-3u]='.';out[name_w-2u]='.';out[name_w-1u]='.';
    }else for(k=0;k<namelen&&k<name_w;++k)out[k]=e->name[k];
    if(ent_is_dir(e)){
        out[name_w+1u]='<';out[name_w+2u]='D';out[name_w+3u]='I';
        out[name_w+4u]='R';out[name_w+5u]='>';
    }else{
        char t[12];unsigned q=0,i;uint32_t v=e->size;
        if(!v)t[q++]='0';
        else{char r[12];unsigned z=0;while(v&&z<11u){r[z++]=(char)('0'+v%10u);v/=10u;}while(z)t[q++]=r[--z];}
        if(q>10u)q=10u;
        for(i=0;i<q;++i)out[(cols-11u)-q+i]=t[i];
    }
}
static void paint_panel(unsigned p)
{
    ac_panel_t *q=&pnl[p];
    char row[34];unsigned i,last;int16_t bx=px[p],y=ROW_TOP;
    add(GUI_FILL,bx,TB_H,pw,(uint16_t)(client_h-TB_H-18),ac_bg(),0);
    add(GUI_FRAME,bx,TB_H,pw,(uint16_t)(client_h-TB_H-18),ac_frame(),0);
    add(GUI_BEVEL,(int16_t)(bx+2),BAR_Y,(uint16_t)(pw-4),BAR_H-2,ac_bar(),0);
    list[n-1u].reserved=1u;
    add(GUI_TEXT,(int16_t)(bx+4),(int16_t)(BAR_Y+1),0,0,ac_bar_text(),q->path);
    list[n-1u].background=ac_bg();
    last=q->total+1u;
    for(i=0;i<rows&&q->top+i<last;++i){
        unsigned idx=q->top+i;
        if(idx==0u){
            unsigned k;
            for(k=0;k<31u;++k)row[k]=' ';
            row[31]=0;row[0]='.';row[1]='.';
            row[13]='<';row[14]='D';row[15]='I';row[16]='R';row[17]='>';
        }else row_text(row,&q->ent[idx-1u]);
        if(p==active&&idx==q->sel)add(GUI_FILL,(int16_t)(bx+2),y,(uint16_t)(pw-4),(uint16_t)(ROW_H-1u),ac_sel(),0);
        add(GUI_TEXT,(int16_t)(bx+4),y,0,0,ac_text(),row);
        list[n-1u].background=ac_bg();
        if(p==active&&idx==q->sel){list[n-1u].color=ac_sel_text();list[n-1u].background=ac_sel();}
        y=(int16_t)(y+ROW_H);
    }
    flush();
}
/* Redraw ONE visible panel row.  Selection moves used to call paint(), which at
   1024x768 rewrote the whole client (~90 KB per arrow key); with key autorepeat
   the mailbox queued seconds of work behind the pointer.  Two rows cost ~2 KB. */
static void paint_row(unsigned p,unsigned idx)
{
    ac_panel_t *q=&pnl[p];
    char row[34];int16_t bx=px[p],y;
    if(idx<q->top||idx>=q->top+rows)return;
    y=(int16_t)(ROW_TOP+(int16_t)((idx-q->top)*ROW_H));
    if(idx==0u){
        unsigned k;
        for(k=0;k<31u;++k)row[k]=' ';
        row[31]=0;row[0]='.';row[1]='.';
        row[13]='<';row[14]='D';row[15]='I';row[16]='R';row[17]='>';
    }else{
        if(idx-1u>=q->total)return;
        row_text(row,&q->ent[idx-1u]);
    }
    n=0;
    add(GUI_FILL,(int16_t)(bx+2),y,(uint16_t)(pw-4),(uint16_t)(ROW_H-1u),
        (p==active&&idx==q->sel)?ac_sel():ac_bg(),0);
    add(GUI_TEXT,(int16_t)(bx+4),y,0,0,ac_text(),row);
    list[n-1u].background=ac_bg();
    if(p==active&&idx==q->sel){list[n-1u].color=ac_sel_text();list[n-1u].background=ac_sel();}
    flush();
}
static void paint_bottom(void)
{
    n=0;
    add(GUI_FILL,0,(int16_t)(client_h-16),ww,16,ac_bar(),0);
    /* Light text on the blue strip (inverse video in mono). */
    add(GUI_TEXT,4,(int16_t)(client_h-16),0,0,ac_bar_text(),"TAB PANEL  ENTER  BACKSP  ESC");
    list[n-1u].background=ac_bar();
    if(status[0]){
        add(GUI_TEXT,(int16_t)(ww-168),(int16_t)(client_h-16),0,0,ac_bar_text(),status);
        list[n-1u].background=ac_bar();
    }
    flush();
}
static void paint_prompt(void)
{
    n=0;
    add(GUI_FILL,150,120,340,56,1,0);
    add(GUI_FRAME,150,120,340,56,0,0);
    add(GUI_TEXT,158,126,0,0,0,prompt==1u?"NEW FOLDER NAME:":"NEW NAME:");
    add(GUI_TEXT,158,146,0,0,0,prompt_buf);
    add(GUI_TEXT,(int16_t)(158+(int)prompt_len*8),146,0,0,0,"_");
    flush();
}
static void paint_confirm(void)
{
    n=0;
    add(GUI_FILL,150,120,340,64,1,0);
    add(GUI_FRAME,150,120,340,64,0,0);
    add(GUI_TEXT,158,126,0,0,0,"MOVE TO TRASH?");
    add(GUI_TEXT,158,146,0,0,0,pending_name);
    add(GUI_TEXT,158,166,0,0,0,"ENTER=YES  ESC=NO");
    flush();
}
static void paint_view(void)
{
    unsigned i,per=(unsigned)((client_h-70u)/ROW_H);int16_t y=48;
    char line[34];
    n=0;
    add(GUI_FILL,4,26,(uint16_t)(ww-8),(uint16_t)(client_h-44),1,0);
    add(GUI_FRAME,4,26,(uint16_t)(ww-8),(uint16_t)(client_h-44),0,0);
    add(GUI_TEXT,8,28,0,0,0,"VIEW  UP/DOWN SCROLL  ESC CLOSE");
    add(GUI_TEXT,(int16_t)(ww-120),28,0,0,0,viewname);
    for(i=0;i<per&&viewtop+i<viewrows;++i){
        unsigned off=(viewtop+i)*VIEW_COLS,k,c=0;
        for(k=0;k<VIEW_COLS&&off+k<viewlen;++k){
            char ch=viewbuf[off+k];
            line[c++]=(ch=='\t')?' ':((ch>=32&&ch<127)?ch:'.');
        }
        line[c]=0;
        add(GUI_TEXT,8,y,0,0,0,line);
        y=(int16_t)(y+ROW_H);
    }
    flush();
}
static void paint(void)
{
    unsigned i;
    if(viewing){paint_view();return;}
    n=0;add(GUI_FILL,0,0,ww,client_h,ac_bg(),0);flush();
    paint_toolbar();
    for(i=0;i<2u;++i)paint_panel(i);
    paint_bottom();
    if(confirm)paint_confirm();
    if(prompt)paint_prompt();
    if(!fullscreen)(void)gfx(GFX_CONTENT,(uint32_t)client_h,0,0);
}

/* ---- input ---------------------------------------------------------------- */
static int panel_hit(int mx,int my,unsigned *row)
{
    unsigned p;int sy=my-(int)wy-16;
    for(p=0;p<2u;++p)if(mx>=px[p]&&mx<px[p]+(int)pw){
        if(sy>=ROW_TOP&&sy<ROW_TOP+(int)rows*ROW_H){*row=(unsigned)((sy-ROW_TOP)/ROW_H);return (int)p;}
        return (int)p;
    }
    return -1;
}
static int tb_hit(int cx,int cy)
{
    unsigned i;
    if(cy<2||cy>=TB_H)return -1;
    for(i=0;i<TB_N;++i){int16_t x=tb_x(i);if(cx>=x&&cx<x+TB_BW)return (int)i;}
    return -1;
}
static void start_prompt(unsigned mode)
{
    prompt=(uint8_t)mode;prompt_len=0;prompt_buf[0]=0;paint();
}
static void toolbar_action(unsigned i)
{
    if(i==0u)do_transfer(1);
    else if(i==1u)do_transfer(0);
    else if(i==2u)start_prompt(1u);
    else if(i==3u){if(sel_ent(active))start_prompt(2u);else{copytext(status,"SELECT A FILE",sizeof(status));paint();}}
    else if(i==4u){const dir_ent_t *e=sel_ent(active);if(e){copytext(pending_name,e->name,sizeof(pending_name));confirm=1u;paint();}}
    else if(i==5u)do_view();
    else quit=1u;
}
static void mouse_click(int mx,int my)
{
    unsigned row=0;int p;
    int16_t cx=(int16_t)(mx-(int)wx),cy=(int16_t)(my-(int)wy-16);
    if(viewing){viewing=0;paint();return;}
    if(confirm||prompt)return;
    {int tb=tb_hit(cx,cy);if(tb>=0){last_action=(unsigned)tb;toolbar_action((unsigned)tb);return;}}
    p=panel_hit(mx,my,&row);
    if(p<0)return;
    {unsigned old_active=active;active=(unsigned)p;
     if(row<rows&&(pnl[p].top+row)<pnl[p].total+1u){
        unsigned idx=pnl[p].top+row,old_sel=pnl[p].sel,old_top=pnl[p].top;
        int dbl=(click_row==(int)idx&&(uint32_t)(hires()-click_tick)<DBL_TICKS);
        click_row=(int)idx;click_tick=(uint32_t)hires();
        pnl[p].sel=idx;
        clamp_sel((unsigned)p);
        if(dbl){activate((unsigned)p);return;}
        /* Selecting a row changes exactly two rows: repainting the whole window
           cost ~90 KB per click at 1024x768. */
        if((unsigned)p==old_active&&pnl[p].top==old_top&&old_sel!=pnl[p].sel){
            paint_row((unsigned)p,old_sel);
            paint_row((unsigned)p,pnl[p].sel);
            return;
        }
     }}
    paint();
}
static void key_event(uint32_t scan,uint32_t ch)
{
    if(viewing){
        if(scan==0x48u){if(viewtop)--viewtop;}
        else if(scan==0x50u){if(viewtop+1u<viewrows)++viewtop;}
        else if(scan==0x01u||scan==0x1cu||scan==0x10u)viewing=0;
        paint();return;
    }
    if(confirm){
        if(scan==0x1cu||scan==0x15u){confirm=0;do_trash();}
        else if(scan==0x01u||scan==0x10u){confirm=0;paint();}
        return;
    }
    if(prompt){
        if(scan==0x1cu){
            unsigned mode=prompt;prompt=0;
            if(mode==1u)do_mkdir();else do_rename();
        }else if(scan==0x01u){prompt=0;paint();}
        else if(scan==0x0eu){if(prompt_len){prompt_buf[--prompt_len]=0;paint();}}
        else if(ch>=32u&&ch<127u&&prompt_len<12u){
            prompt_buf[prompt_len++]=(char)ch;prompt_buf[prompt_len]=0;paint();
        }
        return;
    }
    /* Norton/MC-compatible function keys, mirroring the toolbar. */
    if(scan==0x3cu){if(sel_ent(active))start_prompt(2u);else{copytext(status,"SELECT A FILE",sizeof(status));paint();}return;} /* F2 rename */
    if(scan==0x3du){do_view();return;}                          /* F3 view */
    if(scan==0x3fu){do_transfer(1);return;}                     /* F5 copy */
    if(scan==0x40u){do_transfer(0);return;}                     /* F6 move */
    if(scan==0x41u){start_prompt(1u);return;}                   /* F7 mkdir */
    if(scan==0x42u){const dir_ent_t *e2=sel_ent(active);        /* F8 trash */
        if(e2){copytext(pending_name,e2->name,sizeof(pending_name));confirm=1u;paint();}return;}
    if(scan==0x44u){quit=1;return;}                             /* F10 quit */
    if(scan==0x0fu){active^=1u;paint();return;}                 /* TAB */
    if(scan==0x48u||scan==0x50u){                               /* UP / DOWN */
        unsigned old_sel=pnl[active].sel,old_top=pnl[active].top;
        move_sel(active,scan==0x48u?-1:1);
        if(pnl[active].top==old_top){
            /* Only the two affected rows change: repaint them, not the window. */
            paint_row(active,old_sel);
            paint_row(active,pnl[active].sel);
        }else paint_panel(active);                              /* list scrolled */
        return;
    }
    if(scan==0x1cu||scan==0x72u){activate(active);paint();return;} /* ENTER */
    if(scan==0x0eu){panel_parent(active);paint();return;}       /* BACKSPACE */
    if(scan==0x01u||scan==0x10u){quit=1;return;}                /* ESC / Q */
    (void)last_action;
}

int app_main(void)
{
    int32_t v;
    gfx_pid=lookup("SYSTEM.GFX");
    if(gfx_pid<0)return 1;
    gui_pid=lookup("SYSTEM.GUI");
    v=gfx(GFX_VIDEO_MODE,0,0,0);
    if(v>0){scrw=(uint16_t)((uint32_t)v>>16);scrh=(uint16_t)((uint32_t)v&0xffffu);}
    chunky=(uint8_t)(scrh>=480u);   /* 800x600x256 vs 640x400 mono */
    /* Environment: if no other GUI application owns the keyboard, AC takes the
       whole screen - that is how it runs from the console or as its own session
       desktop.  Inside a running desktop the ownership is already taken, so AC
       becomes an ordinary window instead. */
    if(gui_op(GUI_BEGIN_KEYS)==0){
        if(gfx(GFX_DESKTOP,0,0,0)==0)fullscreen=1;
        else (void)gui_op(GUI_END_KEYS);
    }
    layout_full();
    enter_path(0,"\\");
    enter_path(1,"\\SYSTEM\\APP");
    if(!fullscreen){
        (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,
                  ((uint32_t)ww<<16)|wh,0u);
        (void)gfx(GFX_TITLE,(uint32_t)(uintptr_t)"ATARI COMMANDER",0,0);
    }
    copytext(status,fullscreen?"FULL SCREEN":"READY",sizeof(status));
    paint();
    for(;;){
        if(!pend_pop(&m)&&receive(&m,1)){pause_tick();continue;}
        if(m.sender)continue;
        if(m.v[0]==KKEY){
            if(((m.v[1]>>24)&0xffu)==1u)key_event(m.v[1]&0x7fu,m.v[2]);
        }else if(m.v[0]==KMOU){
            int mx=(int)(uint16_t)(m.v[1]>>16),my=(int)(uint16_t)(m.v[1]&0xffffu);
            unsigned buttons=(m.v[2]>>24)&7u;
            if((buttons&1u)&&!prev_left)mouse_click(mx,my);
            prev_left=buttons&1u;
        }else if(m.v[0]==WMOV){
            int16_t nx=(int16_t)(m.v[1]>>16),ny=(int16_t)(m.v[1]&0xffffu);
            uint16_t nw=(uint16_t)(m.v[2]>>16),nh=(uint16_t)m.v[2];
            int resized=(nw&&nh&&(nw!=ww||nh!=wh));
            /* A pure MOVE only changes where the compositor places the client
               surface; the pixels are unchanged, so only the hit-test origin
               moves.  The video mode is queried on a RESIZE only: it used to be
               a synchronous round trip on EVERY pointer move, and a resolution
               change that does not resize this window leaves a client-relative
               layout valid anyway. */
            if(resized){
                int32_t v2=gfx(GFX_VIDEO_MODE,0,0,0);
                if(v2>0){
                    uint16_t sw=(uint16_t)((uint32_t)v2>>16),sh=(uint16_t)((uint32_t)v2&0xffffu);
                    if(sw>=320u&&sh>=200u&&(sw!=scrw||sh!=scrh)){
                        /* The desktop changed the resolution under us: re-fit the
                           whole window to the new screen and re-lay out. */
                        scrw=sw;scrh=sh;layout_full();
                        (void)gfx(GFX_WINDOW,((uint32_t)(uint16_t)wx<<16)|(uint16_t)wy,
                                  ((uint32_t)ww<<16)|wh,0u);
                        paint();continue;
                    }
                }
                wx=nx;wy=ny;ww=nw;wh=nh;apply_size();paint();
            }else{wx=nx;wy=ny;}
        }else if(m.v[0]==WCLS){quit=1;}
        if(quit)break;
    }
    if(fullscreen){(void)gui_op(GUI_END_KEYS);(void)gfx(GFX_RESTORE,0,0,0);}
    return 0;
}
