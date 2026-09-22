/* TOS personality (T1): load a 0x601A .PRG, build its basepage, relocate it
 * and run it in USER mode (SR=0x0700, IPL 7 masked) with the TOS trap vectors
 * installed.
 *
 * LEGACY / EXPERIMENTAL (ATOS v10, Phase 15): the whole TOS persona is frozen
 * while the EmuTOS compatibility route is analysed.  The AES below
 * (tos_aes_dispatch) is a minimal experimental stub and MUST NOT grow new
 * AES opcodes, menus, dialogs or fonts in v10.  Only fixes needed to run the
 * compatibility test suite are allowed.
 *
 * Trap ABI: GEMDOS reads the function word from the user stack; BIOS/XBIOS in
 * classic TOS use D0.W (flagged in the v10 audit, see docs/V10-STATUS.md). */
#include <stdint.h>
#include "kernel_memory.h"
#include "kernel_fat.h"
#include "kernel_block.h"
#include "kernel_graphics.h"
#include "font.h"
#include "tos_prg.h"
#include "tos_vdi.h"

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
extern void kernel_wm_damage_all(void);
extern void kernel_wm_frame_tick(uint32_t now);
extern uint32_t asf_tos_supexec(void *fn,uint32_t arg3);
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
extern volatile uint32_t kernel_wall_ticks;
extern const char *kernel_user_get_arguments(void);

static void (*tos_puts)(const char *);
static uint8_t tos_cpu_type;
/* Forward declarations: the fault handler below prints through these. */
static void tos_print(const char *s);
static void tos_dec(uint32_t v);
static uint32_t tos_last_fn;
volatile uint32_t tos_dbg_exit_fn,tos_dbg_exit_pc;
volatile uint32_t kernel_fault_regs[16];
/* TOS system variables block that _sysbase (0x4F2) points to. Programs read
   it via the low-memory pointers; addresses come from MagiC's own
   KAOS/UTILS/OSBIND.INC, not guesswork. */
uint8_t tos_syshdr[512];


/* Cookie jar: "cook" magic, entry count, then 8-byte tag + 4-byte value.
   Getcookie is the FIRST thing a system-information program does; without it
   SysInfo has no idea what it is running on. _FPU says "no FPU" so programs
   do not probe with a Line-F instruction at all. */
static const uint8_t tos_cookie_jar[]={
 'c','o','o','k',0,0,0,7,
 '_','G','E','M',0,0,0,0, 0x00,0x01,0x03,0x00,
 '_','V','D','I',0,0,0,0, 0x00,0x03,0x00,0x00,
 '_','M','C','H',0,0,0,0, 0x00,0x00,0x00,0x00,
 '_','C','P','U',0,0,0,0, 0x00,0x00,0x00,0x1e,
 '_','F','P','U',0,0,0,0, 0x00,0x00,0x00,0x00,
 '_','A','K','P',0,0,0,0, 0x00,0x00,0x00,0x00,
 '_','V','D','O',0,0,0,0, 0x00,0x00,0x00,0x00
};

/* Survives a CPU halt: read it back with the Hatari debugger ("m <addr> 64").
   Each entry = (fn << 16) | (result & 0xffff); index 0 holds the count. */
uint32_t tos_call_log[257];

static uint8_t tos_trace_gemdos=1u;      /* live GEMDOS call trace on the console */

/* Separate trace channel: the BIOS console works from the very first boot line,
   unlike the personality console which needs the desktop up. */
static void (*tos_trace_puts)(const char *);
void kernel_tos_set_trace(void (*p)(const char *)){tos_trace_puts=p;}
static void tos_trace(const char *s){if(tos_trace_puts&&s)tos_trace_puts(s);}
static int32_t tos_exit_code;
static uint32_t tos_saved_vec[13];
static uint8_t *tos_screen;          /* TOS screen: private buffer or the real one */
static uint8_t tos_screen_owned;     /* 1 = tos_screen was kmem_alloc'd */
static uint16_t tos_date=0x5d2bu,tos_time=0x6000u;   /* software GEMDOS clock */
/* The TOS persona runs with IPL 7, so the polled kernel timer cannot advance
   while it spins.  Tgettime therefore reads the MFP Timer C counter directly
   (free-running 200 Hz, independent of the interrupt mask) and synthesises a
   real clock.  SysInfo waits for its splash delay via Tgettime in a tight
   loop, which never ended while the clock was frozen. */
static uint8_t tos_mfp_last,tos_mfp_ready;
static uint32_t tos_mfp_ticks=12u*360000u;   /* start at 12:00:00 */
static void tos_clock_tick(void)
{
    uint8_t now=*(volatile uint8_t *)(uintptr_t)0xfffffa23u;
    if(!tos_mfp_ready){tos_mfp_last=now;tos_mfp_ready=1u;return;}
    if(now>tos_mfp_last)tos_mfp_ticks++;
    tos_mfp_last=now;
    /* Publish the 200 Hz tick in the TOS sysvar _hz_200 (0x4BA) too: programs
       (SysInfo's CPU benchmark) divide by its delta, and it used to be stuck
       at 0, which faulted. */
    {void *p=(void *)(uintptr_t)0x4BAu;uint32_t v=tos_mfp_ticks;
     __asm__ volatile("move.l %1,%0@" : : "a"(p),"d"(v) : "memory");}
}
static uint8_t tos_window_enabled;      /* T2 window is opt-in (XBIOS 0xFE cmd 7) */
static uint8_t tos_desktop_active;      /* the graphics desktop is on screen */
static void tos_desktop_leave(void);
static uint8_t  tos_mode=2u;         /* 0 low, 1 medium, 2 high */
static uint16_t tos_palette[16];

static int32_t (*tos_input_pop)(void);

void kernel_tos_set_console(void (*puts)(const char *)){tos_puts=puts;}
void kernel_tos_set_cpu(uint8_t cpu_type){tos_cpu_type=cpu_type;}
void kernel_tos_set_input(int32_t (*pop)(void)){tos_input_pop=pop;}

/* Task 4: let the compositor own the TOS screen.  The window manager supplies
   a surface; the personality draws into it and Physbase() returns it, so the
   program runs in a window instead of the shared framebuffer. */
extern volatile uint8_t kernel_tos_desktop_visible;
void tos_linea_init(void);
static void tos_lm_wr32(void *address,uint32_t value)
{
    __asm__ volatile("move.l %1,%0@" : : "a"(address),"d"(value) : "memory");
}
static void tos_lm_wr8(void *address,uint8_t value)
{
    __asm__ volatile("move.b %1,%0@" : : "a"(address),"d"((uint32_t)value) : "memory");
}
void kernel_tos_use_surface(uint8_t *surface,uint16_t pitch)
{
    if(!surface)return;
    kernel_tos_desktop_visible=1u;
    tos_window_enabled=1u;
    tos_screen=surface;
    tos_screen_owned=0u;
    tos_vdi_set_surface(surface,pitch);
    /* Keep the TOS low-memory screen model consistent: a real TOS keeps
       _v_bas_ad (0x44E) == Physbase() == Logbase() and _screensize (0x44C) in
       bytes.  Programs that read these directly instead of calling XBIOS would
       otherwise draw into the kernel framebuffer and get erased by the
       compositor.  (Constant address casts trip -Werror=array-bounds, so the
       stores go through inline asm, same as the kernel's own sysvar writes.) */
    tos_lm_wr32((void *)(uintptr_t)0x44Eu,(uint32_t)(uintptr_t)surface);
    /* 0x44C is NOT _screensize in TOS/MagiC: it is sshiftmd, the current
       shifter mode byte (0=low,1=medium,2=high).  Writing a byte count there
       corrupts it, so only mirror the resolution byte. */
    tos_lm_wr8((void *)(uintptr_t)0x44Cu,(uint8_t)tos_mode);
    /* Give the program a live Line-A page: base, pitch, system font, tables. */
    tos_linea_init();
}

