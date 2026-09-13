/* TOS personality (T1): load a 0x601A .PRG, build its basepage, relocate it
 * and run it in supervisor mode with the TOS trap vectors installed.
 *
 * TOS convention: the function number is in D0 (word) and the parameters are
 * pushed on the stack.  On the 68010+ a TRAP pushes an 8-byte frame
 * (format/vector word, SR, PC), so the first argument is at frame+8. */
#include <stdint.h>
#include "kernel_memory.h"
#include "kernel_fat.h"
#include "kernel_block.h"
#include "kernel_graphics.h"
#include "tos_prg.h"

/* Console shadow kept by kernel_module.c (G0-2). */
extern int32_t kernel_console_log_count(void);
extern const char *kernel_console_log_line(int32_t index);
extern int32_t kernel_ikbd_raw_last(uint32_t back);
extern int32_t kernel_ikbd_drop_count(void);
extern int32_t kernel_ikbd_raw_total(void);

extern void asf_tos_enter(void *basepage,void *entry,void *stack_top);
extern void asf_tos_exit(void);
extern void asf_tos_trap1(void);
extern void asf_tos_trap2(void);
extern void asf_tos_trap13(void);
extern void asf_tos_trap14(void);
extern void asf_tos_linea(void);
extern void asf_tos_fault2(void);
extern void asf_tos_fault3(void);
extern void asf_tos_fault4(void);
extern void asf_tos_fault5(void);
extern void asf_tos_fault6(void);
extern void asf_tos_fault7(void);
extern void asf_tos_fault8(void);
extern void asf_tos_fault11(void);
extern void kernel_image_cache_sync(uint32_t cpu_type);
extern volatile uint32_t kernel_timer_ticks;
extern const char *kernel_user_get_arguments(void);

static void (*tos_puts)(const char *);
static uint8_t tos_cpu_type;
static int32_t tos_exit_code;
static uint32_t tos_saved_vec[13];
static uint8_t *tos_screen;          /* 32 KB TOS screen for the program */
static uint16_t tos_date=0x5d2bu,tos_time=0x6000u;   /* software GEMDOS clock */
static uint8_t tos_window_enabled;      /* T2 window is opt-in (XBIOS 0xFE cmd 7) */
static uint8_t tos_desktop_active;      /* the graphics desktop is on screen */
static void tos_desktop_leave(void);
static uint8_t  tos_mode=2u;         /* 0 low, 1 medium, 2 high */
static uint16_t tos_palette[16];

static int32_t (*tos_input_pop)(void);

void kernel_tos_set_console(void (*puts)(const char *)){tos_puts=puts;}
void kernel_tos_set_cpu(uint8_t cpu_type){tos_cpu_type=cpu_type;}
void kernel_tos_set_input(int32_t (*pop)(void)){tos_input_pop=pop;}

/* A fault inside the TOS program ends it and returns to the shell. */
void asf_tos_fault(uint32_t vector)
{
    tos_exit_code=-(int32_t)(vector&0xffffu);
    asf_tos_exit();
    for(;;){}
}

static uint8_t tos_shift,tos_caps;
static void tos_key_update(uint8_t sc)
{
    uint8_t base=(uint8_t)(sc&0x7fu);
    int up=(sc&0x80u)!=0;
    if(base==0x2au||base==0x36u)tos_shift=up?0u:1u;
    if(base==0x3au&&!up)tos_caps^=1u;
}
static int tos_key_ascii(uint8_t sc,uint8_t shift,uint8_t caps)
{
    static const char map[128]={
        0,0x1b,'1','2','3','4','5','6','7','8','9','0','-','=',0x08,0x09,
        'q','w','e','r','t','y','u','i','o','p','[',']',0x0d,0,
        'a','s','d','f','g','h','j','k','l',';',0x27,0x60,0,
        0x5c,'z','x','c','v','b','n','m',',','.','/',0,0,0,' ',0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    static const char shft[128]={
        0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
        0,0,0,0,0,0,0,0,0,0,'{','}',0,0,
        0,0,0,0,0,0,0,0,0,':','"','~',0,
        '|',0,0,0,0,0,0,0,'<','>','?',0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int c;
    if(sc>=128u)return 0;
    c=map[sc];
    if(!c)return 0;
    if(c>='a'&&c<='z'){if(shift^caps)c-=32;return c;}
    if(shift){int s=shft[sc];if(s)return s;}
    return c;
}
static int32_t tos_key_pending=-2;   /* -2 none buffered, >=0 key */
static uint8_t tos_last_scancode;    /* diagnostic: byte that produced a key */
static int32_t tos_next_scancode(void);

/* The raw IKBD stream also carries mouse packets (0xF8-0xFB header plus two
 * coordinate bytes) and status/reset bytes (>=0xF0).  A coordinate byte must
 * never be delivered as a keypress, otherwise moving the mouse would end a
 * program waiting in Cconin. */
static uint8_t tos_rawlog[16];
static uint8_t tos_rawlog_n,tos_rawlog_i;
static uint8_t tos_resync;           /* an overrun lost a byte: reframe */
static uint8_t tos_mouse_skip;       /* mouse bytes still expected in the frame */
static void tos_rawlog_add(uint8_t b)
{
    tos_rawlog[tos_rawlog_i]=b;
    tos_rawlog_i=(uint8_t)((tos_rawlog_i+1u)&15u);
    if(tos_rawlog_n<16u)++tos_rawlog_n;
}
static int32_t tos_next_scancode(void)
{
    uint8_t *skip=&tos_mouse_skip;
    if(!tos_input_pop)return -1;
    for(;;){
        int32_t v=tos_input_pop();
        uint8_t sc;
        if(v<0)return -1;
        sc=(uint8_t)v;
        tos_rawlog_add(sc);
        /* NOTE: an OVRN-based resync ("discard until the next 0xF8-0xFF
           header") was tried here and had to be removed: the emulated ACIA
           keeps bit 5 set often, so it discarded ordinary key scancodes until
           none were left (typing lost/merged commands). */
        (void)tos_resync;
        if(*skip){--*skip;continue;}
        /* A relative mouse packet is header + X + Y.  Hatari (and real
           IKBDs in "both mouse and joystick" mode) emit headers across the
           whole 0xF8-0xFF range - 0xFE in particular.  Treating 0xFE as a
           lone status byte shifted the frame by one and leaked a coordinate
           (raw: F8 D0 F8 FE FB F8 F8 00 1C). */
        if(sc>=0xf8u){*skip=2u;continue;}
        if(sc>=0xf0u)continue;
        return (int32_t)sc;
    }
}

/* Diagnostic: the last raw IKBD bytes the TOS path saw, so a leaked mouse
 * coordinate can be traced (was the 0xF8-0xFB header there at all?). */
/* TOS key queue.  A raw ring overflow used to drop a mouse packet header and
 * leak a coordinate byte as a keypress (0x1C closed the program waiting in
 * Cconin).  The timer drains the ring with full framing, so nothing overflows
 * while the program is busy drawing; Cconin reads from this queue. */
#define TOS_KEYQ 16
static int32_t tos_keyq[TOS_KEYQ];
static uint8_t tos_keyq_head,tos_keyq_tail;
static uint8_t tos_active;

void tos_poll_input(void)
{
    int guard=64;
    while(guard-->0){
        int32_t sc=tos_next_scancode();
        uint8_t next;
        int a;
        if(sc<0)break;
        tos_key_update((uint8_t)sc);
        a=tos_key_ascii((uint8_t)sc,tos_shift,tos_caps);
        if(!a)continue;
        next=(uint8_t)((tos_keyq_head+1u)%TOS_KEYQ);
        if(next==tos_keyq_tail)break;                 /* queue full: drop */
        tos_keyq[tos_keyq_head]=(sc<<16)|a;
        tos_keyq_head=next;
    }
}
static int32_t tos_keyq_pop(void)
{
    int32_t k;
    if(tos_keyq_tail==tos_keyq_head)return -1;
    k=tos_keyq[tos_keyq_tail];
    tos_keyq_tail=(uint8_t)((tos_keyq_tail+1u)%TOS_KEYQ);
    tos_last_scancode=(uint8_t)(k>>16);
    return k;
}
/* GEMDOS Cconin blocks until a key arrives.  A bounded loop used to return a
 * fake Enter after a couple of seconds, which silently ended programs that
 * were waiting for input (and closed their desktop window).  Timer C keeps
 * running while we wait, so the compositor and the clock stay alive. */
static int32_t tos_getkey(void)
{
    if(tos_key_pending>=0){int32_t k=tos_key_pending;tos_key_pending=-2;return k;}
    if(!tos_input_pop)return 0x0d;
    /* Only the Timer C fills the queue.  Draining here as well raced the
       timer: a nested poll corrupted the mouse framing and leaked a
       coordinate byte as a keypress. */
    for(;;){
        int32_t k=tos_keyq_pop();
        if(k>=0)return k;
    }
}
static int32_t tos_key_ready(void)
{
    if(tos_key_pending>=0)return 1;
    if(tos_keyq_head!=tos_keyq_tail){tos_key_pending=tos_keyq_pop();return 1;}
    return 0;
}

static void tos_print(const char *s){if(tos_puts&&s)tos_puts(s);}
static void tos_dec(uint32_t v){char b[12];int i=0,n=0;char t[12];
    if(!v){tos_print("0");return;}
    while(v){t[n++]=(char)('0'+v%10u);v/=10u;}
    while(n)b[i++]=t[--n];
    b[i]=0;tos_print(b);}
static void tos_putc(char c){char b[2];b[0]=c;b[1]='\0';tos_print(b);}
static uint32_t rd32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];}
static uint16_t rd16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void wr32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}