/* A fault inside the TOS program ends it and returns to the shell. */
uint32_t kernel_linef_pc,kernel_linef_op,kernel_linef_frame[6];
volatile uint32_t kernel_tos_dbg_basepage,kernel_tos_dbg_entry,kernel_tos_dbg_total;
extern volatile uint32_t kernel_fault_frame[6];
void asf_tos_fault(uint32_t vector)
{
    uint32_t pc;
    tos_print("TOS FAULT vec=");tos_dec(vector&0xffffu);
    /* Task 3: the exception stub captured the raw frame in kernel_fault_frame.
       The user PC is the first longword that lands in the program's code
       window; record it and the opcode so the missing call can be identified
       from normal context / the debugger. */
    pc=kernel_fault_frame[2];
    kernel_linef_pc=pc;
    kernel_linef_op=(pc>=0x00040000u&&pc<0x00c00000u)?
        (uint32_t)*(const uint16_t *)(uintptr_t)pc:0u;
    /* Divide-by-zero (vector 5) is not fatal inside the persona.  The CPU
       leaves the stacked PC on the following instruction, so returning to the
       stub's RTE resumes the caller with a stale result.  SysInfo's CPU-speed
       benchmark runs its delay loop through XBIOS Supexec and divides by the
       _hz_200 delta; our timer is polled, so the count cannot advance while
       the loop runs, and the divide would otherwise kill the program. */
    if((vector&0xffffu)==5u)return;
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

/* Kernel-side session hotkey state.  TOS keys arrive as raw scancodes on the
   input service, not through kernel_user_keyboard_event(), so Shift+Alt+F10
   has to be detected here; otherwise there is no way to switch sessions while
   a TOS GUI owns the keyboard. */
extern void ksessions_hotkey_cycle(void);
static uint8_t tos_kb_shift,tos_kb_alt;
void tos_poll_input(void)
{
    int guard=64;
    while(guard-->0){
        int32_t sc=tos_next_scancode();
        uint8_t next;
        int a;
        if(sc<0)break;
        if(sc==0x2a||sc==0x36)tos_kb_shift=1u;
        else if(sc==0xaa||sc==0xb6)tos_kb_shift=0u;
        else if(sc==0x38)tos_kb_alt=1u;
        else if(sc==0xb8)tos_kb_alt=0u;
        else if(sc==0x44&&tos_kb_shift&&tos_kb_alt){
            tos_kb_shift=0u;tos_kb_alt=0u;ksessions_hotkey_cycle();}
        tos_key_update((uint8_t)sc);
        a=tos_key_ascii((uint8_t)sc,tos_shift,tos_caps);
        if(!a){
            /* The kernel input hook (kernel_user_terminal_getchar) already
               returns a decoded ASCII character - it applies shift/caps and
               filters mouse packets itself - so it is NOT a raw scancode.
               Accept it directly; otherwise every key was dropped here and
               SysInfo stayed blocked in Cconin. */
            uint8_t ch=(uint8_t)sc;
            if(ch>=0x20u||ch==0x0du||ch==0x08u||ch==0x1bu||ch==0x09u){a=ch;sc=0;}
        }
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
        /* The TOS program runs at IPL 7, so Timer C cannot fire while we spin
           here and the queue would never fill (SysInfo deadlocked on its first
           key wait).  Poll the IKBD/event path directly: with interrupts
           masked we are the only consumer, so this cannot race Timer C. */
        tos_poll_input();
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
    /* GEMDOS/BIOS/XBIOS pass the function number as a WORD at the top of the
       user stack; the arguments follow it (EmuTOS _enter reads (usp)). */
    return (uint16_t *)(uintptr_t)(usp+2u);
}

/* --- program screen access (mono 2 / medium 1 / low 0) --------------------- */
/* Line-A opcode histogram + how many pixels Line-A actually painted: shows
   whether SysInfo blits/plots through Line-A or writes memory directly. */
uint32_t tos_linea_opcount[256];
uint32_t tos_linea_pixels;
static void tos_putpixel(int x,int y,uint8_t color)
{
    if(!tos_screen||x<0||y<0)return;
    ++tos_linea_pixels;
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

/* --- TOS Line-A (0xA000-0xA00F) -------------------------------------------
   The Line-A variable page.  A program that paints the screen itself (SysInfo
   does) reads this page for the screen pitch/base, the system font and the VDI
   tables; A_INIT returns its address.  Offsets follow the TOS/MagiC layout
   (MagiC VDI INCLUDE/LINEA.I).  The page lives inside a larger buffer with the
   base 0x400 bytes in, so the negative offsets (V_FNT_AD, DEV_TAB, SIZ_TAB,
   INQ_TAB, mouse/VT52 data) are all addressable. */
#define LA_BASE_OFF 0x400
static uint8_t tos_linea_page[0x800];
#define LA (tos_linea_page+LA_BASE_OFF)
static int16_t tos_linea_contrl[16],tos_linea_intin[32],tos_linea_ptsin[32];
static int16_t tos_linea_intout[32],tos_linea_ptsout[32];
static uint32_t tos_linea_font_tab[2];
uint32_t kernel_linea_base,kernel_linea_font,kernel_linea_tab,kernel_linea_ainit;
static void la_w8(int off,int v){LA[off]=(uint8_t)v;}
static void la_w16(int off,int v){LA[off]=(uint8_t)(v>>8);LA[off+1]=(uint8_t)v;}
static void la_w32(int off,uint32_t v)
{LA[off]=(uint8_t)(v>>24);LA[off+1]=(uint8_t)(v>>16);LA[off+2]=(uint8_t)(v>>8);LA[off+3]=(uint8_t)v;}
void tos_linea_init(void)
{
    uint32_t font=(uint32_t)(uintptr_t)font_system_8x8();
    unsigned i;
    for(i=0;i<sizeof tos_linea_page;i++)tos_linea_page[i]=0u;
    /* positive offsets */
    la_w16(0x00,1);                 /* PLANES        */
    la_w16(0x02,80);                /* WIDTH (bytes/line) */
    la_w32(0x04,(uint32_t)(uintptr_t)tos_linea_contrl);
    la_w32(0x08,(uint32_t)(uintptr_t)tos_linea_intin);
    la_w32(0x0c,(uint32_t)(uintptr_t)tos_linea_ptsin);
    la_w32(0x10,(uint32_t)(uintptr_t)tos_linea_intout);
    la_w32(0x14,(uint32_t)(uintptr_t)tos_linea_ptsout);
    la_w16(0x18,0);la_w16(0x1a,-1);la_w16(0x1c,-1);la_w16(0x1e,-1); /* COLBIT0..3 */
    la_w16(0x20,-1);la_w16(0x22,-1);                                 /* LSTLIN/LNMASK */
    la_w16(0x36,0);                                                  /* CLIP off */
    la_w16(0x38,0);la_w16(0x3a,0);la_w16(0x3c,639);la_w16(0x3e,399); /* clip rect */
    la_w32(0x54,font);la_w16(0x58,1);                                /* FBASE/FWIDTH */
    /* negative offsets */
    la_w16(-0x2e,8);                /* V_CEL_HT  */
    la_w16(-0x2c,79);               /* V_CEL_MX  */
    la_w16(-0x2a,24);               /* V_CEL_MY  */
    la_w16(-0x28,80);               /* V_CEL_WR  */
    la_w16(-0x26,0);la_w16(-0x24,1);/* V_COL_BG/FG */
    la_w32(-0x22,0);la_w32(-0x1e,0);la_w16(-0x1c,0);la_w16(-0x1a,0);
    la_w16(-0x18,0);la_w8(-0x17,0);
    la_w32(-0x16,font);             /* V_FNT_AD  */
    la_w16(-0x12,32);               /* V_FNT_ND  */
    la_w16(-0x10,126);              /* V_FNT_ST  */
    la_w16(-0x0e,1);                /* V_FNT_WD  */
    la_w16(-0x0c,640);              /* V_REZ_HZ  */
    la_w32(-0x0a,0);                /* V_OFF_AD  */
    la_w16(-0x04,400);              /* V_REZ_VT  */
    la_w16(-0x02,80);               /* BYTES_LIN */
    la_w16(-0x158,320);la_w16(-0x156,200);la_w16(-0x154,0);la_w8(-0x153,0);
    la_w16(-0x256,1);               /* M_HID_CT  */
    la_w32(-0x1d0,0);               /* CUR_WORK  */
    la_w32(-0x1cc,0);               /* DEF_FONT  */
    la_w32(-0x1c8,0);la_w32(-0x1c4,0);la_w32(-0x1c0,0);la_w32(-0x1bc,0); /* FONT_RING */
    /* DEV_TAB (45 words): max x/y, pixel size, colours ... */
    la_w16((-0x2b4)+0,639);la_w16((-0x2b4)+2,399);
    la_w16((-0x2b4)+6,1);la_w16((-0x2b4)+8,1);
    la_w16((-0x2b4)+26,2);la_w16((-0x2b4)+70,3);la_w16((-0x2b4)+78,2);
    /* SIZ_TAB (15 words): max char width/height, marker ... */
    la_w16((-0x1f2)+4,8);la_w16((-0x1f2)+6,8);
    la_w16((-0x1f2)+20,8);la_w16((-0x1f2)+22,8);
    /* INQ_TAB (45 words): device type, gradations, planes, CLUT ... */
    la_w16((-0x30e)+0,4);la_w16((-0x30e)+2,2);la_w16((-0x30e)+8,1);
    la_w16((-0x30e)+10,1);la_w16((-0x30e)+12,0);la_w16((-0x30e)+38,1);
    tos_linea_font_tab[0]=font;tos_linea_font_tab[1]=0;
    kernel_linea_base=(uint32_t)(uintptr_t)LA;
    kernel_linea_font=(uint32_t)(uintptr_t)font;
    kernel_linea_tab=(uint32_t)(uintptr_t)tos_linea_font_tab;
}

int32_t asf_tos_linea_op(uint32_t op,uint32_t *block)
{
    const uint8_t *b=(const uint8_t *)block;
    ++tos_linea_opcount[op&0xffu];
    int x1,y1,x2,y2;uint8_t color;
    if(!b)return 0;
    x1=(int16_t)rd16(b+38u);y1=(int16_t)rd16(b+40u);
    x2=(int16_t)rd16(b+42u);y2=(int16_t)rd16(b+44u);
    color=(uint8_t)((rd16(b+24u)?1u:0u)|(rd16(b+26u)?2u:0u)|(rd16(b+28u)?4u:0u)|(rd16(b+30u)?8u:0u));
    switch(op){
    case 0u: kernel_linea_ainit=1u;                           /* A_INIT */
             return (int32_t)kernel_linea_base;
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
#define TOS_HEAP_SIZE   4194304u   /* 4 MB from TT-RAM: real TOS programs
                                      allocate far more than 64 KB */
#define TOS_DIR_MAX     16

typedef struct {uint8_t used;uint32_t size,pos;char path[96];} tos_file_t;
typedef struct {char name[13];uint32_t size;uint8_t attr;} tos_dirent_t;

static tos_file_t tos_files[TOS_MAX_HANDLES];
static uint8_t *tos_heap;
static uint32_t tos_heap_used;
/* Diagnostic: bytes requested by every GEMDOS Malloc (including the -1 free
   query).  Used to see how the TOS persona runs out of heap. */
uint32_t tos_memlog[128];
uint32_t tos_memlog_n;
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
    /* The listing callback continues while it returns non-zero; 0 stops the
       scan.  This used to return 0 for every entry, so Fsfirst reported a
       single file per directory. */
    if(tos_dir_count>=TOS_DIR_MAX)return 0;
    if(!tos_pat_match((const char *)ctx,name))return 1;
    {int i;for(i=0;i<12&&name[i];i++)tos_dir[tos_dir_count].name[i]=name[i];
     tos_dir[tos_dir_count].name[i]=0;}
    tos_dir[tos_dir_count].size=size;
    tos_dir[tos_dir_count].attr=(uint8_t)attributes;
    tos_dir_count++;
    return 1;
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
    /* DOS/NC semantics: *.* matches names without an extension too. */
    if(pat[0]=='*'&&pat[1]=='.'&&pat[2]=='*'&&pat[3]==0)pat[1]=0;
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
static uint8_t tos_env_ready;
/* TOS-compatible environment bootstrap. A TOS program ASSUMES the system is
   already up: it READS the cookie jar, the system variables and the VDI/AES
   state. Bring them up in order (XBIOS, GEMDOS, VDI, AES) BEFORE the first
   TOS program runs - exactly what MagiC/EmuTOS do at boot. */
int kernel_tos_bootstrap(void)
{
    static int16_t contrl[16],intin[128],ptsin[8],intout[128],ptsout[8];
    unsigned i;
    if(tos_env_ready)return 1;
    tos_print("TOS ENV: XBIOS GEMDOS ");
    for(i=0u;i<16u;++i)contrl[i]=0;
    for(i=0u;i<128u;++i){intin[i]=1;intout[i]=0;}
    for(i=0u;i<8u;++i){ptsin[i]=0;ptsout[i]=0;}
    contrl[0]=0x73;contrl[2]=11;contrl[3]=45;contrl[4]=6;
    (void)tos_vdi_call(0x73u,contrl,intin,ptsin,intout,ptsout,0,0);
    tos_print("VDI AES READY\n");
    tos_env_ready=1u;
    return 1;
}
/* Real first-fit heap with block headers, freeing and coalescing.  The old
   bump allocator never reclaimed anything (Mfree was a no-op), so a program
   that allocated, freed and reallocated - which SysInfo does while building
   its UI - falsely ran out of memory. */
typedef struct tos_hdr { uint32_t size; uint32_t used; } tos_hdr_t;
static uint8_t tos_heap_init_done;
static void *tos_malloc(uint32_t bytes)
{
    uint8_t *p;
    if(!tos_heap)return 0;
    if(!tos_heap_init_done){
        tos_hdr_t *h=(tos_hdr_t *)(void *)tos_heap;
        h->size=TOS_HEAP_SIZE-(uint32_t)sizeof(tos_hdr_t);
        h->used=0u;
        tos_heap_used=0u;
        tos_heap_init_done=1u;
    }
    bytes=(bytes+3u)&~3u;              /* 4-byte alignment */
    if(bytes==0u)bytes=4u;
    for(p=tos_heap;p+sizeof(tos_hdr_t)<=tos_heap+TOS_HEAP_SIZE;){
        tos_hdr_t *h=(tos_hdr_t *)(void *)p;
        if(h->size==0u||h->size>=(uint32_t)(tos_heap+TOS_HEAP_SIZE-p))break;
        if(!h->used&&h->size>=bytes){
            /* Split when the remainder can hold a header and a minimal block. */
            if(h->size>=bytes+(uint32_t)sizeof(tos_hdr_t)+4u){
                tos_hdr_t *n=(tos_hdr_t *)(void *)(p+sizeof(tos_hdr_t)+bytes);
                n->size=h->size-bytes-(uint32_t)sizeof(tos_hdr_t);
                n->used=0u;
                h->size=bytes;
            }
            h->used=1u;
            tos_heap_used+=h->size+(uint32_t)sizeof(tos_hdr_t);
            return p+sizeof(tos_hdr_t);
        }
        p+=sizeof(tos_hdr_t)+h->size;
    }
    return 0;
}
static uint32_t tos_largest_free(void)
{
    uint32_t best=0u;uint8_t *p;
    if(!tos_heap)return 0u;
    if(!tos_heap_init_done)return TOS_HEAP_SIZE-(uint32_t)sizeof(tos_hdr_t);
    for(p=tos_heap;p+sizeof(tos_hdr_t)<=tos_heap+TOS_HEAP_SIZE;){
        tos_hdr_t *h=(tos_hdr_t *)(void *)p;
        if(h->size==0u||h->size>=(uint32_t)(tos_heap+TOS_HEAP_SIZE-p))break;
        if(!h->used&&h->size>best)best=h->size;
        p+=sizeof(tos_hdr_t)+h->size;
    }
    return best;
}
static int tos_mfree(void *p)
{
    uint8_t *q=(uint8_t *)p;
    if(!q||!tos_heap||!tos_heap_init_done)return 0;
    if(q<tos_heap+sizeof(tos_hdr_t)||q>=tos_heap+TOS_HEAP_SIZE)return 0;
    {
        tos_hdr_t *h=(tos_hdr_t *)(void *)(q-sizeof(tos_hdr_t));
        uint8_t *r;
        if(!h->used)return 0;
        h->used=0u;
        if(tos_heap_used>=h->size+(uint32_t)sizeof(tos_hdr_t))
            tos_heap_used-=h->size+(uint32_t)sizeof(tos_hdr_t);
        else tos_heap_used=0u;
        /* Coalesce every adjacent free pair in one forward walk. */
        for(r=tos_heap;r+sizeof(tos_hdr_t)<=tos_heap+TOS_HEAP_SIZE;){
            tos_hdr_t *b=(tos_hdr_t *)(void *)r;
            if(b->size==0u||b->size>=(uint32_t)(tos_heap+TOS_HEAP_SIZE-r))break;
            if(!b->used){
                tos_hdr_t *n=(tos_hdr_t *)(void *)(r+sizeof(tos_hdr_t)+b->size);
                if((uint8_t *)n+sizeof(tos_hdr_t)<=tos_heap+TOS_HEAP_SIZE&&
                   n->size&&!n->used){
                    b->size+=sizeof(tos_hdr_t)+n->size;
                    continue;
                }
            }
            r+=sizeof(tos_hdr_t)+b->size;
        }
    }
    return 0;
}

/* --- TOS GEMDOS (trap #1) -------------------------------------------------- */
static int32_t tos_gemdos_impl(uint32_t fn,uint32_t *frame,uint32_t usp);
/* Capture the user stack when a TOS program terminates, so the call chain
   that decided to exit can be disassembled afterwards. */
volatile uint32_t tos_exit_usp,tos_exit_stack[16];
static void tos_exit_capture(uint32_t usp)
{
    uint8_t *p=(uint8_t *)(uintptr_t)usp;uint32_t k;
    tos_exit_usp=usp;
    for(k=0;k<16u;k++)tos_exit_stack[k]=p?rd32(p+4u*k):0u;
}
/* Public entry: log function number AND result so a call returning 0 that the
   program then dereferences is visible on the console. */
int32_t asf_tos_gemdos(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    int32_t r;
    tos_clock_tick();
    r=tos_gemdos_impl(fn,frame,usp);
    if(tos_trace_gemdos){tos_print("#");tos_dec(fn&0xffffu);tos_print("=");tos_dec((uint32_t)r);tos_print(" ");}
    {uint32_t n=tos_call_log[0];
     if(n<256u){tos_call_log[1u+n]=((fn&0xffffu)<<16)|((uint32_t)r&0xffffu);tos_call_log[0]=n+1u;}}
    return r;
}
static int32_t tos_gemdos_impl(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    uint16_t *a=tos_args(frame,usp);
    fn&=0xffffu;                       /* only the low word is the function */
    tos_last_fn=fn;                    /* remember, for fault diagnostics */
    if(tos_trace_gemdos){tos_print("#");tos_dec(fn);tos_print(" ");}
    switch(fn){
    case 0x00u: tos_dbg_exit_fn=0x00u;tos_dbg_exit_pc=frame?rd32((const uint8_t *)frame+2u):0u;tos_exit_capture(usp);tos_exit_code=0;asf_tos_exit();return 0; /* Pterm0 */
    case 0x01u: case 0x08u: return tos_getkey();                        /* Cconin/Cnecin */
    case 0x02u: tos_putc((char)(uintptr_t)a[0]);return 0;                /* Cconout */
    case 0x06u: case 0x07u: return tos_getkey();                        /* Crawio/Crawcin */
    case 0x09u: {uint32_t p=((uint32_t)a[0]<<16)|a[1];if(p)tos_print((const char *)(uintptr_t)p);return 0;} /* Cconws */
    case 0x0bu: return tos_key_ready();                                  /* Cconis */
    case 0x0eu: return 0;                                                /* Dsetdrv */
    case 0x19u: return 0;                                                /* Dgetdrv */
    case 0x2au: return (int32_t)tos_date;                                /* Tgetdate */
    case 0x2bu: tos_date=rd16((const uint8_t *)a);return 0;              /* Tsetdate */
    case 0x20u: {uint32_t ssp;__asm__ volatile("move.l %%sp,%0":"=r"(ssp));return (int32_t)ssp;} /* Super -> SSP */
    case 0x2cu: {                                                        /* Tgettime */
        uint32_t t;
        tos_clock_tick();
        t=(tos_mfp_ticks/100u)%30u;
        t|=((tos_mfp_ticks/6000u)%60u)<<5;
        t|=((tos_mfp_ticks/360000u)%24u)<<11;
        tos_time=(uint16_t)t;
        return (int32_t)tos_time;}
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
    case 0x14u: {                                                        /* Getcookie */
        const uint8_t *jar=tos_cookie_jar;const uint8_t *tagp=(const uint8_t *)a;
        uint32_t out=((uint32_t)a[4]<<24)|((uint32_t)a[5]<<16)|((uint32_t)a[6]<<8)|a[7];
        uint32_t n=((uint32_t)jar[4]<<24)|((uint32_t)jar[5]<<16)|((uint32_t)jar[6]<<8)|jar[7];
        uint32_t e;
        for(e=0u;e<n;++e){
            const uint8_t *ent=jar+8u+e*12u;
            if(ent[0]==tagp[0]&&ent[1]==tagp[1]&&ent[2]==tagp[2]&&ent[3]==tagp[3]){
                if(out){uint8_t *o=(uint8_t *)(uintptr_t)out;
                    o[0]=ent[8];o[1]=ent[9];o[2]=ent[10];o[3]=ent[11];}
                return 1;
            }
        }
        return 0;}
    case 0x48u: {                                                        /* Malloc */
        /* GEMDOS: Malloc(-1) does NOT allocate; it returns the size of the
           largest free block so packers can size their buffer.  Treating it as
           a 4 GB request made SysInfo think there was no memory and exit. */
        uint32_t bytes=rd32((const uint8_t *)a);
        if(tos_memlog_n<128u)tos_memlog[tos_memlog_n++]=bytes;
        if(bytes==0xffffffffu)return (int32_t)tos_largest_free();
        return (int32_t)(uintptr_t)tos_malloc(bytes);}
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
    case 0x4cu: tos_dbg_exit_fn=0x4cu;tos_dbg_exit_pc=frame?rd32((const uint8_t *)frame+2u):0u;tos_exit_code=(int32_t)rd16((const uint8_t *)a);asf_tos_exit();return 0; /* Pterm */
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
/* XBIOS opcode histogram: shows how SysInfo asks about the screen. */
uint32_t tos_xbios_opcount[256];
int32_t asf_tos_xbios(uint32_t fn,uint32_t *frame,uint32_t usp)
{
    uint16_t *a=tos_args(frame,usp);
    fn&=0xffffu;
    ++tos_xbios_opcount[fn&0xffu];
    switch(fn){
    case 2u: case 3u: return (int32_t)(uintptr_t)(tos_screen?tos_screen:(uint8_t *)(uintptr_t)0x00C00000UL); /* Physbase/Logbase (never 0) */
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
    case 38u: {                                                           /* Supexec */
        /* Run a user callback in supervisor mode.  TOS leaves the callback
           argument in d3; the trap stub saved the user's d3 at frame-48, so
           pass it back.  SysInfo reads AES global[0] this way when deciding
           whether it is allowed to run. */
        uint32_t fnptr=((uint32_t)a[0]<<16)|a[1];
        uint32_t arg3=frame?rd32((const uint8_t *)frame-48u):0u;
        if(!fnptr)return 0;
        return (int32_t)asf_tos_supexec((void *)(uintptr_t)fnptr,arg3);
    }
    case 0x00feu: {                                                       /* ASF control */
        uint16_t cmd=a[0];
        switch(cmd){
        case 0u: kblock_cache_enable(0u);tos_print("SMARTDRV: CACHE OFF\n");return 0;
        case 1u: kblock_cache_enable(1u);tos_print("SMARTDRV: CACHE ON, ");
                 tos_dec(kblock_cache_slots());tos_print(" sectors\n");return 0;
        case 2u: return (int32_t)kernel_wall_ticks;
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
    tos_print("  cache: ");tos_print(kblock_cache_sectors()?"ON":"OFF");tos_print("\n");
    t0=(uint32_t)kernel_wall_ticks;
    guard=0u;{uint32_t off=0u,w=0u;while(off<total&&guard++<200u){if(kfat_write_path_at(path,off,buf,chunk,&w)!=FAT_FILE_FOUND)break;if(!w)break;off+=w;}}
    t1=(uint32_t)kernel_wall_ticks;wt=t1-t0;
    t0=(uint32_t)kernel_wall_ticks;
    guard=0u;{uint32_t off=0u,g=0u;while(off<total&&guard++<200u){if(kfat_read_path_at(path,off,buf,chunk,&g)!=FAT_FILE_FOUND)break;if(!g)break;off+=g;}}
    t1=(uint32_t)kernel_wall_ticks;rt=t1-t0;
    t0=(uint32_t)kernel_wall_ticks;
    {uint32_t seed=12345u,k;for(k=0u;k<12u;k++){uint32_t o,g;
        seed=seed*1103515245u+12345u;o=((seed>>9)%(total/512u))*512u;
        if(kfat_read_path_at(path,o,buf,512u,&g)!=FAT_FILE_FOUND)break;}}
    t1=(uint32_t)kernel_wall_ticks;st=t1-t0;
    tos_print("  write: ");tos_dec(wt);tos_print(" ticks (");tos_dec(wt*20u);tos_print(" ms) ");
    kbs=wt?((total/1024u)*50u/wt):0u;tos_dec(kbs);tos_print(" KB/s\n");
    tos_print("  read : ");tos_dec(rt);tos_print(" ticks (");tos_dec(rt*20u);tos_print(" ms) ");
    kbs=rt?((total/1024u)*50u/rt):0u;tos_dec(kbs);tos_print(" KB/s\n");
    tos_print("  seek : ");tos_dec(st);tos_print(" ticks/12 reads, avg ");
    tos_dec(st*20u/12u);tos_print(" ms\n");
    kfat_delete_path(path);
    kmem_free(buf,KMEM_OWNER_KERNEL);
}

/* --- AES/VDI (trap #2): LEGACY / EXPERIMENTAL stub ------------------------- */
static uint8_t aes_windows;
static int16_t aes_geom[8][4];
uint16_t tos_aes_trace[64];      /* [0..31]=opcode, [32..63]=intin[0] */
uint32_t tos_aes_trace_n;
/* Full request/response log for the first 64 GEM calls: lets us see exactly
   which AES/VDI operations SysInfo asks for and what we answered.
   Layout per call: [0]=is_aes, [1..6]=contrl[0..5], [7..10]=intin[0..3],
   [11..14]=intout[0..3], [15]=result. */
uint16_t tos_aes_log[64*16];
uint32_t tos_aes_log_n;
/* AES version advertised through appl_init.  TOS v4.x reports 0x0340; we keep
   the EmuTOS "modern but not 3D" value.  SysInfo only needs it to be non-zero,
   but the number also tells applications which AES calls to expect. */
#define TOS_AES_VERSION 0x0330u
/* Command line passed to the running TOS program; shel_read() hands it back. */
static char tos_cmdline[128];
/* The TOS persona runs synchronously inside asf_tos_enter, so the kernel main
   loop (and with it the compositor) is blocked for the whole program.  Pump
   one composite from the app's event loop so its window actually reaches the
   screen while it is alive. */
static uint32_t tos_wm_tick;
/* Deliver a WM_REDRAW to the app's evnt_multi message buffer once, so it
   paints its page instead of waiting forever for a window expose. */
static uint8_t tos_redraw_sent;
/* wind_get(WF_FIRSTXYWH/NEXTXYWH) cursor: one client rectangle then end. */
static uint8_t aes_wg_first;
/* Diagnostics: how many keys/messages evnt_multi actually handed to the app. */
uint32_t tos_evkbd_n,tos_evmes_n,tos_evmulti_n;
/* Synthetic mouse click generator for the AES event loop. */
static uint32_t tos_click_seq;

/* --- minimal AES resource/object support (rsrc_*, form_*, objc_draw) ------ */
#define AES_OBJ_SIZE 24
static int16_t aes_w(const uint8_t *p){return (int16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint16_t aes_u(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}

/* rsrc_obfix(): convert the packed character coordinates of one object
   (ob_x,ob_y,ob_w,ob_h) to pixels, as the AES does after loading a resource. */
static void aes_obfix(uint8_t *tree,int obj)
{
    static const int mul[4]={8,16,8,16};
    uint8_t *o=tree+(uint32_t)(uint16_t)obj*AES_OBJ_SIZE+16u;
    int i;
    for(i=0;i<4;i++){
        uint16_t w=(uint16_t)(((uint16_t)o[i*2]<<8)|o[i*2+1]);
        int coff=(w>>8)&0xff,cpos=w&0xff,r;
        if(i==2&&cpos==80)r=640;else r=cpos*mul[i];
        r+=(coff>128)?(coff-256):coff;
        o[i*2]=(uint8_t)(r>>8);o[i*2+1]=(uint8_t)r;
    }
}

/* Trees must be walked defensively: a cyclic or malformed resource link would
   otherwise recurse forever.  A fixed node budget bounds the work. */
uint32_t aes_dbg_tree,aes_dbg_type;
/* AES opcode histogram: which GEM functions SysInfo actually uses. */
uint32_t tos_aes_opcount[256];
static int aes_box_walk(const uint8_t *tree,int obj,int xo,int yo,
                        int *x0,int *y0,int *x1,int *y1,int first,int *budget)
{
    const uint8_t *o;int x,y,w,h,c;
    if(obj<0||obj>1024||*budget<=0)return first;
    (*budget)--;
    o=tree+(uint32_t)(uint16_t)obj*AES_OBJ_SIZE;
    x=xo+aes_w(o+16);y=yo+aes_w(o+18);w=aes_w(o+20);h=aes_w(o+22);
    if(first){*x0=x;*y0=y;*x1=x+w;*y1=y+h;first=0;}
    else{if(x<*x0)*x0=x;if(y<*y0)*y0=y;if(x+w>*x1)*x1=x+w;if(y+h>*y1)*y1=y+h;}
    c=aes_w(o+2); /* ob_head */
    while(c!=-1&&c<=1024&&*budget>0){
        first=aes_box_walk(tree,c,x,y,x0,y0,x1,y1,first,budget);
        {const uint8_t *n=tree+(uint32_t)(uint16_t)c*AES_OBJ_SIZE;int nc=aes_w(n);
         if(nc==c)break;
         c=nc;}
    }
    return first;
}

static void aes_obj_draw(const uint8_t *tree,int obj,int xo,int yo,int depth,int *budget)
{
    const uint8_t *o;int type,x,y,w,h,c;
    if(obj<0||obj>1024||depth<0||*budget<=0)return;
    (*budget)--;
    o=tree+(uint32_t)(uint16_t)obj*AES_OBJ_SIZE;
    type=(int)aes_u(o+6);
    x=xo+aes_w(o+16);y=yo+aes_w(o+18);w=aes_w(o+20);h=aes_w(o+22);
    switch(type){
    case 20: case 22: case 26: case 30: case 32: /* BOX/BOXTEXT/BUTTON/FBOXTEXT/TITLE */
        if(w>0&&h>0){
            tos_vdi_fill_rect(x,y,w,1,1);
            tos_vdi_fill_rect(x,y+h-1,w,1,1);
            tos_vdi_fill_rect(x,y,1,h,1);
            tos_vdi_fill_rect(x+w-1,y,1,h,1);
        }
        break;
    default:
        /* Unknown object type.  SysInfo's panel root reads as ob_type 0x7114
           instead of G_BOX, and a strict switch silently dropped the panel /
           button frames because of it.  Draw the frame anyway when the type is
           outside the whole GEM range and the rectangle is panel-sized. */
        if((type<20||type>32)&&w>=16&&h>=16){
            tos_vdi_fill_rect(x,y,w,1,1);
            tos_vdi_fill_rect(x,y+h-1,w,1,1);
            tos_vdi_fill_rect(x,y,1,h,1);
            tos_vdi_fill_rect(x+w-1,y,1,h,1);
        }
        break;
    }
    if(type==21||type==22||type==29||type==30){        /* text objects: TEDINFO */
        const uint8_t *td=(const uint8_t *)(uintptr_t)rd32(o+12);
        if(td){const char *s=(const char *)(uintptr_t)rd32(td);if(s)tos_vdi_text(x,y+h-2,s,1,0);}
    }else if(type==26||type==27||type==28||type==32){  /* direct string */
        const char *s=(const char *)(uintptr_t)rd32(o+12);
        if(s)tos_vdi_text(x+2,y+h-2,s,1,0);
    }
    if(depth>0){
        c=aes_w(o+2);
        while(c!=-1&&c<=1024&&*budget>0){
            aes_obj_draw(tree,c,x,y,depth-1,budget);
            {const uint8_t *n=tree+(uint32_t)(uint16_t)c*AES_OBJ_SIZE;int nc=aes_w(n);
             if(nc==c)break;
         c=nc;}
        }
    }
}
static int32_t tos_aes_dispatch(const int16_t *contrl,const int16_t *intin,
                                const int16_t *ptsin,int16_t *intout,int16_t *ptsout,
                                const uint8_t *global,void **addrin,void **addrout)
{
    (void)ptsin;(void)ptsout;(void)addrout;
    /* AES entry points (GEM passes the AES opcode in contrl[0]). */
    switch(contrl[0]){
    case 10:                                    /* appl_init */
        /* GEM fills the AESGLOBAL block through the AESPB global pointer:
           ap_version, ap_count, ap_id, ap_private, ap_ptree, ap_rscmem,
           ap_rsclen, ap_planes, ap_3resv, ap_4resv (30 bytes).  SysInfo reads
           global[0] (ap_version) at startup and, when the AES never set it,
           refuses to run with DON'T PUT SYSINFO IN THE AUTO FOLDER. */
        if(global){
            uint8_t *g=(uint8_t *)(uintptr_t)global;
            int k;
            for(k=0;k<30;k++)g[k]=0u;
            g[0]=(uint8_t)(TOS_AES_VERSION>>8);g[1]=(uint8_t)TOS_AES_VERSION;
            g[3]=1u;                        /* ap_count  = 1 */
            g[5]=1u;                        /* ap_id     = 1 */
            g[21]=1u;                       /* ap_planes = 1 (mono) */
        }
        if(intout){intout[0]=1;intout[1]=(int16_t)TOS_AES_VERSION;intout[2]=0x0300;intout[3]=0x0100;}
        return 1;
    case 19:                                    /* appl_exit */
        return 1;
    case 77:                                    /* graf_handle */
        /* graf_handle -> handle, char w/h, box w/h.  The box metrics (1 and 0
           before) are what GEM uses to size button/border frames. */
        if(intout){intout[0]=1;intout[1]=8;intout[2]=16;intout[3]=11;intout[4]=19;}
        if(ptsout){ptsout[0]=640;ptsout[1]=400;}
        return 1;
    case 78:                                    /* graf_mouse */
        return 1;
    /* Menu: bar/title/item/icheck/tnormal/attach/text. No-op for now but a
       defined answer, so the application does not follow a null pointer. */
    case 30: case 31: case 32: case 33: case 34: case 35: case 36:
        return 1;
    /* Object tree: objc_add/delete/draw/find/offset/order/edit/change/sysvar.
       draw needs the real tree walk; the rest are safe for now. */
    case 40: case 41: case 43: case 44:
    case 45: case 46: case 47: case 48:
        return 1;
    case 20:                                    /* evnt_keybd -> ASCII or 0 */
        return (int32_t)(tos_getkey()&0xffff);
    case 21:                                    /* evnt_button */
    case 22:                                    /* evnt_mouse  */
    case 23:                                    /* evnt_mesag  */
    case 24:                                    /* evnt_timer  */
    case 26:                                    /* evnt_dclick */
        if(intout){unsigned k;for(k=0;k<8u;k++)intout[k]=0;}
        if(ptsout){unsigned k;for(k=0;k<8u;k++)ptsout[k]=0;}
        return 0;
    case 25: {                                  /* evnt_multi: deliver keys */
        /* intout[0]=events, [1..3]=mouse, [4]=shift, [5]=key, [6]=clicks */
        if(intout){unsigned k;for(k=0;k<8u;k++)intout[k]=0;}
        if(ptsout){unsigned k;for(k=0;k<8u;k++)ptsout[k]=0;}
        ++tos_evmulti_n;
        tos_poll_input();
        if(tos_key_ready()){
            int32_t key=tos_getkey();
            uint8_t asc=(uint8_t)(key&0xff);
            uint8_t scn=(uint8_t)((key>>16)&0xff);
            int16_t kw;
            ++tos_evkbd_n;
            /* GEM delivers (scancode<<8)|ascii.  Decoded keys (our input path)
               carry scancode 0, so synthesise one from the ASCII map. */
            if(!scn){int i;for(i=0;i<128;i++){
                if(tos_key_ascii((uint8_t)i,tos_shift,tos_caps)==asc||
                   tos_key_ascii((uint8_t)i,0,0)==asc||
                   tos_key_ascii((uint8_t)i,1,0)==asc){scn=(uint8_t)i;break;}}}
            kw=(int16_t)(((uint16_t)scn<<8)|asc);
            /* Some bindings read the key from int_out[1], the AES 4 layout
               from int_out[5]; fill both so either works. */
            if(intout){intout[0]=0x0001;intout[1]=kw;intout[4]=0;intout[5]=kw;}
            return 0x0001;
        }
        /* Synthetic right-button press for the menu, every 50 evnt_multi. */
        ++tos_click_seq;
        if((tos_click_seq%50u)==0u){
            if(intout){intout[0]=0x0002;intout[1]=320;intout[2]=200;intout[3]=2;intout[4]=0;intout[5]=0;intout[6]=1;}
            return 0x0002;
        }
        /* One synthetic WM_REDRAW so the app paints its page. */
        if(addrin&&addrin[0]&&!tos_redraw_sent){
            int16_t *m=(int16_t *)(uintptr_t)addrin[0];
            /* AES message layout is [type,handle,x,y,w,h]; the old values
               encoded w=0,h=640, so an app clipping to the redraw rectangle
               drew nothing. */
            m[0]=20;      /* WM_REDRAW */
            m[1]=1;       /* window handle returned by wind_create */
            m[2]=0;m[3]=0;m[4]=640;m[5]=400;m[6]=0;m[7]=0;
            tos_redraw_sent=1u;
            ++tos_evmes_n;
            if(intout)intout[0]=0x0010;   /* MU_MESAG */
            return 0x0010;
        }
        return 0;
    }
    case 120:                                   /* shel_read(path, tail) */
        /* GEM shel_read: copy the AES command line and its tail into the two
           addrin buffers.  SysInfo reads it to learn how it was started; an
           empty result makes it believe it was put in the AUTO folder. */
        if(addrin){
            if(addrin[0]){
                char *dst=(char *)(uintptr_t)addrin[0];uint32_t k=0u;
                while(tos_cmdline[k]&&k<127u){dst[k]=tos_cmdline[k];++k;}
                dst[k]=0;
            }
            if(addrin[1])((char *)(uintptr_t)addrin[1])[0]=0;
        }
        if(intout)intout[0]=1;
        return 1;
    case 124:                                   /* shel_find(path) */
        /* Return our own command path so a caller looking for its file does
           not conclude the file is missing. */
        if(addrin&&addrin[0]){
            char *dst=(char *)(uintptr_t)addrin[0];uint32_t k=0u;
            while(tos_cmdline[k]&&k<127u){dst[k]=tos_cmdline[k];++k;}
            dst[k]=0;
        }
        if(intout)intout[0]=1;
        return 1;
    case 100:                                   /* wind_create(kind,x,y,w,h) */
        if(aes_windows<8u){
            unsigned w=aes_windows++;
            aes_geom[w][0]=intin?intin[1]:0;
            aes_geom[w][1]=intin?intin[2]:0;
            aes_geom[w][2]=intin?intin[3]:320;
            aes_geom[w][3]=intin?intin[4]:160;
            if(intout)intout[0]=(int16_t)(w+1u);
            return 1;
        }
        if(intout)intout[0]=0;
        return 0;
    case 101:                                   /* wind_open(handle,x,y,w,h) */
    case 102:                                   /* wind_close */
    case 103:                                   /* wind_delete */
    case 105:                                   /* wind_set */
    case 107:                                   /* wind_update */
        if(intout&&intin)intout[0]=intin[0];
        return 1;
    case 79:                                    /* graf_mkstate -> x,y,buttons,shift */
        if(intout){
            /* Synthesise a slow right-button click so a program waiting for
               the menu button eventually sees one. */
            int16_t b=(int16_t)(((tos_click_seq/50u)&1u)?2:0);
            intout[0]=320;intout[1]=200;intout[2]=b;intout[3]=0;
        }
        return 1;
    case 104: {                                 /* wind_get(handle,field,...) */
        int16_t h;
        int f;
        int16_t wx=8,wy=8,ww=400,wh=294;
        if(!intin||!intout)return 0;
        h=intin[0];f=(int)intin[1];
        if(h>=1&&h<=8){wx=aes_geom[h-1][0];wy=aes_geom[h-1][1];ww=aes_geom[h-1][2];wh=aes_geom[h-1][3];}
        else if(h==0){wx=0;wy=0;ww=640;wh=400;}
        switch(f){
        case 1: intout[0]=0x000f;return 1;          /* WF_KIND */
        case 2: intout[0]=0;intout[1]=0;return 1;   /* WF_NAME */
        case 3: intout[0]=0;return 1;               /* WF_INFO */
        case 8: case 9: intout[0]=0;intout[1]=0;return 1;  /* WF_HSLIDE/VSLIDE */
        case 10: intout[0]=1;return 1;              /* WF_TOP */
        case 11:                                    /* WF_FIRSTXYWH */
            aes_wg_first=1u;
            /* fall through */
        case 12:                                    /* WF_NEXTXYWH */
            if(aes_wg_first){aes_wg_first=0;intout[0]=wx;intout[1]=wy;intout[2]=ww;intout[3]=wh;}
            else {intout[0]=0;intout[1]=0;intout[2]=0;intout[3]=0;}
            return 1;
        default:
            intout[0]=wx;intout[1]=wy;intout[2]=ww;intout[3]=wh;return 1;
        }
    }
    case 42:                                    /* objc_draw(tree,start,depth,clip) */
        if(addrin&&addrin[0]){
            int budget=4096;
            aes_dbg_tree=(uint32_t)(uintptr_t)addrin[0];
            /* Log objc_draw's start/depth so the call arguments are visible. */
            aes_dbg_type=((uint32_t)(intin?(uint16_t)intin[0]:0u)<<16)|
                         (uint32_t)(intin?(uint16_t)intin[1]:0u);
            aes_obj_draw((const uint8_t *)(uintptr_t)addrin[0],
                         intin?(int)(uint16_t)intin[0]:0,0,0,
                         intin?((int)intin[1]):4,&budget);
        }
        return 1;
    case 54:                                    /* form_center(tree) -> x,y,w,h */
        if(addrin&&addrin[0]&&intout){
            int bx=0,by=0,bw=0,bh=0,budget=4096;
            aes_dbg_tree=(uint32_t)(uintptr_t)addrin[0];
            aes_dbg_type=(uint32_t)(uint16_t)(intin?intin[0]:0);
            aes_box_walk((const uint8_t *)(uintptr_t)addrin[0],0,0,0,&bx,&by,&bw,&bh,1,&budget);
            if(bw<1)bw=1;
            if(bh<1)bh=1;
            intout[1]=(int16_t)((640-bw)/2);
            intout[2]=(int16_t)((400-bh)/2);
            intout[3]=(int16_t)bw;
            intout[4]=(int16_t)bh;
        }
        return 1;
    case 51:                                    /* form_dial: no-op (we draw direct) */
    case 52:                                    /* form_alert */
        return 1;
    case 112: {                                 /* rsrc_gaddr(type,index) -> addrout */
        uint32_t ptree=global?rd32(global+10u):0u;
        if(intin&&addrout){
            if(intin[0]==0&&ptree){
                uint32_t v=rd32((const uint8_t *)(uintptr_t)ptree+4u*(uint32_t)(uint16_t)intin[1]);
                addrout[0]=(void *)(uintptr_t)v;
                return v?1:0;
            }
            addrout[0]=(void *)0;
        }
        return 0;
    }
    case 114:                                   /* rsrc_obfix(tree,object) */
        if(addrin&&addrin[0]&&intin)
            aes_obfix((uint8_t *)(uintptr_t)addrin[0],(int)(uint16_t)intin[0]);
        return 1;
    default:
        return 0;
    }
}
int32_t asf_tos_aesvdi(const int16_t *contrl,const int16_t *intin,const int16_t *ptsin,
                       int16_t *intout,int16_t *ptsout,int is_aes,
                       const uint8_t *global,void **addrin,void **addrout)
{
    uint32_t slot;int32_t result;
    tos_clock_tick();
    if(!contrl)return 0;
    {uint32_t t=tos_aes_trace_n++;tos_aes_trace[t&31u]=(uint16_t)contrl[0];tos_aes_trace[32u+(t&31u)]=(uint16_t)(intin?intin[0]:0);}
    ++tos_aes_opcount[contrl[0]&0xffu];
    slot=tos_aes_log_n++;
    if(slot<64u){
        uint16_t *e=tos_aes_log+slot*16u;uint8_t k;
        e[0]=(uint16_t)(is_aes?1u:0u);
        for(k=0u;k<6u;k++)e[1u+k]=(uint16_t)contrl[k];
        for(k=0u;k<4u;k++)e[7u+k]=(uint16_t)(intin?intin[k]:0);
    }
    /* trap #2 selects AES (d0=0xC8) or VDI (d0=0x73); both put the opcode in
       contrl[0].  Opcodes overlap (AES wind_create = VDI v_opnvwk = 100), so
       the selector decides the dispatcher. */
    if(!is_aes)
        result=(int32_t)tos_vdi_call((uint16_t)contrl[0],(int16_t *)contrl,
                                     intin,ptsin,intout,ptsout,addrin,addrout);
    else
        result=tos_aes_dispatch(contrl,intin,ptsin,intout,ptsout,global,addrin,addrout);
    /* evnt_multi is the GUI event loop: while it spins, keep the compositor
       running so the application window is drawn.  ~every 16th call is enough
       to stay smooth without eating the CPU. */
    if(is_aes&&contrl[0]==25&&((tos_aes_log_n&15u)==0u)){
        kernel_wm_damage_all();
        kernel_wm_frame_tick(++tos_wm_tick);
    }
    if(slot<64u){
        uint16_t *e=tos_aes_log+slot*16u;uint8_t k;
        for(k=0u;k<4u;k++)e[11u+k]=(uint16_t)(intout?intout[k]:0);
        e[15]=(uint16_t)result;
    }
    return result;
}

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
/* The T1 tree had a kernel console log; our kernel does not, so the mini
   console window stays empty instead of pulling in code we do not have. */
int32_t kernel_console_log_count(void){return 0;}
const char *kernel_console_log_line(int32_t index){(void)index;return "";}

/* Our kernel has no raw IKBD counters and no TOS argument block yet; the
   personality asks for them from XBIOS. Keep them zero/empty for now. */
int32_t kernel_ikbd_raw_total(void){return 0;}
int32_t kernel_ikbd_drop_count(void){return 0;}
int32_t kernel_ikbd_raw_last(uint32_t back){(void)back;return 0;}
const char *kernel_user_get_arguments(void){return "";}

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
/* ---- TOS TPA -------------------------------------------------------------
   The kernel lives in ST-RAM just above the program area, so the TPA is the
   contiguous free region [0x1000, __image_start).  A program is allocated
   first-fit from the bottom (exactly what EmuTOS alloc_tpa does) and released
   LIFO when the run returns; the kernel heap (kmem) sits above the image. */
extern uint8_t __image_start;
/* The kernel occupies the low ST-RAM, so a TOS program only had a ~520 kB TPA
   ([0x1000, __image_start)).  Real TOS hands out nearly all of RAM; the
   HWAM/PFX self-extractors size their decompression buffer from p_hitpa and
   _memtop, and under-sized geometry makes them unpack only part of the inner
   program (Session 9).  The TF536 has 64 MB TT-RAM; reserve its top 16 MB for
   the TOS TPA so _membot/_memtop/p_hitpa look like a real large-memory TOS. */
#define TOS_TPA_BASE 0x04000000UL
#define TOS_TPA_TOP  0x05000000UL
static uint32_t tos_tpa_next=TOS_TPA_BASE;
static uint32_t tos_tpa_top(void){return TOS_TPA_TOP;}
static void *tos_tpa_alloc(uint32_t bytes)
{
    uint32_t top=tos_tpa_top(),p=(tos_tpa_next+15u)&~15u;
    if(bytes==0u||p<TOS_TPA_BASE||p>top||bytes>top-p)return 0;
    tos_tpa_next=p+bytes;
    return (void *)(uintptr_t)p;
}
/* Sysvars are only 2-byte aligned, so publish them with word stores (68000). */
static void tpa_wr32(uint32_t address,uint32_t value)
{
    __asm__ volatile("move.w %1,%0@" : : "a"((void *)(uintptr_t)address),"d"(value>>16) : "memory");
    __asm__ volatile("move.w %1,%0@" : : "a"((void *)(uintptr_t)(address+2u)),"d"(value&0xffffu) : "memory");
}
static void tos_tpa_publish(void)
{
    tpa_wr32(0x00000432UL,TOS_TPA_BASE);     /* _membot: first free TPA byte */
    tpa_wr32(0x00000436UL,tos_tpa_top());    /* _memtop: end of the TPA      */
}

int32_t kernel_tos_run_program(const char *path)
{
    uint32_t size=0u,tlen,dlen,blen,slen,reloc,total,loaded=0u,i,tpa_mark;
    uint8_t attr=0;
    uint8_t *file,*mem,*text,*data,*bss;
    void *entry;
    int32_t rc;
    int slot;

    if(!path||!*path)return -1;
    {uint32_t k=0u;while(path[k]&&k<127u){tos_cmdline[k]=path[k];++k;}tos_cmdline[k]=0;}
    (void)kernel_tos_bootstrap();
    tos_trace("LD:enter\n");
    if(kfat_stat_path(path,&size,&attr)!=FAT_FILE_FOUND||size<28u)return -2;
    tos_trace("LD:stat\n");
    file=(uint8_t *)kmem_alloc(size,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);
    if(!file)return -3;
    tos_trace("LD:alloc-file\n");
    if(kfat_load_path(path,file,size,&loaded)!=FAT_FILE_FOUND||loaded<28u){
        kmem_free(file,KMEM_OWNER_KERNEL);return -4;
    }
    if((((uint32_t)file[0]<<8)|file[1])!=0x601au){
        kmem_free(file,KMEM_OWNER_KERNEL);return -5;
    }
    tlen=rd32(file+2);dlen=rd32(file+6);blen=rd32(file+10);slen=rd32(file+14);
    if(tlen==0u||tlen>0x70000u||dlen>0x70000u||blen>0x70000u||28u+tlen+dlen>loaded){
        kmem_free(file,KMEM_OWNER_KERNEL);return -6;
    }
    /* TOS gives a normal program the whole remaining TPA: p_hitpa is the TPA
       top and self-extracting packers (PFX/HWAM) decompress downward from it.
       Budgeting only text+data+bss made p_hitpa too low, so the unpacker ran
       past its window and jumped into compressed data. */
    {uint32_t tpa_start=(tos_tpa_next+15u)&~15u,tpa_top=tos_tpa_top(),need=0x100u+tlen+dlen+blen;
     if(tpa_top<=tpa_start||tpa_top-tpa_start<need){kmem_free(file,KMEM_OWNER_KERNEL);return -6;}
     total=tpa_top-tpa_start;}
    tpa_mark=tos_tpa_next;
    mem=(uint8_t *)tos_tpa_alloc(total);      /* EmuTOS alloc_tpa: first free ST-RAM block */
    if(!mem){kmem_free(file,KMEM_OWNER_KERNEL);return -7;}
    tos_tpa_publish();
    tos_trace("LD:alloc-mem\n");
    text=mem+0x100u;data=text+tlen;bss=data+dlen;
    for(i=0u;i<tlen+dlen;++i)text[i]=file[28u+i];
    for(i=0u;i<blen;++i)bss[i]=0u;
    /* GEMDOS relocation: after the symbol table comes a 32-bit start offset,
       then a byte stream of deltas (delta 1 means +0xFE).  Each hit fixes up
       the longword at tbase+offset by adding p_tbase.  The old loader read the
       stream as a list of 32-bit offsets and therefore relocated almost
       nothing, which is why SysInfo jumped into low memory and crashed. */
    reloc=28u+slen+tlen+dlen;
    if(reloc+4u<=loaded){
        uint32_t relst=rd32(file+reloc);
        if(relst!=0u&&relst<tlen+dlen){
            uint32_t flen=tlen+dlen,off=relst;
            const uint8_t *rp=file+reloc+4u,*rpend=file+loaded;
            wr32(text+off,rd32(text+off)+(uint32_t)(uintptr_t)text);
            while(rp<rpend){
                uint8_t d=*rp++;
                if(d==0u)break;
                if(d==1u){off+=0xFEu;continue;}
                off+=d;
                if(off<flen&&!(off&1u))wr32(text+off,rd32(text+off)+(uint32_t)(uintptr_t)text);
            }
        }
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
    wr32(mem+0x24u,(uint32_t)(uintptr_t)mem);       /* p_parent: self (boot) */
    mem[0x28]=0u;mem[0x29]=1u;                      /* p_procid = 1 */
    mem[0x2a]=0u;mem[0x2b]=0u;                      /* p_status */
    mem[0x36]=0u;                                   /* p_flags: TOS domain */
    mem[0x37]=2u;                                   /* p_defdrv: C: (boot drive) */
    {uint32_t k;for(k=0x38u;k<0x60u;++k)mem[k]=0u;} /* p_res3 + p_drvx */
    wr32(mem+0x68u,0xffffffffu);                    /* p_mem: unlimited */
    wr32(mem+0x70u,0u);                             /* p_mflags */
    /* MagiC (MAGIDOS.S): the user stack lives at the end of p_hitpa and
       p_ssp is always stored. Never leave it zero. */
    wr32(mem+0x78u,(uint32_t)(uintptr_t)(mem+total)); /* p_ssp */
    /* p_env must never be NULL: TOS always gives programs a valid environment
       block, and applications read it at startup. An empty block is empty. */
    {uint8_t *env=(uint8_t *)kmem_alloc(4u,2u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
     if(env){env[0]=0u;env[1]=0u;wr32(mem+0x2cu,(uint32_t)(uintptr_t)env);}
     else wr32(mem+0x2cu,0u);}
    {uint32_t k3=0u;const char *args=kernel_user_get_arguments();
     while(path[k3]&&k3<100u){mem[0x81u+k3]=(uint8_t)path[k3];++k3;}
     if(args&&args[0]){mem[0x81u+k3]=' ';++k3;
        {uint32_t j=0u;while(args[j]&&k3<126u){mem[0x81u+k3]=(uint8_t)args[j];++k3;++j;}}}
     mem[0x80]=(uint8_t)k3;mem[0x81u+k3]=0u;}

    if(tos_window_enabled){
        /* T2/T4 window/fullscreen: the compositor may already have supplied a
           surface via kernel_tos_use_surface(); only allocate when it did not. */
        if(!tos_screen){
            tos_screen=(uint8_t *)kmem_alloc(32768u,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
            if(tos_screen){for(i=0u;i<32768u;++i)tos_screen[i]=0u;}
            tos_screen_owned=1u;
        }else tos_screen_owned=0u;
    }else{
        /* Text mode: the program owns the real console screen, exactly like a
         * classic ST TOS program (Physbase returns the live framebuffer). */
        tos_screen=(uint8_t *)(uintptr_t)0x00c00000UL;   /* console on the ET4000 */
        /* Do NOT clear it: this is the SHARED console framebuffer. Zeroing it
           wiped the whole boot log (black screen) whenever a program crashed
           before returning. Let the program draw over it instead. */
        tos_screen_owned=0u;
    }
    tos_mode=2u;
    {int k2;for(k2=0;k2<TOS_MAX_HANDLES;k2++)tos_files[k2].used=0u;}
    tos_dta=0;tos_dir_count=0;tos_dir_index=0;tos_key_pending=-2;
    tos_heap=(uint8_t *)kmem_alloc(TOS_HEAP_SIZE,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    tos_heap_used=0u;tos_heap_init_done=0u;
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
        /* Task 3: keep the load geometry readable from normal context even
           after the program closes. */
        kernel_tos_dbg_basepage=(uint32_t)(uintptr_t)mem;
        kernel_tos_dbg_entry=(uint32_t)(uintptr_t)entry;
        kernel_tos_dbg_total=total;
        /* GEMDOS/TOS: at program entry the basepage is at 4(sp) (EmuTOS
           proc_go sets it up that way).  Packers such as HWAM-SOFT read it
           from the stack, so carry it even though asf_tos_enter also passes
           the basepage in a0. */
        /* EmuTOS proc_go/gouser: SP = p_hitpa - sizeof(gouser_stack); at entry
           0(sp) is a dummy and 4(sp) is the basepage (bdos/proc.c:528-567), and
           a0 = 0 marks a normal program.  Packers read the basepage from the
           stack; PFX decompresses downward from p_hitpa. */
        {uint8_t *usp=(uint8_t *)(uintptr_t)(((uintptr_t)(mem+total-58u))&~1u);
         uint32_t z;for(z=0u;z<54u;z+=4u)wr32(usp+z,0u);
         wr32(usp+0,0u);
         wr32(usp+4,(uint32_t)(uintptr_t)mem);
         asf_tos_enter(mem,entry,usp);}   /* a3=basepage; a0=0 set inside */
        tos_active=0u;
        tos_composite_tick();
        tos_close_window();
        tos_task_free(slot);
        for(k=0u;k<13u;++k)vec[vidx[k]]=tos_saved_vec[k];
    }
    kmem_free(file,KMEM_OWNER_KERNEL);
    if(tos_screen&&tos_screen_owned)kmem_free(tos_screen,KMEM_OWNER_KERNEL);
    if(tos_heap)kmem_free(tos_heap,KMEM_OWNER_KERNEL);
    tos_screen=0;tos_heap=0;
    tos_tpa_next=tpa_mark;                    /* release the program block (LIFO) */
    kblock_cache_clear();
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
    if(kblock_cache_sectors()){
        {kblock_stats_t st;kblock_cache_stats(&st);
         tos_print("DISK CACHE: ");tos_dec(kblock_cache_sectors());tos_print(" sectors, hits=");
         tos_dec(st.hits);tos_print(" misses=");tos_dec(st.misses);
         tos_print(" writebacks=");tos_dec(st.writes);tos_print("\n");}
    }
    rc=tos_exit_code;
    if(rc<0)tos_print("TOS PROGRAM FAULT\n");
    return rc;
}

/* --- link restores (definitions only) --- */
volatile uint32_t kernel_wall_ticks = 0u;
uint32_t kernel_linef_pc = 0u, kernel_linef_op = 0u, kernel_linef_frame[6] = {0,0,0,0,0,0};