/* TOS traps are stack based.  From supervisor mode the arguments sit above
 * the 8-byte exception frame; from user mode the CPU pushed that frame on the
 * supervisor stack and the arguments are still on the user stack. */
static uint16_t *tos_args(uint32_t *frame,uint32_t usp)
{
    uint16_t sr=rd16((const uint8_t *)frame);
    if(sr&0x2000u)return (uint16_t *)((uint8_t *)frame+8u);
    return (uint16_t *)(uintptr_t)usp;
}

/* --- program screen access (mono 2 / medium 1 / low 0) --------------------- */
static void tos_putpixel(int x,int y,uint8_t color)
{
    if(!tos_screen||x<0||y<0)return;
    if(tos_mode==2u){
        uint32_t o;uint8_t m;
        if(x>=640||y>=400)return;
        o=(uint32_t)y*80u+(uint32_t)(x>>3);m=(uint8_t)(0x80u>>(x&7));
        if(color)tos_screen[o]|=m;else tos_screen[o]&=(uint8_t)~m;
    } else if(tos_mode==1u){
        uint32_t o;uint8_t m;
        if(x>=640||y>=200)return;
        o=(uint32_t)y*160u+(uint32_t)(x>>3);m=(uint8_t)(0x80u>>(x&7));
        if(color&1u)tos_screen[o]|=m;else tos_screen[o]&=(uint8_t)~m;
        if(color&2u)tos_screen[o+32000u]|=m;else tos_screen[o+32000u]&=(uint8_t)~m;
    } else {
        uint32_t o;uint8_t m;int p;
        if(x>=320||y>=200)return;
        o=(uint32_t)y*160u+(uint32_t)(x>>3);m=(uint8_t)(0x80u>>(x&7));
        for(p=0;p<4;p++){uint32_t po=o+(uint32_t)p*32000u;if(color&(1u<<p))tos_screen[po]|=m;else tos_screen[po]&=(uint8_t)~m;}
    }
}
static uint8_t tos_getpixel(int x,int y)
{
    if(!tos_screen||x<0||y<0)return 0u;
    if(tos_mode==2u){if(x>=640||y>=400)return 0u;return (uint8_t)((tos_screen[(uint32_t)y*80u+(uint32_t)(x>>3)]>>(7-(x&7)))&1u);}
    if(tos_mode==1u){if(x>=640||y>=200)return 0u;{uint32_t o=(uint32_t)y*160u+(uint32_t)(x>>3);
        return (uint8_t)(((tos_screen[o]>>(7-(x&7)))&1u)|(((tos_screen[o+32000u]>>(7-(x&7)))&1u)<<1));}}
    if(x>=320||y>=200)return 0u;
    {uint32_t o=(uint32_t)y*160u+(uint32_t)(x>>3);uint8_t r=0u;int p;
     for(p=0;p<4;p++)r=(uint8_t)(r|((((tos_screen[o+(uint32_t)p*32000u]>>(7-(x&7)))&1u))<<p));
     return r;}
}
static void tos_line(int x0,int y0,int x1,int y1,uint8_t c)
{
    int dx=(x1>x0)?(x1-x0):(x0-x1),sx=(x0<x1)?1:-1;
    int dy=(y1>y0)?(y1-y0):(y0-y1),sy=(y0<y1)?1:-1;
    int err=dx-dy;
    for(;;){tos_putpixel(x0,y0,c);if(x0==x1&&y0==y1)break;
        {int e2=2*err;if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}}
}
static void tos_rect(int x0,int y0,int x1,int y1,uint8_t c)
{
    int x,y;
    if(x1<x0){int t=x0;x0=x1;x1=t;}
    if(y1<y0){int t=y0;y0=y1;y1=t;}
    for(y=y0;y<=y1;y++)for(x=x0;x<=x1;x++)tos_putpixel(x,y,c);
}

static void tos_polygon(int n,const uint16_t *pts,uint8_t c)
{
    int i,j,ymin,ymax;
    if(n<3)return;
    ymin=ymax=(int16_t)pts[1];
    for(i=1;i<n;i++){int y=(int16_t)pts[i*2+1];if(y<ymin)ymin=y;if(y>ymax)ymax=y;}
    for(j=ymin;j<=ymax;j++){
        int xints[32],nx=0;
        for(i=0;i<n&&nx<31;i++){
            int x1=(int16_t)pts[i*2],y1=(int16_t)pts[i*2+1];
            int k=(i+1)%n,x2=(int16_t)pts[k*2],y2=(int16_t)pts[k*2+1];
            if((y1<=j&&y2>j)||(y2<=j&&y1>j)){
                int x=x1+(int)(((long)(j-y1)*(x2-x1))/(y2-y1));
                xints[nx++]=x;
            }
        }
        {int a,b;for(a=0;a<nx;a++)for(b=a+1;b<nx;b++)if(xints[b]<xints[a]){int t=xints[a];xints[a]=xints[b];xints[b]=t;}}
        for(i=0;i+1<nx;i+=2){int x;for(x=xints[i];x<=xints[i+1];x++)tos_putpixel(x,j,c);}
    }
}
static void tos_floodfill(int x,int y,uint8_t c)
{
    static int stack_x[256],stack_y[256];
    int sp=0,limitx=639,limity=399;
    uint8_t target;
    if(!tos_screen)return;
    if(tos_mode==1u){limitx=639;limity=199;}
    else if(tos_mode==0u){limitx=319;limity=199;}
    if(x<0||y<0||x>limitx||y>limity)return;
    target=tos_getpixel(x,y);
    if(target==c)return;
    stack_x[0]=x;stack_y[0]=y;sp=1;
    while(sp>0){
        int px,py,lx,rx,nx;
        --sp;px=stack_x[sp];py=stack_y[sp];
        if(tos_getpixel(px,py)!=target)continue;
        lx=px;while(lx>0&&tos_getpixel(lx-1,py)==target)lx--;
        rx=px;while(rx<limitx&&tos_getpixel(rx+1,py)==target)rx++;
        for(nx=lx;nx<=rx;nx++)tos_putpixel(nx,py,c);
        for(nx=lx;nx<=rx;nx++){
            if(py>0&&tos_getpixel(nx,py-1)==target&&sp<254){stack_x[sp]=nx;stack_y[sp]=py-1;sp++;}
            if(py<limity&&tos_getpixel(nx,py+1)==target&&sp<254){stack_x[sp]=nx;stack_y[sp]=py+1;sp++;}
        }
    }
}
static void tos_raster_copy(void *form,int to_form)
{
    if(!tos_screen||!form)return;
    if(to_form){uint32_t i;uint8_t *d=(uint8_t *)form;for(i=0u;i<32000u;i++)d[i]=tos_screen[i];}
    else{uint32_t i;const uint8_t *s=(const uint8_t *)form;for(i=0u;i<32000u;i++)tos_screen[i]=s[i];}
}

/* --- TOS Line-A (0xA000-0xA00F) ------------------------------------------- */
int32_t asf_tos_linea_op(uint32_t op,uint32_t *block)
{
    const uint8_t *b=(const uint8_t *)block;
    int x1,y1,x2,y2;uint8_t color;
    if(!b)return 0;
    x1=(int16_t)rd16(b+38u);y1=(int16_t)rd16(b+40u);
    x2=(int16_t)rd16(b+42u);y2=(int16_t)rd16(b+44u);
    color=(uint8_t)((rd16(b+24u)?1u:0u)|(rd16(b+26u)?2u:0u)|(rd16(b+28u)?4u:0u)|(rd16(b+30u)?8u:0u));
    switch(op){
    case 1u: tos_putpixel(x1,y1,color?color:1u);return 0;      /* put pixel */
    case 2u: return (int32_t)tos_getpixel(x1,y1);              /* get pixel */
    case 3u: tos_line(x1,y1,x2,y2,color?color:1u);return 0;    /* line */
    case 4u: {int x;if(x2<x1){int t=x1;x1=x2;x2=t;}
              for(x=x1;x<=x2;x++)tos_putpixel(x,y1,color?color:1u);
              return 0;}  /* hline */
    case 5u: tos_rect(x1,y1,x2,y2,color?color:1u);return 0;    /* filled rect */
    case 6u: {uint32_t pp=rd32(b+8u);int n=(int)(int16_t)rd16(b+6u);
              if(pp)tos_polygon(n,(const uint16_t *)(uintptr_t)pp,color?color:1u);
              return 0;}                                          /* filled polygon */
    case 0x0eu: {uint32_t fp=rd32(b+8u);if(fp)tos_raster_copy((void *)(uintptr_t)fp,1);return 0;} /* raster -> form */
    case 0x0fu: tos_floodfill(x1,y1,color?color:1u);return 0;  /* flood fill */
    default: return 0;
    }
}

static void tos_diskpd(void);

/* --- GEMDOS file handles, heap and directory cache ------------------------ */
#define TOS_MAX_HANDLES 8
#define TOS_HEAP_SIZE   65536u
#define TOS_DIR_MAX     16

typedef struct {uint8_t used;uint32_t size,pos;char path[96];} tos_file_t;
typedef struct {char name[13];uint32_t size;uint8_t attr;} tos_dirent_t;

static tos_file_t tos_files[TOS_MAX_HANDLES];
static uint8_t *tos_heap;
static uint32_t tos_heap_used;
static uint8_t tos_dta_default[64];
static uint8_t *tos_dta;
static tos_dirent_t tos_dir[TOS_DIR_MAX];
static int tos_dir_count,tos_dir_index;

/* Saved parent state around a nested Pexec run. */
typedef struct {
    uint8_t *screen;
    uint8_t mode;
    uint16_t palette[16];
    uint8_t *heap;
    uint32_t heap_used;
    tos_file_t files[TOS_MAX_HANDLES];
    uint8_t *dta;
    int32_t key_pending;
    /* G1: everything a TOS process needs to resume on another time slice. */
    uint8_t *basepage;
    int32_t exit_code;
    uint16_t date,time;
    int dir_count,dir_index;
    uint8_t shift,caps;
} tos_state_t;
static tos_state_t tos_states[4];
static int tos_state_depth;

/* G1.2: per-process park slots.  The scheduler calls tos_proc_switch() with
 * the outgoing and incoming process ids; the globals above always describe
 * the process that is currently running. */
#define TOS_MAX_PROCS 4
static tos_state_t tos_proc[TOS_MAX_PROCS];
static uint8_t tos_proc_used[TOS_MAX_PROCS];
static uint8_t *tos_basepage;

static void tos_state_save(tos_state_t *s);
static void tos_state_restore(const tos_state_t *s);

/* G1.3a: a runnable TOS task.  The loader fills basepage/entry/state; once
 * the scheduler enters the task it owns the saved SP (the task's stack at the
 * moment the Timer C frame was pushed) for preemptive switches. */
typedef struct {
    uint8_t     used;
    uint32_t    sp;
    /* The state lives in kmem, not in the table: a full tos_state_t per slot
     * grew the kernel BSS by ~850 bytes and broke nested Pexec. */
    tos_state_t *state;
    uint8_t    *basepage;
    uint8_t    *entry;
    uint8_t    *mem;         /* loaded image, freed on task exit */
    uint8_t    *stack;       /* task stack top */
    uint32_t    total;
    uint32_t    pid;
} tos_task_t;
static tos_task_t tos_task[TOS_MAX_PROCS];

static uint32_t tos_task_prepare_entry(uint8_t *stack_top,const void *entry,
                                       const uint8_t *basepage);
static int tos_task_alloc(void)
{
    int i;
    for(i=0;i<TOS_MAX_PROCS;i++)if(!tos_task[i].used)return i;
    return -1;
}
static void tos_task_free(int slot)
{
    if(slot>=0&&slot<TOS_MAX_PROCS)tos_task[slot].used=0;
}
static int tos_task_count(void)
{
    int i,n=0;
    for(i=0;i<TOS_MAX_PROCS;i++)if(tos_task[i].used)++n;
    return n;
}

#define TOS_KERNEL_SLOT TOS_MAX_PROCS    /* the shell/kernel context slot */
static uint8_t tos_task_current=TOS_KERNEL_SLOT;  /* running context */

/* G1.3b-2b-i: build the first-entry frame for a task.
 *
 * The layout is exactly what the kernel's own interrupt path uses
 * (kernel_entry.S: movem.l d0-d7/a0-a6 = 60 bytes, then SR at +60 and PC at
 * +62), so the Timer C epilogue (movem.l (sp)+,d0-d7/a0-a6 / rte) resumes the
 * task at its entry with A0 = basepage.  All registers are zero except A0.
 * Must only be called with an even stack_top. */
static uint32_t tos_task_prepare_entry(uint8_t *stack_top,const void *entry,
                                       const uint8_t *basepage)
    __attribute__((unused));   /* wired up by the next half-step (2b-ii) */
static uint32_t tos_task_prepare_entry(uint8_t *stack_top,const void *entry,
                                       const uint8_t *basepage)
{
    uint8_t *sp=stack_top-66u;
    uint32_t i;
    for(i=0u;i<60u;i++)sp[i]=0u;
    sp[32]=(uint8_t)((uintptr_t)basepage>>24);
    sp[33]=(uint8_t)((uintptr_t)basepage>>16);
    sp[34]=(uint8_t)((uintptr_t)basepage>>8);
    sp[35]=(uint8_t)(uintptr_t)basepage;              /* A0 */
    sp[60]=0x20u;sp[61]=0x00u;                        /* SR: supervisor, IRQs on */
    sp[62]=(uint8_t)((uintptr_t)entry>>24);
    sp[63]=(uint8_t)((uintptr_t)entry>>16);
    sp[64]=(uint8_t)((uintptr_t)entry>>8);
    sp[65]=(uint8_t)(uintptr_t)entry;                 /* PC */
    return (uint32_t)(uintptr_t)sp;
}

/* G1.3b: preemptive switch, called from the Timer C handler with the SP of the
 * interrupted context (its saved registers plus the exception frame).  Returns
 * 0 to keep the current task, or the SP of the next one.  Dormant while a
 * single task exists, so enabling the scheduler is a separate, testable step. */
/* SP of each context: the shell (index TOS_MAX_PROCS) plus the TOS slots.
 * Kept as a tiny separate array - storing a full tos_state_t in every slot
 * grew the kernel BSS enough to break nested Pexec. */
static uint32_t tos_task_sp[TOS_MAX_PROCS+1];

uint32_t tos_task_tick(uint32_t sp)
{
    int i,count=0,next=-1;
    for(i=0;i<TOS_MAX_PROCS;i++)if(tos_task[i].used)++count;
    if(count<2)return 0u;                      /* dormant until two tasks */
    tos_task_sp[tos_task_current]=sp;
    for(i=1;i<=TOS_MAX_PROCS+1;i++){
        int t=(tos_task_current+i)%(TOS_MAX_PROCS+1);
        if(t>=TOS_MAX_PROCS){next=t;break;}    /* the shell is always ready */
        if(tos_task[t].used){next=t;break;}
    }
    if(next<0)return 0u;
    if(tos_task_current<TOS_MAX_PROCS&&tos_task[tos_task_current].state)
        tos_state_save(tos_task[tos_task_current].state);
    tos_task_current=(uint8_t)next;
    if(next<TOS_MAX_PROCS&&tos_task[next].state)
        tos_state_restore(tos_task[next].state);
    return tos_task_sp[next];
}

void tos_proc_alloc(uint32_t pid)
{if(pid<TOS_MAX_PROCS)tos_proc_used[pid]=1u;}
void tos_proc_free(uint32_t pid)
{if(pid<TOS_MAX_PROCS)tos_proc_used[pid]=0u;}
void tos_proc_switch(uint32_t from,uint32_t to)
{
    if(from<TOS_MAX_PROCS&&tos_proc_used[from])tos_state_save(&tos_proc[from]);
    if(to<TOS_MAX_PROCS&&tos_proc_used[to])tos_state_restore(&tos_proc[to]);
}

static void tos_state_save(tos_state_t *s)
{
    int i;
    s->screen=tos_screen;s->mode=tos_mode;s->heap=tos_heap;s->heap_used=tos_heap_used;
    s->dta=tos_dta;s->key_pending=tos_key_pending;
    s->basepage=tos_basepage;s->exit_code=tos_exit_code;
    s->date=tos_date;s->time=tos_time;
    s->dir_count=tos_dir_count;s->dir_index=tos_dir_index;
    s->shift=tos_shift;s->caps=tos_caps;
    for(i=0;i<16;i++)s->palette[i]=tos_palette[i];
    for(i=0;i<TOS_MAX_HANDLES;i++){
        int j;
        s->files[i].used=tos_files[i].used;
        s->files[i].size=tos_files[i].size;
        s->files[i].pos=tos_files[i].pos;
        for(j=0;j<96;j++)s->files[i].path[j]=tos_files[i].path[j];
    }
}
static void tos_state_restore(const tos_state_t *s)
{
    int i;
    tos_screen=s->screen;tos_mode=s->mode;tos_heap=s->heap;tos_heap_used=s->heap_used;
    tos_dta=s->dta;tos_key_pending=s->key_pending;
    tos_basepage=s->basepage;tos_exit_code=s->exit_code;
    tos_date=s->date;tos_time=s->time;
    tos_dir_count=s->dir_count;tos_dir_index=s->dir_index;
    tos_shift=s->shift;tos_caps=s->caps;
    for(i=0;i<16;i++)tos_palette[i]=s->palette[i];
    for(i=0;i<TOS_MAX_HANDLES;i++){
        int j;
        tos_files[i].used=s->files[i].used;
        tos_files[i].size=s->files[i].size;
        tos_files[i].pos=s->files[i].pos;
        for(j=0;j<96;j++)tos_files[i].path[j]=s->files[i].path[j];
    }
}

static int tos_pat_match(const char *pat,const char *name)
{
    while(*pat){
        char pc=*pat;
        if(pc=='*'){
            ++pat;
            if(!*pat)return 1;
            while(*name){if(tos_pat_match(pat,name))return 1;++name;}
            return 0;
        }
        {char nc=*name;
         if(pc>='a'&&pc<='z')pc=(char)(pc-32);
         if(nc>='a'&&nc<='z')nc=(char)(nc-32);
         if(pc!='?'&&pc!=nc)return 0;}
        ++pat;
        if(!*name)return 0;
        ++name;
    }
    return *name==0;
}

static int tos_dir_collect(const char *name,uint32_t size,uint32_t attributes,void *ctx)
{
    (void)ctx;
    if(tos_dir_count>=TOS_DIR_MAX)return 0;
    if(!tos_pat_match((const char *)ctx,name))return 0;
    {int i;for(i=0;i<12&&name[i];i++)tos_dir[tos_dir_count].name[i]=name[i];
     tos_dir[tos_dir_count].name[i]=0;}
    tos_dir[tos_dir_count].size=size;
    tos_dir[tos_dir_count].attr=(uint8_t)attributes;
    tos_dir_count++;
    return 0;
}

static void tos_dta_write(const tos_dirent_t *e)
{
    uint8_t *d=tos_dta?tos_dta:tos_dta_default;
    int i;
    for(i=0;i<21;i++)d[i]=0;
    d[21]=e->attr;d[22]=0;d[23]=0;d[24]=0;d[25]=0;
    wr32(d+26,e->size);
    for(i=0;i<12&&e->name[i];i++)d[30+i]=(uint8_t)e->name[i];
    d[30+i]=0;
}

static int32_t tos_fopen(const char *name,uint16_t mode)
{
    int i;uint32_t size=0;uint8_t attr=0;
    (void)mode;
    if(!name||!*name)return -33;
    for(i=0;i<TOS_MAX_HANDLES;i++)if(!tos_files[i].used)break;
    if(i>=TOS_MAX_HANDLES)return -35;
    if(kfat_stat_path(name,&size,&attr)!=FAT_FILE_FOUND)return -33;
    if(attr&0x10u)return -33;
    tos_files[i].used=1;tos_files[i].size=size;tos_files[i].pos=0;
    {uint32_t k=0;while(name[k]&&k<95u){tos_files[i].path[k]=name[k];++k;}tos_files[i].path[k]=0;}
    return (int32_t)i;
}
static int32_t tos_fread(uint16_t handle,uint32_t count,void *buf)
{
    uint32_t got=0u;
    if(handle>=TOS_MAX_HANDLES||!tos_files[handle].used)return -37;
    if(count>tos_files[handle].size-tos_files[handle].pos)count=tos_files[handle].size-tos_files[handle].pos;
    if(count==0u)return 0;
    if(kfat_read_path_at(tos_files[handle].path,tos_files[handle].pos,buf,count,&got)!=FAT_FILE_FOUND)return -37;
    tos_files[handle].pos+=got;
    return (int32_t)got;
}
static int32_t tos_fwrite(uint16_t handle,uint32_t count,const void *buf)
{
    uint32_t written=0u;
    if(handle>=TOS_MAX_HANDLES||!tos_files[handle].used)return -37;
    if(count==0u)return 0;
    if(kfat_write_path_at(tos_files[handle].path,tos_files[handle].pos,buf,count,&written)!=FAT_FILE_FOUND)return -37;
    tos_files[handle].pos+=written;
    if(tos_files[handle].pos>tos_files[handle].size)tos_files[handle].size=tos_files[handle].pos;
    return (int32_t)written;
}
static int32_t tos_fclose(uint16_t handle)
{
    if(handle>=TOS_MAX_HANDLES||!tos_files[handle].used)return -37;
    tos_files[handle].used=0;
    return 0;
}
static int32_t tos_fseek(int32_t off,uint16_t handle,uint16_t mode)
{
    int32_t np;
    if(handle>=TOS_MAX_HANDLES||!tos_files[handle].used)return -37;
    if(mode==0u)np=off;else if(mode==1u)np=(int32_t)tos_files[handle].pos+off;else np=(int32_t)tos_files[handle].size+off;
    if(np<0)np=0;
    if((uint32_t)np>tos_files[handle].size)np=(int32_t)tos_files[handle].size;
    tos_files[handle].pos=(uint32_t)np;
    return np;
}
static int tos_fsfirst(const char *pattern)
{
    char dir[96],pat[16];
    const char *slash=0,*p;
    int k=0;
    for(p=pattern;*p;++p)if(*p=='\\'||*p==':')slash=p;
    if(slash){uint32_t n=(uint32_t)(slash-pattern)+1u,kk;if(n>95u)n=95u;
        for(kk=0u;kk<n;++kk)dir[kk]=pattern[kk];
        dir[kk]=0;}
    else{dir[0]='.';dir[1]=0;}
    {const char *q=slash?slash+1:pattern;k=0;while(*q&&k<15)pat[k++]=*q++;pat[k]=0;}
    tos_dir_count=0;tos_dir_index=0;
    {char *ctx=pat;(void)ctx;}
    kfat_list_path(dir,(kfat_list_callback_t)tos_dir_collect,pat);
    if(tos_dir_count==0)return -33;
    tos_dta_write(&tos_dir[0]);
    tos_dir_index=1;
    return 0;
}
static int tos_fsnext(void)
{
    if(tos_dir_index>=tos_dir_count)return -33;
    tos_dta_write(&tos_dir[tos_dir_index]);
    tos_dir_index++;
    return 0;
}
static void *tos_malloc(uint32_t bytes)
{
    void *p;
    if(!tos_heap)return 0;
    bytes=(bytes+1u)&~1u;
    if(tos_heap_used+bytes>TOS_HEAP_SIZE)return 0;
    p=tos_heap+tos_heap_used;
    tos_heap_used+=bytes;
    return p;
}
static int tos_mfree(void *p){(void)p;return 0;}

/* --- TOS GEMDOS (trap #1) -------------------------------------------------- */
int32_t asf_tos_gemdos(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    uint16_t *a=tos_args(frame,usp);
    fn&=0xffffu;                       /* only the low word is the function */
    switch(fn){
    case 0x00u: tos_exit_code=0;asf_tos_exit();return 0;                 /* Pterm0 */
    case 0x01u: case 0x08u: return tos_getkey();                        /* Cconin/Cnecin */
    case 0x02u: tos_putc((char)(uintptr_t)a[0]);return 0;                /* Cconout */
    case 0x06u: case 0x07u: return tos_getkey();                        /* Crawio/Crawcin */
    case 0x09u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];if(p)tos_print((const char *)(uintptr_t)p);return 0;} /* Cconws */
    case 0x0bu: return tos_key_ready();                                  /* Cconis */
    case 0x0eu: return 0;                                                /* Dsetdrv */
    case 0x19u: return 0;                                                /* Dgetdrv */
    case 0x2au: return (int32_t)tos_date;                                /* Tgetdate */
    case 0x2bu: tos_date=rd16((const uint8_t *)a);return 0;              /* Tsetdate */
    case 0x2cu: return (int32_t)tos_time;                                /* Tgettime */
    case 0x2du: tos_time=rd16((const uint8_t *)a);return 0;              /* Tsettime */
    case 0x39u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];                   /* Dcreate */
        if(!p)return -33;
        return kfat_create_dir((const char *)(uintptr_t)p)==FAT_FILE_FOUND?0:-33;}
    case 0x3au: {uint32_t p=((uint32_t)a[0]<<16)|a[1];                   /* Ddelete */
        if(!p)return -33;
        return kfat_delete_path((const char *)(uintptr_t)p)==FAT_FILE_FOUND?0:-33;} 
    case 0x1au: {uint32_t p=((uint32_t)a[0]<<16)|a[1];if(p)tos_dta=(uint8_t *)(uintptr_t)p;return 0;} /* Fsetdta */
    case 0x2fu: return (int32_t)(uintptr_t)(tos_dta?tos_dta:tos_dta_default);  /* Fgetdta */
    case 0x3cu: {uint32_t p=((uint32_t)a[0]<<16)|a[1];                    /* Fcreate */
        if(!p)return -33;
        if(kfat_create_path((const char *)(uintptr_t)p)!=FAT_FILE_FOUND)return -33;
        return tos_fopen((const char *)(uintptr_t)p,0u);}
    case 0x3du: {uint32_t p=((uint32_t)a[0]<<16)|a[1];return tos_fopen((const char *)(uintptr_t)p,rd16((const uint8_t *)a+4u));} /* Fopen */
    case 0x3eu: return tos_fclose(rd16((const uint8_t *)a));             /* Fclose */
    case 0x3fu: return tos_fread(rd16((const uint8_t *)a),rd32((const uint8_t *)a+2u),(void *)(uintptr_t)rd32((const uint8_t *)a+6u)); /* Fread */
    case 0x40u: return tos_fwrite(rd16((const uint8_t *)a),rd32((const uint8_t *)a+2u),(const void *)(uintptr_t)rd32((const uint8_t *)a+6u)); /* Fwrite */
    case 0x41u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];                    /* Fdelete */
        if(!p)return -33;
        return kfat_delete_path((const char *)(uintptr_t)p)==FAT_FILE_FOUND?0:-33;}
    case 0x42u: return tos_fseek((int32_t)rd32((const uint8_t *)a),rd16((const uint8_t *)a+4u),rd16((const uint8_t *)a+6u)); /* Fseek */
    case 0x43u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];                   /* Fattrib */
        uint8_t old=0;
        if(!p)return -33;
        if(kfat_attrib_path((const char *)(uintptr_t)p,rd16((const uint8_t *)a+4u),
                            (uint8_t)rd16((const uint8_t *)a+6u),&old)!=FAT_FILE_FOUND)return -33;
        return (int32_t)old;}
    case 0x48u: return (int32_t)(uintptr_t)tos_malloc(rd32((const uint8_t *)a)); /* Malloc */
    case 0x49u: return tos_mfree((void *)(uintptr_t)rd32((const uint8_t *)a));   /* Mfree */
    case 0x4au: return 0;                                                /* Mshrink */
    case 0x4bu: {                                                        /* Pexec */
        uint16_t mode=rd16((const uint8_t *)a);
        uint32_t prog=rd32((const uint8_t *)a+2u);
        int32_t r;
        if(mode!=3u||!prog)return -1;
        if(tos_state_depth>=4)return -1;
        tos_state_save(&tos_states[tos_state_depth]);
        tos_state_depth++;
        r=kernel_tos_run_program((const char *)(uintptr_t)prog);
        tos_state_depth--;
        tos_state_restore(&tos_states[tos_state_depth]);
        return r;
    }
    case 0x4cu: tos_exit_code=(int32_t)rd16((const uint8_t *)a);asf_tos_exit();return 0; /* Pterm */
    case 0x4du: {uint32_t p=((uint32_t)a[0]<<16)|a[1];return tos_fsfirst((const char *)(uintptr_t)p);} /* Fsfirst */
    case 0x4eu: return tos_fsnext();                                     /* Fsnext */
    case 0x4fu: {uint32_t f=rd32((const uint8_t *)a+2u);                 /* Frename */
        uint32_t t=rd32((const uint8_t *)a+6u);
        if(!f||!t)return -33;
        return kfat_rename_path((const char *)(uintptr_t)f,(const char *)(uintptr_t)t)==FAT_FILE_FOUND?0:-33;}
    default: return 0;
    }
}

/* --- TOS BIOS (trap #13) --------------------------------------------------- */
int32_t asf_tos_bios(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    uint16_t *a=tos_args(frame,usp);
    fn&=0xffffu;
    if(fn==1u)return tos_key_ready();         /* Bconstat */
    if(fn==2u)return tos_getkey();            /* Bconin */
    if(fn==3u){tos_putc((char)(uintptr_t)a[1]);return 0;}  /* Bconout(dev,c) */
    if(fn==8u)return 0;                       /* Bcostat */
    if(fn==11u)return 0;                      /* Kbshift */
    return 0;
}

/* --- TOS XBIOS (trap #14) -------------------------------------------------- */
int32_t asf_tos_xbios(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    uint16_t *a=tos_args(frame,usp);
    fn&=0xffffu;
    switch(fn){
    case 2u: case 3u: return (int32_t)(uintptr_t)tos_screen;              /* Physbase/Logbase */
    case 4u: return (int32_t)tos_mode;                                    /* Getrez */
    case 5u: {uint32_t log=((uint32_t)a[0]<<16)|a[1];                     /* Setscreen */
              if(log)tos_screen=(uint8_t *)(uintptr_t)log;
              tos_mode=(uint8_t)(((uint32_t)a[4]<<16)|a[5]);
              if(tos_mode>2u)tos_mode=2u;
              return 0;}
    case 6u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];int k;                 /* Setpalette */
              if(p){const uint16_t *s=(const uint16_t *)(uintptr_t)p;for(k=0;k<16;k++)tos_palette[k]=s[k];}
              return 0;}
    case 7u: {uint16_t idx=a[0];uint16_t col=a[1];if(idx<16u)tos_palette[idx]=col;return 0;} /* Setcolor */
    case 37u: return 0;                                                   /* Vsync */
    case 0x00feu: {                                                       /* ASF control */
        uint16_t cmd=a[0];
        switch(cmd){
        case 0u: kblock_cache_enable(0u);tos_print("SMARTDRV: CACHE OFF\n");return 0;
        case 1u: kblock_cache_enable(1u);tos_print("SMARTDRV: CACHE ON, ");
                 tos_dec(kblock_cache_slots());tos_print(" sectors\n");return 0;
        case 2u: return (int32_t)kernel_timer_ticks;
        case 3u: return (int32_t)kblock_cache_hits();
        case 4u: return (int32_t)kblock_cache_misses();
        case 5u: tos_diskpd();return 0;
        case 6u: kblock_cache_release();tos_print("SMARTDRV: CACHE OFF + CLEARED\n");return 0;
        case 7u: tos_window_enabled=1u;tos_print("T2: WINDOW MODE ON\n");return 0;
        case 10u: {                                           /* raw IKBD ring dump */
            int32_t i;
            const char *hx="0123456789ABCDEF";
            tos_print("IKBD TOTAL: ");tos_dec((uint32_t)kernel_ikbd_raw_total());
            tos_print("  DROPPED: ");tos_dec((uint32_t)kernel_ikbd_drop_count());tos_print("\n");
            tos_print("RAW (data:status), newest last:\n");
            for(i=23;i>=0;i--){
                int32_t v=kernel_ikbd_raw_last((uint32_t)i);
                uint8_t d,st;
                if(v<0)continue;
                d=(uint8_t)v;st=(uint8_t)((uint32_t)v>>8);
                tos_putc(hx[(d>>4)&15u]);tos_putc(hx[d&15u]);tos_putc(':');
                tos_putc(hx[(st>>4)&15u]);tos_putc(hx[st&15u]);tos_putc(' ');
            }
            tos_print("\n");
            return 0;
        }
        case 9u: {                                                       /* log hex dump */
            int32_t n=kernel_console_log_count(),k;
            tos_print("LOG LINES: ");tos_dec((uint32_t)n);tos_print("\n");
            for(k=(n>3?n-3:0);k<n;k++){
                const char *s=kernel_console_log_line(k);
                int j;
                tos_print("L");tos_dec((uint32_t)k);tos_print(": ");
                for(j=0;s[j]&&j<40;j++){
                    const char *hx="0123456789ABCDEF";
                    tos_putc(hx[((uint8_t)s[j]>>4)&15u]);
                    tos_putc(hx[(uint8_t)s[j]&15u]);
                    tos_putc(' ');
                }
                tos_print("\n");
            }
            return 0;
        }
        case 8u: tos_window_enabled=0u;
                 if(tos_desktop_active)tos_desktop_leave();   /* back to text */
                 tos_print("T2: WINDOW MODE OFF\n");return 0;
        default: return 0;
        }
    }
    default: return 0;
    }
}

static void tos_diskpd(void);

/* --- DISKPD: short disk benchmark (write/read/seek) ----------------------- */
static void tos_diskpd(void)
{
    static const char path[]="\\DISKPD.TMP";
    uint32_t chunk=8192u,total=64u*1024u,i,t0,t1,wt,rt,st,kbs,guard;
    uint8_t *buf=0;
    if(kfat_create_path(path)!=FAT_FILE_FOUND){tos_print("DISKPD: cannot create temp file\n");return;}
    buf=(uint8_t *)kmem_alloc(chunk,512u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!buf){tos_print("DISKPD: no memory\n");return;}
    for(i=0u;i<chunk;i++)buf[i]=(uint8_t)(i*7u);
    tos_print("DISKPD 1.0 - 64 KB write/read + 12 random reads\n");
    tos_print("  cache: ");tos_print(kblock_cache_enabled()?"ON":"OFF");tos_print("\n");
    t0=(uint32_t)kernel_timer_ticks;
    guard=0u;{uint32_t off=0u,w=0u;while(off<total&&guard++<200u){if(kfat_write_path_at(path,off,buf,chunk,&w)!=FAT_FILE_FOUND)break;if(!w)break;off+=w;}}
    t1=(uint32_t)kernel_timer_ticks;wt=t1-t0;
    t0=(uint32_t)kernel_timer_ticks;
    guard=0u;{uint32_t off=0u,g=0u;while(off<total&&guard++<200u){if(kfat_read_path_at(path,off,buf,chunk,&g)!=FAT_FILE_FOUND)break;if(!g)break;off+=g;}}
    t1=(uint32_t)kernel_timer_ticks;rt=t1-t0;
    t0=(uint32_t)kernel_timer_ticks;
    {uint32_t seed=12345u,k;for(k=0u;k<12u;k++){uint32_t o,g;
        seed=seed*1103515245u+12345u;o=((seed>>9)%(total/512u))*512u;
        if(kfat_read_path_at(path,o,buf,512u,&g)!=FAT_FILE_FOUND)break;}}
    t1=(uint32_t)kernel_timer_ticks;st=t1-t0;
    tos_print("  write: ");tos_dec(wt);tos_print(" ticks (");tos_dec(wt*20u);tos_print(" ms) ");
    kbs=wt?((total/1024u)*50u/wt):0u;tos_dec(kbs);tos_print(" KB/s\n");
    tos_print("  read : ");tos_dec(rt);tos_print(" ticks (");tos_dec(rt*20u);tos_print(" ms) ");
    kbs=rt?((total/1024u)*50u/rt):0u;tos_dec(kbs);tos_print(" KB/s\n");
    tos_print("  seek : ");tos_dec(st);tos_print(" ticks/12 reads, avg ");
    tos_dec(st*20u/12u);tos_print(" ms\n");
    kfat_delete_path(path);
    kmem_free(buf,KMEM_OWNER_KERNEL);
}

/* --- AES/VDI (trap #2): stub for now --------------------------------------- */
int32_t asf_tos_aesvdi(uint32_t fn,uint32_t *frame){(void)frame;(void)fn;return 0;}

/* --- T2: composite the private TOS screen into a window ------------------- */
static void (*vid_enter)(void);
static void (*vid_leave)(void);
static void (*vid_clear)(uint8_t);
static void (*vid_fill)(int,int,int,int,uint8_t);
static void (*vid_text)(int,int,const char *,uint8_t,uint8_t);
static void (*vid_blit_mono)(int,int,int,int,const uint8_t *,int,uint8_t,uint8_t);
static void (*console_clear_hook)(void);
static volatile uint8_t tos_window_active;
#define TOS_WIN_X 160
#define TOS_WIN_Y 90
/* G2: window position, cascaded per run so successive windows do not land on
   top of each other. */
static int32_t tos_win_x=TOS_WIN_X;
static int32_t tos_win_y=TOS_WIN_Y;
static uint8_t tos_window_seq;
/* NOTE: a 1:1 (640x400) client was tried and froze the machine, so the
 * client is the scaled 320x200 again.  The readability problem is real and
 * belongs to the window manager step, not to a plain geometry swap. */
#define TOS_WIN_W 320
#define TOS_WIN_H 200
#define TOS_TITLE 18
#define TOS_STATUS_Y 424

void kernel_tos_set_console_clear(void (*clear)(void)){console_clear_hook=clear;}

void kernel_tos_set_video(void (*enter)(void),void (*leave)(void),void (*clear)(uint8_t),
                          void (*fill)(int,int,int,int,uint8_t),
                          void (*text)(int,int,const char *,uint8_t,uint8_t),
                          void (*blit_mono)(int,int,int,int,const uint8_t *,int,uint8_t,uint8_t))
{
    vid_enter=enter;vid_leave=leave;vid_clear=clear;
    vid_fill=fill;vid_text=text;vid_blit_mono=blit_mono;
}

/* 640x400 -> 320x200 (2:1) subsample of the private screen. */
static uint8_t tos_scaled[320u*200u/8u];
/* Pack the even-numbered pixels of one source byte into a nibble, so the 2:1
 * downscale becomes 40 lookups + 40 stores per row instead of 320 bit tests
 * with a read-modify-write per pixel (that was the slowest loop in T2). */
static uint8_t tos_scale_lut[256];
static uint8_t tos_scale_lut_ready;

static void tos_build_scaled_range(int y0,int y1);

static void tos_build_scaled(void)
{
    int i;
    if(!tos_scale_lut_ready){
        for(i=0;i<256;i++){
            uint8_t v=0u;
            if(i&0x80u)v|=(uint8_t)0x08u;   /* source px0 -> dest bit 3 */
            if(i&0x20u)v|=(uint8_t)0x04u;   /* px2 -> bit 2 */
            if(i&0x08u)v|=(uint8_t)0x02u;   /* px4 -> bit 1 */
            if(i&0x02u)v|=(uint8_t)0x01u;   /* px6 -> bit 0 */
            tos_scale_lut[i]=v;
        }
        tos_scale_lut_ready=1u;
    }
    tos_build_scaled_range(0,200);
}

static void tos_build_scaled_range(int y0,int y1)
{
    int y,i;
    for(y=y0;y<y1;y++){
        const uint8_t *srow=tos_screen+(uint32_t)(y*2)*80u;
        uint8_t *drow=tos_scaled+(uint32_t)y*40u;
        for(i=0;i<40;i++)
            drow[i]=(uint8_t)((tos_scale_lut[srow[i*2]]<<4)|tos_scale_lut[srow[i*2+1]]);
    }
}

/* Cheap rolling checksum over the 32 KB private screen. */
/* EmuTOS redraws only the rectangles in each window's w_rlist, never the whole
 * screen.  We approximate that with band signatures: only the 20-row bands
 * whose contents changed are re-scaled and re-blitted.  Ten bands keep the BSS
 * footprint tiny (a big per-row table previously broke nested Pexec). */
#define TOS_BANDS     10
#define TOS_BAND_ROWS (200/TOS_BANDS)
static uint32_t tos_band_prev[TOS_BANDS];
static uint8_t tos_band_valid;
static uint8_t tos_composite_divider;

/* Signature of one destination row taken from the private 640x400 screen. */
static uint32_t tos_row_signature(int y)
{
    const uint16_t *p=(const uint16_t *)(const void *)(tos_screen+(uint32_t)(y*2)*80u);
    uint32_t i,s=0u;
    for(i=0u;i<40u;i++)s=s*31u+(uint32_t)p[i];
    return s;
}

static void tos_composite_now(void)
{
    int i;
    if(!tos_screen||!vid_blit_mono)return;
    for(i=0;i<TOS_BANDS;i++){
        uint32_t s=0u;int r;
        for(r=0;r<TOS_BAND_ROWS;r++)s=s*31u+tos_row_signature(i*TOS_BAND_ROWS+r);
        tos_band_prev[i]=s;
    }
    tos_band_valid=1u;
    tos_build_scaled();
    vid_blit_mono(tos_win_x,tos_win_y+TOS_TITLE,320,200,tos_scaled,40,15u,0u);
}
static void tos_draw_console_window(void);

/* Called from the 50 Hz timer: recompute at ~10 Hz and re-scale/re-blit only
 * the 20-row bands whose contents changed - our stand-in for the AES w_rlist
 * update rectangles, which is what makes EmuTOS fast even on a 68000. */
void tos_composite_tick(void)
{
    int b,r,y0=-1,y1=-1;
    if(tos_active)tos_poll_input();      /* keep the raw ring from overflowing */
    /* NOTE: redrawing the console window from here (on every log change) was
       tried and reverted: doing that much framebuffer work inside the Timer C
       handler is not safe.  The window is drawn with the desktop instead. */
    if(!tos_window_active||!tos_screen||!vid_blit_mono)return;
    if(++tos_composite_divider<5u)return;
    tos_composite_divider=0u;
    if(!tos_band_valid){tos_composite_now();return;}
    for(b=0;b<TOS_BANDS;b++){
        uint32_t s=0u;
        for(r=0;r<TOS_BAND_ROWS;r++)s=s*31u+tos_row_signature(b*TOS_BAND_ROWS+r);
        if(s!=tos_band_prev[b]){
            tos_band_prev[b]=s;
            if(y0<0)y0=b*TOS_BAND_ROWS;
            y1=(b+1)*TOS_BAND_ROWS;
        }
    }
    if(y0<0)return;
    tos_build_scaled_range(y0,y1);
    vid_blit_mono(tos_win_x,tos_win_y+TOS_TITLE+y0,320,y1-y0,
                  tos_scaled+(uint32_t)y0*40u,40,15u,0u);
}

/* The console window: shell/TOS output lives here, never on the raw screen. */
static void tos_draw_console_window(void)
{
    int32_t n=kernel_console_log_count(),i;
    vid_fill(0,312,640,108,15u);
    vid_fill(2,314,636,104,0u);
    vid_text(4,314,"CONSOLE",0,15u);
    for(i=0;i<7;i++){
        int32_t idx=n-7+i;
        if(idx<0)continue;
        vid_text(4,326+i*12,kernel_console_log_line(idx),15,0u);
    }
}

/* Called from the kernel main loop (shell context - never from the Timer C
 * handler, where this much framebuffer work hung the machine).  Redraws the
 * console window only when the log actually grew. */
void tos_console_refresh(void)
{
    /* Kept for a future THROTTLED refresh.  Calling the full console redraw on
       every log change (i.e. on most shell output) made typing take hundreds
       of milliseconds per character and the machine looked hung, so the window
       is drawn together with the desktop for now. */
    static int32_t last_n=-1;
    int32_t n;
    if(!tos_desktop_active)return;
    n=kernel_console_log_count();
    if(n==last_n)return;
    last_n=n;
    tos_draw_console_window();
}

/* Desktop layout. The application window is smaller than the screen and the
 * private 640x400 TOS screen is shown scaled 2:1 (320x200) inside it, so the
 * desktop, its icons and the status line stay visible around the window. */
static void tos_draw_desktop(const char *name)
{
    int y;
    vid_clear(0u);                                     /* black desktop */
    for(y=0;y<436;y+=2)vid_fill(0,y,640,1,8u);         /* 50% dither */
    /* drive icons */
    vid_fill(10,10,26,22,15u);vid_fill(12,12,22,18,0u);vid_fill(12,14,22,2,15u);
    vid_text(44,16,"FLOPPY A:",15,0u);
    vid_fill(10,44,26,22,15u);vid_fill(12,46,22,18,0u);vid_fill(12,48,22,2,15u);
    vid_text(44,50,"HARDDISK C:",15,0u);
    vid_fill(10,78,26,22,15u);vid_fill(12,80,22,18,0u);vid_fill(12,82,22,2,15u);
    vid_text(44,84,"TRASH",15,0u);
    /* window: 1px frame, title bar, client.  A NULL name means "desktop only"
       (the state after a program exits): drawing the chrome then would leave an
       empty TOS APPLICATION frame behind. */
    /* The console window goes down first: a 1:1 application window is large
       and must cover it (it reappears when the program exits). */
    tos_draw_console_window();
    if(name){
        vid_fill(tos_win_x-2,tos_win_y-2,TOS_WIN_W+4,TOS_WIN_H+TOS_TITLE+4,15u);
        vid_fill(tos_win_x,tos_win_y,TOS_WIN_W,TOS_TITLE,8u);      /* TOS-like title bar */
        vid_text(tos_win_x+4,tos_win_y+3,name[0]?name:"TOS APPLICATION",15,8u);
        vid_fill(tos_win_x+TOS_WIN_W-16,tos_win_y+3,12,12,0u);
        vid_text(tos_win_x+TOS_WIN_W-13,tos_win_y+3,"X",15,0u);
        vid_fill(tos_win_x,tos_win_y+TOS_TITLE,TOS_WIN_W,TOS_WIN_H,0u);
    }
    /* status line */
    vid_fill(0,TOS_STATUS_Y,640,12,15u);
    vid_text(4,TOS_STATUS_Y+2,"ASF DESKTOP  |  RUN <APP.PRG>  |  CTRL+ALT+DEL = RESET",0,15u);
}


static const char *tos_basename(const char *path)
{
    const char *base=path,*p;
    if(!path)return "";
    for(p=path;*p;++p)if(*p=='\\'||*p=='/')base=p+1;
    return base;
}

static int tos_open_window(const char *name)
{
    if(!tos_window_enabled)return 0;     /* T1: console stays visible by default */
    if(!vid_enter||!vid_fill||!vid_text||!vid_blit_mono)return 0;
    tos_win_x=TOS_WIN_X+(int32_t)(tos_window_seq%3u)*16;
    tos_win_y=TOS_WIN_Y;
    ++tos_window_seq;
    vid_enter();
    tos_desktop_active=1u;
    tos_draw_desktop(name);
    tos_window_active=1u;
    tos_composite_now();
    return 1;
}

/* The BIOS console and the 8bpp desktop share the ET4000 memory, so after
 * switching back to the text mode the card holds the desktop's pixels. The
 * console must be cleared before anything is printed again. */
int32_t kernel_tos_desktop_active(void){return tos_desktop_active?1:0;}

static void tos_desktop_leave(void)
{
    if(vid_leave)vid_leave();
    if(console_clear_hook)console_clear_hook();   /* resets the console cursor */
    if(vid_clear)vid_clear(0u);                   /* wipe the whole card memory */
    tos_desktop_active=0u;
}

static void tos_close_window(void)
{
    if(!tos_window_active)return;
    tos_window_active=0u;
    /* While the window mode is on the desktop stays: only the application
       window disappears.  Leaving the graphics mode here was why the shell
       "came back" after every program. */
    if(tos_window_enabled){tos_draw_desktop(0);return;}   /* clean desktop */
    tos_desktop_leave();
}
int32_t kernel_tos_run_program(const char *path)
{
    uint32_t size=0u,tlen,dlen,blen,slen,reloc,total,loaded=0u,i;
    uint8_t attr=0;
    uint8_t *file,*mem,*text,*data,*bss;
    void *entry;
    int32_t rc;
    int slot;

    if(!path||!*path)return -1;
    if(kfat_stat_path(path,&size,&attr)!=FAT_FILE_FOUND||size<28u)return -2;
    file=(uint8_t *)kmem_alloc(size,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);
    if(!file)return -3;
    if(kfat_load_path(path,file,size,&loaded)!=FAT_FILE_FOUND||loaded<28u){
        kmem_free(file,KMEM_OWNER_KERNEL);return -4;
    }
    if((((uint32_t)file[0]<<8)|file[1])!=0x601au){
        kmem_free(file,KMEM_OWNER_KERNEL);return -5;
    }
    tlen=rd32(file+2);dlen=rd32(file+6);blen=rd32(file+10);slen=rd32(file+14);
    (void)slen;
    if(tlen==0u||tlen>0x70000u||dlen>0x70000u||blen>0x70000u||28u+tlen+dlen>loaded){
        kmem_free(file,KMEM_OWNER_KERNEL);return -6;
    }
    total=0x100u+tlen+dlen+blen+0x4000u;
    mem=(uint8_t *)kmem_alloc(total,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE|KMEM_EXEC,1);
    if(!mem){kmem_free(file,KMEM_OWNER_KERNEL);return -7;}
    text=mem+0x100u;data=text+tlen;bss=data+dlen;
    for(i=0u;i<tlen+dlen;++i)text[i]=file[28u+i];
    for(i=0u;i<blen;++i)bss[i]=0u;
    reloc=28u+tlen+dlen;
    while(reloc+4u<=loaded){
        uint32_t r=rd32(file+reloc);
        if(r==0u)break;
        reloc+=4u;
        if(r<tlen+dlen)wr32(text+r,rd32(text+r)+(uint32_t)(uintptr_t)mem);
    }
    wr32(mem+0x00u,(uint32_t)(uintptr_t)mem);
    wr32(mem+0x04u,(uint32_t)(uintptr_t)(mem+total));
    wr32(mem+0x08u,(uint32_t)(uintptr_t)text);
    wr32(mem+0x0cu,tlen);
    wr32(mem+0x10u,(uint32_t)(uintptr_t)data);
    wr32(mem+0x14u,dlen);
    wr32(mem+0x18u,(uint32_t)(uintptr_t)bss);
    wr32(mem+0x1cu,blen);
    wr32(mem+0x20u,(uint32_t)(uintptr_t)(mem+0x80u));
    wr32(mem+0x24u,0u);wr32(mem+0x28u,0u);wr32(mem+0x2cu,0u);
    {uint32_t k3=0u;const char *args=kernel_user_get_arguments();
     while(path[k3]&&k3<100u){mem[0x81u+k3]=(uint8_t)path[k3];++k3;}
     if(args&&args[0]){mem[0x81u+k3]=' ';++k3;
        {uint32_t j=0u;while(args[j]&&k3<126u){mem[0x81u+k3]=(uint8_t)args[j];++k3;++j;}}}
     mem[0x80]=(uint8_t)k3;mem[0x81u+k3]=0u;}

    tos_screen=(uint8_t *)(uintptr_t)(mem+total+16u);
    /* The program screen lives after the program image (32 KB). */
    tos_screen=(uint8_t *)kmem_alloc(32768u,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(tos_screen){for(i=0u;i<32768u;++i)tos_screen[i]=0u;}
    tos_mode=2u;
    {int k2;for(k2=0;k2<TOS_MAX_HANDLES;k2++)tos_files[k2].used=0u;}
    tos_dta=0;tos_dir_count=0;tos_dir_index=0;tos_key_pending=-2;
    tos_heap=(uint8_t *)kmem_alloc(TOS_HEAP_SIZE,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    tos_heap_used=0u;
    for(i=0u;i<16u;++i)tos_palette[i]=(uint16_t)(i*0x111u);

    if(tos_cpu_type>=2u)kernel_image_cache_sync(tos_cpu_type);

    {
        volatile uint32_t *vec=(volatile uint32_t *)(uintptr_t)0x00000000UL;
        static const uint16_t vidx[13]={2u,3u,4u,5u,6u,7u,8u,10u,11u,33u,34u,45u,46u};
        uint32_t hnd[13],k;
        hnd[0]=(uint32_t)(uintptr_t)asf_tos_fault2;
        hnd[1]=(uint32_t)(uintptr_t)asf_tos_fault3;
        hnd[2]=(uint32_t)(uintptr_t)asf_tos_fault4;
        hnd[3]=(uint32_t)(uintptr_t)asf_tos_fault5;
        hnd[4]=(uint32_t)(uintptr_t)asf_tos_fault6;
        hnd[5]=(uint32_t)(uintptr_t)asf_tos_fault7;
        hnd[6]=(uint32_t)(uintptr_t)asf_tos_fault8;
        hnd[7]=(uint32_t)(uintptr_t)asf_tos_linea;
        hnd[8]=(uint32_t)(uintptr_t)asf_tos_fault11;
        hnd[9]=(uint32_t)(uintptr_t)asf_tos_trap1;
        hnd[10]=(uint32_t)(uintptr_t)asf_tos_trap2;
        hnd[11]=(uint32_t)(uintptr_t)asf_tos_trap13;
        hnd[12]=(uint32_t)(uintptr_t)asf_tos_trap14;
        for(k=0u;k<13u;++k)tos_saved_vec[k]=vec[vidx[k]];
        for(k=0u;k<13u;++k)vec[vidx[k]]=hnd[k];
        entry=text;
        /* Register the run as a task slot.  G1.3b hands the slot to the
           scheduler instead of running it inline. */
        slot=tos_task_alloc();
        if(slot>=0){
            tos_task_current=(uint8_t)slot;
            tos_task[slot].used=1u;
            tos_task[slot].sp=0u;
            tos_task[slot].basepage=mem;
            tos_task[slot].entry=(uint8_t *)entry;
            tos_task[slot].pid=0u;
            tos_task[slot].state=0;
            tos_task[slot].mem=0;
            tos_task[slot].stack=0;
            tos_task[slot].total=0u;
        }
        tos_keyq_head=tos_keyq_tail=0u;
        tos_active=1u;
        tos_rawlog_n=0u;tos_rawlog_i=0u;
        tos_open_window(tos_basename(path));
        asf_tos_enter(mem,entry,(void *)(mem+total-16u));
        tos_active=0u;
        tos_composite_tick();
        tos_close_window();
        tos_task_free(slot);
        for(k=0u;k<13u;++k)vec[vidx[k]]=tos_saved_vec[k];
    }
    kmem_free(file,KMEM_OWNER_KERNEL);
    if(tos_screen)kmem_free(tos_screen,KMEM_OWNER_KERNEL);
    if(tos_heap)kmem_free(tos_heap,KMEM_OWNER_KERNEL);
    tos_screen=0;tos_heap=0;
    kmem_free(mem,KMEM_OWNER_KERNEL);
    kblock_flush();
    /* Only report the cache when it is actually enabled: an unconditional
       "DISK CACHE: OFF ..." after every program was pure noise. */
    tos_print("TOS TASKS LEFT: ");tos_dec((uint32_t)tos_task_count());tos_print("\n");
    if(tos_last_scancode){
        tos_print("TOS EXIT KEY: scancode 0x");
        {const char *hx="0123456789ABCDEF";tos_putc(hx[(tos_last_scancode>>4)&15u]);tos_putc(hx[tos_last_scancode&15u]);}
        tos_print("  raw(16):");
        {const char *hx2="0123456789ABCDEF";uint8_t k;
         for(k=0u;k<tos_rawlog_n;++k){
            uint8_t b=tos_rawlog[(uint8_t)((tos_rawlog_i+16u-tos_rawlog_n+k)&15u)];
            tos_putc(' ');tos_putc(hx2[(b>>4)&15u]);tos_putc(hx2[b&15u]);
        }}
        tos_print("\n");
        tos_last_scancode=0u;tos_rawlog_n=0u;tos_rawlog_i=0u;
    }
    if(kblock_cache_enabled()){
        tos_print("DISK CACHE: ");tos_dec(kblock_cache_slots());tos_print(" sectors, hits=");
        tos_dec(kblock_cache_hits());tos_print(" misses=");tos_dec(kblock_cache_misses());
        tos_print(" writebacks=");tos_dec(kblock_cache_flushes());tos_print("\n");
    }
    rc=tos_exit_code;
    if(rc<0)tos_print("TOS PROGRAM FAULT\n");
    return rc;
}
