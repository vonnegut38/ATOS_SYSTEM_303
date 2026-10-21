#include "boot_protocol.h"
#include "kernel_memory.h"
#include "tos_async.h"
#include "kernel_audio.h"
#include "kernel_module.h"
#include "kernel_block.h"
#include "kernel_fat.h"
#include "kernel_partition.h"
#include "kernel_vfs.h"
#include "kernel_proc.h"
#include "kernel_timer.h"
#include "kernel_event.h"
#include "kernel_input.h"
#include "kernel_shell.h"
#include "kernel_console.h"
#include "kernel_video.h"
#include "kernel_sessions.h"
#include "kernel_graphics.h"
#include "icons_blob.h"
#include "kernel_surface.h"
#include "kernel_blitter.h"
#include "vofa_et4000.h"

extern void kernel_exception_install(uint32_t cpu_type);
extern int kernel_preemption_enable(void);
extern void kernel_exception_bind(const boot_handoff_t *h);
extern uint8_t __image_memory_end;
extern int32_t kernel_dispatch_ready_user(void);
extern int kernel_dispatch_completed_user(void);
extern int kernel_dispatch_output_user(void);
extern int kernel_user_console_dirty(void);
extern int kernel_user_next_completion(uint32_t *pid,int32_t *status);
extern int kernel_user_ready_pending(void);
extern int kernel_user_terminal_active(void);
extern int kernel_user_terminal_wake(void);
extern int kernel_user_gui_keyboard_active(void);
extern int kernel_user_gui_keyboard_wake(void);
extern int32_t kernel_user_terminal_getchar(void);
extern unsigned kernel_user_last_terminal(void);
extern int32_t kernel_spawn_user_async(const char *,const char *,const char *,uint32_t *);
extern uint32_t kernel_user_service_pid(const char *);
extern int kernel_user_kill(uint32_t pid);
extern void kblock_cache_clear(void);

/* TOS personality hooks (tos_prg.c / tos_vdi.c). */
extern int32_t kernel_tos_run_program(const char *path);
extern void kernel_tos_set_console(void (*puts)(const char *));
extern void tos_composite_work(void);
struct perf_ev { void (*send)(uint32_t,uint32_t,uint32_t); };
extern void kernel_tos_set_trace(void (*puts)(const char *));
extern void kernel_tos_set_cpu(uint8_t cpu_type);
extern void kernel_tos_set_input(int32_t (*pop)(void));
extern void tos_vdi_init(void);
extern void tos_vdi_set_surface(uint8_t *,uint16_t);
extern void kernel_tos_set_console_clear(void (*clear)(void));
extern void kernel_tos_set_video(void (*enter)(void),void (*leave)(void),void (*clear)(uint8_t),
                                 void (*fill)(int,int,int,int,uint8_t),
                                 void (*text)(int,int,const char *,uint8_t,uint8_t),
                                 void (*blit_mono)(int,int,int,int,const uint8_t *,int,uint8_t,uint8_t),
                                 void (*blit_planar)(const uint8_t *,int,int,uint8_t,int,
                                                     uint32_t,int,int,int,int,uint8_t));
extern void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch);
extern uint8_t tos_syshdr[512];
extern uint32_t kernel_linef_pc,kernel_linef_op,kernel_linef_frame[6];
#if !CONFIG_TOS
/* This profile builds no TOS persona.  Keep the two unchanged call sites
   (module API table and syscall 65) linkable and report "unimplemented" so the
   shell's TOS command fails cleanly instead of dragging the persona in. */
int32_t kernel_tos_run_program(const char *path){(void)path;return -0x2ff;}
#endif

/* Compositor and session services (kernel_console.c/kernel_sessions.c/kernel_wm.c). */
extern void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                          void (*number)(uint32_t));
extern void kconsole_clear(void);
extern void kconsole_puts(const char *);
extern void kconsole_put_u32(uint32_t);
extern void kconsole_flush(void);
extern void kconsole_capture_end(void);
extern void kernel_ikbd_poll_capture(void);
extern void kconsole_set_grid(unsigned cols,unsigned rows);
extern void kconsole_seed(const char *cells,unsigned column,unsigned row);
extern unsigned ksessions_current(void);
extern int32_t ksessions_control(uint32_t op,uint32_t value);
extern void ksessions_init(const kernel_shell_service_t *shell,int enabled);
extern void ksessions_set_reset(void (*fn)(void));
extern int  ksessions_take_autostart(void);
uint32_t kdbg_desk_ok,kdbg_desk_fail;
extern void ksessions_pump(void);
extern void ksessions_paint(void);
extern int  kernel_wm_self_test(void);
extern int  kernel_wm_enabled(void);
extern int  kernel_wm_console_visible(void);
extern int  kernel_wm_console_focused(void);
extern int  kernel_wm_console_visible(void);
extern void kernel_wm_console_present(void);
extern void kernel_wm_damage_all(void);
extern void kernel_wm_set_desktop_session(int session);
extern void kernel_wm_frame_tick(uint32_t now);
extern int kvideo_guard_active(void);
extern int kvideo_guard_rollback(uint32_t now);
extern uint32_t kernel_user_clock(void);
extern uint32_t kernel_user_wall_clock(void);
extern void kernel_wm_applet_bar(uint32_t now);
extern void kernel_wm_debug(int *count,int *flags,int *hidden,int *damage);

/* Video output device (kernel_video.c/kernel_video_boot.c). */
extern void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer);
extern int kvideo_module_attach(const kernel_timer_service_t *timer);
extern void kvideo_set_detected(uint8_t adapter_field,uint8_t chip);
extern int kvideo_activate_vofa(volatile uint8_t *vram);
extern void kernel_fault_report(void (*)(const char *),void (*)(uint32_t));
extern uint8_t *kernel_wm_app_surface(uint32_t pid);
extern void kernel_tos_use_surface(uint8_t *surface,uint16_t pitch);
extern void kvideo_flush(void);
extern const void *kvideo_front(void);
extern uint32_t kvideo_dbg_active,kvideo_dbg_calls,kvideo_dbg_copies;

extern void kgfx_gem_desktop_sticky(int on);

#if CONFIG_TOS
/* Low-memory sysvars (TOS ABI): a constant address cast trips
   -Werror=array-bounds, so the stores go through inline asm. */
static void lm_wr32(void *address,uint32_t value)
{
    __asm__ volatile("move.l %1,%0@" : : "a"(address),"d"(value) : "memory");
}
static void lm_wr16(void *address,uint16_t value)
{
    __asm__ volatile("move.w %1,%0@" : : "a"(address),"d"((uint32_t)value) : "memory");
}

/* TOS callbacks used when the personality takes over the screen. */
static void tos_text(int x,int y,const char *text,uint8_t foreground,uint8_t background)
{
    kgfx_text(x,y,text,foreground,background,1);
}
static void tos_fill(int x,int y,int width,int height,uint8_t color)
{
    kgfx_fill_rect(x,y,width,height,color);
}
/* Colour client of the TOS window: expand the program's planar screen into the
   chunky desktop.  The TOS personality owns the screen format (it knows the
   program's mode), so it passes the planes/stride it programmed; the clip
   rectangle keeps a partial (band) update inside the window. */
static void tos_blit_planar(const uint8_t *source,int width,int height,uint8_t bpp,
                            int source_stride,uint32_t source_capacity,
                            int destination_x,int destination_y,
                            int clip_y,int clip_height,uint8_t palette_base)
{
    kgfx_blit_planar(source,(uint16_t)width,(uint16_t)height,bpp,(uint16_t)source_stride,
                     source_capacity,destination_x,destination_y,
                     destination_x,destination_y+clip_y,width,clip_height,palette_base);
}
static void tos_blit_mono(int dx,int dy,int w,int h,const uint8_t *source,int stride,uint8_t ink,uint8_t paper)
{
    kgfx_blit_mono(source,dx,dy,w,h,stride,ink,paper);
}
static void tos_enter_screen(void)
{
    kgfx_gem_desktop_sticky(1);
    kgfx_enter_desktop();
}

/* SETTINGS -> TOS compatibility options, written by the desktop's SAVE
   SETTINGS into \SYSTEM\DESKTOP.CFG (next to RES/THEME, not ATOS.CFG).  Two
   independent keys:
     TOSTA=1  - load TOS.SYS right after START (default off: the compatibility
                loads on the first TOS program instead, which keeps boot short)
     TOSTT=1  - ask for TT RAM when TOS.SYS is loaded
   The address decides the truth about placement: prefer_tt is only a request. */
uint8_t kernel_tos_cfg_at_start;
uint8_t kernel_tos_cfg_prefer_tt;

static int tos_cfg_lookup(const char *buf,uint32_t len,const char *key)
{
    uint32_t i,k;
    for(i=0;i<len;++i){
        if(i&&buf[i-1]!='\n'&&buf[i-1]!='\r')continue;   /* keys start a line */
        for(k=0;key[k]&&i+k<len;++k)if(buf[i+k]!=key[k])break;
        if(key[k])continue;
        if(i+k<len&&buf[i+k]=='=')return (i+k+1u<len&&buf[i+k+1u]=='1')?1:0;
    }
    return -1;
}
static void tos_cfg_read(void)
{
    /* Big enough for what desktop_save() can write: RES + THEME + TOSTA +
       TOSTT + one line per icon, up to ~365 B.  The old 192-byte buffer made
       kfat_load_system_file() reject the WHOLE file (capacity < file_size), so
       once the desktop saved a full icon list both TOS options silently fell
       back to their defaults. */
    static char buf[512];
    /* The desktop persists these in \SYSTEM\DESKTOP.CFG alongside RES/THEME. */
    static const char name[11]={'D','E','S','K','T','O','P',' ','C','F','G'};
    static const char key_start[]="TOSTA",key_tt[]="TOSTT";
    uint32_t size=0;int v,state;
    kernel_tos_cfg_at_start=0u;kernel_tos_cfg_prefer_tt=0u;
    state=(int)kfat_load_system_file(name,buf,sizeof(buf)-1u,&size);
    if(state!=FAT_FILE_FOUND){
        /* Not silent any more: a rejected/oversized file disables the options. */
        if(state!=FAT_FILE_NOT_FOUND)kconsole_notice_puts("DESKTOP.CFG: UNREADABLE - TOS OPTIONS DEFAULT\n");
        return;
    }
    if(size>=sizeof(buf))size=sizeof(buf)-1u;
    v=tos_cfg_lookup(buf,size,key_start);if(v>=0)kernel_tos_cfg_at_start=(uint8_t)v;
    v=tos_cfg_lookup(buf,size,key_tt);if(v>=0)kernel_tos_cfg_prefer_tt=(uint8_t)v;
}
#endif

typedef struct {const char *path,*name;uint32_t timeout_ticks;} startup_service_t;

static boot_handoff_t runtime_handoff;
/* Boot phase timing on the guest's own 50 Hz clock (diagnostics). */
static uint32_t boot_phase_ticks[4];

/* --- Boot splash ----------------------------------------------------------
   The kernel owns the screen for several seconds while the services come up and
   the text console is not rendered in that window, so the display was simply
   black.  Paint the machine's identity and its real hardware there instead, with
   a progress bar.  The desktop repaints the whole screen when it takes over, so
   nothing here has to be undone.  No libc: numbers are formatted by hand. */
static void (*rom_boot_progress)(uint16_t,const char *);
/* Desktop process supervision.  The desktop used to be spawned with its result
   discarded and its later death unreported: a crash at start-up left the boot
   board on screen (quiet mode hid the completion line) and the machine looked
   hung.  Both events now end up on a visible text console. */
static uint32_t kernel_desktop_pid;

/* Dostep dla innych czesci jadra (np. epilog programu TOS musi kazac pulpitowi
   przemalowac ekran po zabiciu programu). */
uint32_t kernel_desktop_active_pid(void)
{
    return kernel_desktop_pid;   /* 0 takze po zatrzymaniu pulpitu */
}
static uint8_t kernel_desktop_stopped;
extern int kernel_user_take_completion(uint32_t pid,int32_t *status);
static void kernel_desktop_report(int32_t status,int spawn_failed)
{
    kernel_desktop_stopped=1u;
    kconsole_boot_screen(0);
    kconsole_quiet(0);                       /* diagnostics are visible again */
    if(kgfx_desktop_active())(void)kgfx_restore_console();
    /* Cover the boot board: kgfx_clear() writes the application target, which is
       not necessarily the frame that is on screen (the board stayed visible).
       Paint the active frame explicitly before showing the diagnostic. */
    kgfx_fill_rect(0,0,(int32_t)kvideo_mode_width(),(int32_t)kvideo_mode_height(),0);
    kvideo_flush();
    kconsole_clear();
    kconsole_puts("\n");
    if(spawn_failed){
        kconsole_puts("DESKTOP.APP DID NOT START (code ");
        kconsole_put_u32((uint32_t)(-status));
        kconsole_puts(")");
    }else{
        kconsole_puts(status<0?"DESKTOP.APP FAILED":"DESKTOP.APP STOPPED");
        if(kernel_desktop_pid){kconsole_puts(" [PID ");kconsole_put_u32(kernel_desktop_pid);kconsole_puts("]");}
        if(status==-3)kconsole_puts(" KILLED");
        else if(status==-4)kconsole_puts(" WATCHDOG");
        else if(status<0){kconsole_puts(" FAULT ");kconsole_put_u32((uint32_t)-status);}
        else if(status>0){kconsole_puts(" EXIT ");kconsole_put_u32((uint32_t)status);}
    }
    kconsole_puts("\nTEXT SHELL ACTIVE - THE DESKTOP CAN BE STARTED AGAIN\n");
    kconsole_flush();
}
/* Keep the ROM's screen and renderer until the desktop takes ownership.
   No duplicate logo renderer, resolution change or full-frame redraw here. */
static void boot_show(uint16_t percent,const char *label)
{
    if(rom_boot_progress)rom_boot_progress(percent,label);
}

static void kernel_startup_services(const boot_handoff_t *h,
                                    const kernel_timer_service_t *timer)
{
    static const startup_service_t table[]={
        {"\\SYSTEM\\INIT.APP","SYSTEM.INIT",250u},
        {"\\SYSTEM\\GFX.APP","SYSTEM.GFX",250u},
        {"\\SYSTEM\\GUI.APP","SYSTEM.GUI",250u}
    };
    uint8_t i;uint32_t pid,start,now;int32_t result;
    boot_show(85u,"STARTUP SERVICES");
    h->console_puts("STARTUP SERVICES: ");
    for(i=0;i<(uint8_t)(sizeof(table)/sizeof(table[0]));++i){
        if(i)h->console_puts("                  ");
        boot_show((uint16_t)(85u+i*3u),table[i].name);
        pid=0;result=kernel_spawn_user_async(table[i].path,"","\\",&pid);
        if(result<0||!pid){h->console_puts(table[i].name);h->console_puts(" LOAD FAILED\n");continue;}
        timer->poll();start=timer->ticks();
        for(;;){
            if(kernel_user_service_pid(table[i].name)==pid){
                h->console_puts(table[i].name);h->console_puts(" READY\n");
                boot_show((uint16_t)(88u+i*3u),table[i].name);break;}
            kernel_dispatch_ready_user();timer->poll();now=timer->ticks();
            if((uint32_t)(now-start)>=table[i].timeout_ticks){
                (void)kernel_user_kill(pid);h->console_puts(table[i].name);
                h->console_puts(" TIMEOUT - DISABLED\n");break;}
        }
    }
}

static void kernel_print_completion(const boot_handoff_t *h,uint32_t pid,int32_t status)
{
    h->console_puts("[PID ");h->console_put_u32(pid);
    if(status==-3)h->console_puts(" KILLED");
    else if(status==-4)h->console_puts(" WATCHDOG");
    else if(status<0){h->console_puts(" FAULT ");h->console_put_u32((uint32_t)-status);}
    else{h->console_puts(" EXIT ");h->console_put_u32((uint32_t)status);}
    h->console_puts("]");
}

static void kernel_notify_pending(const boot_handoff_t *h,const kernel_shell_service_t *shell)
{
    uint32_t pid;int32_t status;int first=1,had_output;
    if(kernel_user_terminal_active()||kernel_user_gui_keyboard_active()||shell->line_active()||
       !kernel_user_next_completion(&pid,&status))return;
    had_output=kernel_user_console_dirty();
    do{if(!had_output||!first)h->console_puts("\n");first=0;
       kernel_print_completion(h,pid,status);}
    while(kernel_user_next_completion(&pid,&status));
    shell->redraw_prompt();
}

static int kernel_handle_dispatch_completion(const boot_handoff_t *h,
                                               const kernel_shell_service_t *shell)
{
    uint32_t pid;int32_t status;int first=1,had_output;
    if(!kernel_dispatch_completed_user()||kernel_user_terminal_active()||
       kernel_user_gui_keyboard_active())return 0;
    if(kernel_user_last_terminal()!=ksessions_current())return 0;
    if(shell->line_active()){
        /* Program output may have advanced the console, but completion text
           must remain queued until the user finishes the current line. */
        shell->redraw_prompt();
        return 1;
    }
    had_output=kernel_user_console_dirty();
    while(kernel_user_next_completion(&pid,&status)){
        if(!had_output||!first)h->console_puts("\n");
        first=0;kernel_print_completion(h,pid,status);
    }
    /* kernel_notify_pending already drained and redrew when it printed the
       completion; only redraw here if this path actually printed one. */
    if(!first&&!kernel_user_ready_pending())shell->redraw_prompt();
    return 1;
}

/* ---- Quiet bring-up reporting ------------------------------------------
   The single "LOADING COMPONENTS:" status row shows only the current component
   name; a failure leaves that row and prints one real line (no scrolling,
   messages only on error - the user's request). */
static const char *module_fail_reason(enum module_state st)
{
    if(st==MODULE_NOT_FOUND)return "NOT FOUND";
    if(st==MODULE_NO_MEMORY)return "NO MEMORY";
    if(st==MODULE_IO_FAILED)return "I/O FAILED";
    if(st==MODULE_INIT_FAILED)return "INIT FAILED";
    if(st==MODULE_INVALID)return "INVALID FILE";
    return "FAILED";
}
static const char *fat_fail_reason(enum fat_state st)
{
    if(st==FAT_PARTITION_INVALID)return "PARTITION INVALID";
    if(st==FAT_IO_FAILED)return "I/O FAILED";
    if(st==FAT_BPB_INVALID)return "BPB INVALID";
    if(st==FAT_UNSUPPORTED)return "UNSUPPORTED";
    return "NOT FAT16";
}
static void boot_fail(const char *name,const char *why)
{
    kconsole_status_row(-1);
    kconsole_notice_puts("\n");   /* start a fresh line, clear of the status row */
    kconsole_notice_puts(name);
    kconsole_notice_puts(": ");
    kconsole_notice_puts(why);
    kconsole_notice_puts("\n");
    kconsole_status_here();
    kconsole_puts("LOADING COMPONENTS:");
}

static unsigned boot_completed;
static void boot_loading(const char *label)
{boot_show((uint16_t)(45u+boot_completed*35u/11u),label);}
static void boot_loaded(const char *label)
{
    kconsole_puts(label);
    if(boot_completed<11u)++boot_completed;
    boot_show((uint16_t)(45u+boot_completed*35u/11u),label);
}

void kernel_main(const boot_handoff_t *h)
{
    enum block_state block_state;
    uint8_t i;
    int fat_mounted=0;
    int proc_ready=0;
    const kernel_proc_service_t *proc_service=0;
    const kernel_timer_service_t *timer_service=0;
    const kernel_input_service_t *input_service=0;
    int timer_ready=0,input_ready=0,shell_ready=0;
    if(!h||h->magic!=BOOT_HANDOFF_MAGIC||h->version!=BOOT_HANDOFF_VERSION||h->size<sizeof(*h)||!h->console_clear||!h->console_puts)
        for(;;)__asm__ volatile("stop #0x2700");
    kernel_exception_bind(h);
    if(h->cpu_type>1u)__asm__ volatile("moveq #0,%d0\n\t.long 0x4e7b0801");
    kernel_exception_install(h->cpu_type);
    /* No banner or system table: the ROM POST already lists CPU/RAM, and the
       single LOADING COMPONENTS status line carries the rest. Only failures
       print a full line (the user's request). */
    if(!kmem_init(h->st_ram_bytes,h->tt_ram_bytes,(uint32_t)(uintptr_t)&__image_memory_end)){h->console_puts("MEMORY MANAGER: FAILED\n");for(;;)__asm__ volatile("stop #0x2700");}
    /* TOS disables the blitter when an 030 is present, and the TF536 community
       removes the chip outright: an accelerator replaces the 68000, so the
       blitter's fixed ST-RAM bus cycles cannot be trusted at accelerator
       timings (the PAK manual also notes a 33 MHz 030 beats the blitter).  Keep
       the kernel's functional self-test as the second line of defence.  cpu_type
       2 = 68020, 3 = 68030, 4 = 68040/060. */
    {uint8_t blitter_ok=h->blitter_enabled;
     if(h->cpu_type>=2u)blitter_ok=0u;
    if(!kgfx_init(2u,h->blitter_present,blitter_ok)||!kgfx_self_test()||
       kgfx_get_service()->magic!=KGFX_SERVICE_MAGIC)h->console_puts("GRAPHICS SELF-TEST FAILED\n");}
    {const kgfx_info_t *gfx=kgfx_get_info();int surface_ok;
        ksurface_init(gfx->width,gfx->height);surface_ok=ksurface_self_test();
        ksurface_init(gfx->width,gfx->height);
        if(!surface_ok)h->console_puts("SURFACE SELF-TEST FAILED\n");}
    (void)kmem_self_test();
    kgfx_set_palette(0,0x0000u);
    kgfx_set_palette(1,0x0fffu);
    kconsole_init(1,h->console_clear,h->console_puts,h->console_put_u32);
    kconsole_boot_screen(1);
    kvideo_set_flush_gate(kconsole_boot_active);
    kconsole_set_grid(80u,25u);
    {static char boot_cells[80u*25u];uint16_t bc=0u,br=0u;
     if(h->console_text_copy){h->console_text_copy(boot_cells,&bc,&br);kconsole_seed(boot_cells,bc,br);}}
    /* The ROM already owns the card: aim the kernel console at its VRAM now so
       every later render lands there instead of the (invisible) Shifter. */
    if(h->nova_initialized&&!h->video_force_shifter){
        int act=kvideo_activate_vofa((volatile uint8_t *)(uintptr_t)VOFA_MEM);
        /* Measured: without this the kernel stayed at mode=2/planes=1 with
           framebuffer=0x10000 (ST-RAM) while the card scanned 0xC00000, so every
           kernel-console line was painted where nobody could see it.  Adopt the
           card's geometry AND aim the drawing target at its VRAM. */
        if(act){
            /* The CARD mode comes from BIOS.CFG (handoff.card_video_mode);
               h->video_mode is the Shifter's - which is why the kernel used to
               stay in mono over an 800x600x256 scanout. */
            if(h->card_video_mode==3u){kvideo_set_mode_format(800u,600u,8u);kgfx_set_depth(800u,600u,8u);}
            else if(h->card_video_mode==4u){kvideo_set_mode_format(800u,600u,1u);kgfx_set_depth(800u,600u,1u);}
            else if(h->card_video_mode==5u){kvideo_set_mode_format(1024u,768u,1u);kgfx_set_depth(1024u,768u,1u);}
            (void)kgfx_set_target((uint8_t *)(uintptr_t)VOFA_MEM);
            kvideo_select((const volatile uint8_t *)(uintptr_t)VOFA_MEM);
            kgfx_palette_defaults();   /* the RAMDAC is reachable only now */
        }
    }
    {uint32_t k;for(k=0;k<sizeof(runtime_handoff);++k)((uint8_t *)&runtime_handoff)[k]=((const uint8_t *)h)[k];}
    runtime_handoff.console_clear=kconsole_clear;
    runtime_handoff.console_puts=kconsole_puts;
    runtime_handoff.console_put_u32=kconsole_put_u32;
    boot_phase_ticks[0]=timer_service?timer_service->ticks():0;
    /* Quiet bring-up: only the LOADING COMPONENTS status line and errors draw.
       Clear the scrolled ROM log and write from the very top - no gap. */
    kconsole_quiet(1);
    kernel_exception_bind(&runtime_handoff);
    ksessions_set_reset(runtime_handoff.reset);
    boot_completed=0u;
    rom_boot_progress=h->boot_progress;
    boot_show(45u,"LOADING COMPONENTS");
    kconsole_status_row(22);
    kconsole_puts("LOADING COMPONENTS:");

    {boot_loading("ACSI.SYS");kernel_load_acsi(&runtime_handoff);boot_loaded("ACSI.SYS");}   /* any failure falls back to the BIOS driver - not an error */
    boot_loading("DISK");
    block_state=kblock_init(runtime_handoff.boot_acsi_id);if(block_state==BLOCK_READY)boot_loaded("DISK");else boot_fail("DISK",block_state==BLOCK_NO_MEMORY?"NO MEMORY":"I/O FAILED");
    if(block_state==BLOCK_READY){
        boot_loading("VOLUMES");
        enum partition_state ps=kpart_scan(runtime_handoff.boot_acsi_id,runtime_handoff.boot_partition,runtime_handoff.acsi_devices,runtime_handoff.ide_units);
        if(ps==KPART_READY)boot_loaded("VOLUMES");
        else boot_fail("VOLUMES",ps==KPART_IO_FAILED?"I/O FAILED":"INVALID TABLE");
        /* The transport must be selected BEFORE the mount: a failed select
           leaves the block layer not-ready, and the mount would then report a
           generic I/O error instead of the real cause. */
        {boot_loading("FAT");enum fat_state state;
         if(!kblock_select(kpart_boot_device()))state=FAT_IO_FAILED;
         else state=kfat_mount(kpart_boot_start(),kpart_boot_id());
         {unsigned s;for(s=1;s<SESSION_COUNT;s++){(void)kfat_select_session(s);
          if(kblock_select(kpart_boot_device()))(void)kfat_mount(kpart_boot_start(),kpart_boot_id());}
          (void)kfat_select_session(0);}
         if(state==FAT_MOUNTED){fat_mounted=1;boot_loaded("FAT");}
         else boot_fail("FAT",fat_fail_reason(state));}
    }else{
        {enum fat_state fst=kfat_mount(0u,"FDD");
         if(fst==FAT_MOUNTED){fat_mounted=1;boot_loaded("FAT");}
         else boot_fail("FAT","FLOPPY UNAVAILABLE");}
    }
    if(fat_mounted){
        /* A non-clean volume is the one health warning that must stay visible. */
        if(kfat_health()==FAT_HEALTH_DIRTY)boot_fail("FAT","DIRTY - READ ONLY");
        else if(kfat_health()==FAT_HEALTH_HARD_ERROR)boot_fail("FAT","HARD ERROR - READ ONLY");
        else if(kfat_health()!=FAT_HEALTH_CLEAN)boot_fail("FAT","UNKNOWN");
        {boot_loading("CORE.SYS");static const char core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(core_name,0x0101u,1);
         if(state==MODULE_READY&&kernel_find_service(KERNEL_SERVICE_CORE,1u))boot_loaded("CORE.SYS");
         else boot_fail("CORE.SYS",state==MODULE_READY?"SERVICE MISSING":module_fail_reason(state));}
    }
    if(fat_mounted){boot_loading("VFS.SYS");static const char vfs_name[11]={'V','F','S',' ',' ',' ',' ',' ','S','Y','S'},core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};uint32_t size=0;enum module_state state=kernel_load_system_module(vfs_name,0x0102u,1);const kernel_vfs_service_t *vfs=(const kernel_vfs_service_t *)kernel_find_service(KERNEL_SERVICE_VFS,1u);if(state==MODULE_READY&&vfs&&vfs->magic==KERNEL_VFS_MAGIC&&vfs->version>=KERNEL_VFS_VERSION&&vfs->size>=sizeof(*vfs)&&vfs->stat_system&&vfs->stat_system(core_name,&size)&&size>=BOOT_IMAGE_HEADER_SIZE)boot_loaded("VFS.SYS");else boot_fail("VFS.SYS",state==MODULE_READY?"SELF-TEST FAILED":module_fail_reason(state));}
    if(fat_mounted){boot_loading("PROC.SYS");static const char proc_name[11]={'P','R','O','C',' ',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(proc_name,0x0103u,1);proc_service=(const kernel_proc_service_t *)kernel_find_service(KERNEL_SERVICE_PROC,KERNEL_PROC_VERSION);proc_ready=state==MODULE_READY&&proc_service&&proc_service->magic==KERNEL_PROC_MAGIC&&proc_service->version>=KERNEL_PROC_VERSION&&proc_service->size>=sizeof(*proc_service)&&proc_service->task_count&&proc_service->cooperate&&proc_service->dispatch_count&&proc_service->sleep_ticks&&proc_service->timer_tick&&proc_service->sleep_remaining&&proc_service->context_self_test&&(proc_service->capabilities&KERNEL_PROC_CAP_FULL_CONTEXT)&&proc_service->task_count()==2u&&proc_service->cooperate()&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==2u&&proc_service->dispatch_count(1u)==2u&&proc_service->context_self_test();if(proc_ready)boot_loaded("PROC.SYS");else boot_fail("PROC.SYS",state==MODULE_READY?"SELF-TEST FAILED":module_fail_reason(state));}
    if(fat_mounted){boot_loading("TIMER.SYS");static const char timer_name[11]={'T','I','M','E','R',' ',' ',' ','S','Y','S'};uint32_t before,guard,quantum;enum module_state state=kernel_load_system_module(timer_name,0x0104u,1);timer_service=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);timer_ready=proc_ready&&state==MODULE_READY&&timer_service&&timer_service->magic==KERNEL_TIMER_MAGIC&&timer_service->version>=KERNEL_TIMER_VERSION&&timer_service->size>=sizeof(*timer_service)&&timer_service->poll&&timer_service->ticks&&timer_service->switch_pending&&timer_service->acknowledge_switch&&timer_service->frequency_hz==50u&&proc_service->sleep_ticks(0u,2u)&&proc_service->sleep_remaining(0u)==2u;before=timer_ready?timer_service->ticks():0;for(quantum=0;timer_ready&&quantum<4u;++quantum){guard=2000000UL;while(guard--&&!timer_service->switch_pending())timer_service->poll();if(!timer_service->switch_pending()){timer_ready=0;break;}timer_service->acknowledge_switch();proc_service->timer_tick();if(timer_service->switch_pending()||!proc_service->cooperate())timer_ready=0;}if(timer_ready)timer_ready=timer_service->ticks()>=before+4u&&proc_service->sleep_remaining(0u)==0u&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==6u;if(timer_ready)boot_loaded("TIMER.SYS");else boot_fail("TIMER.SYS",state==MODULE_READY?"SELF-TEST FAILED":module_fail_reason(state));}
    if(fat_mounted){boot_loading("EVENT.SYS");static const char event_name[11]={'E','V','E','N','T',' ',' ',' ','S','Y','S'};uint32_t type=0,value=0;enum module_state state=kernel_load_system_module(event_name,0x0105u,1);const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);int ok=proc_ready&&state==MODULE_READY&&event&&event->magic==KERNEL_EVENT_MAGIC&&event->version>=KERNEL_EVENT_VERSION&&event->size>=sizeof(*event)&&event->receive&&event->send&&event->pending&&!event->receive(0u,&type,&value)&&proc_service->task_state(0u)==KERNEL_PROC_WAITING&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==7u&&event->send(0u,0x1234u,0x41535953UL)&&event->pending(0u)&&proc_service->task_state(0u)==KERNEL_PROC_READY&&event->receive(0u,&type,&value)&&!event->pending(0u)&&type==0x1234u&&value==0x41535953UL&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==6u&&proc_service->dispatch_count(1u)==8u;if(ok)boot_loaded("EVENT.SYS");else boot_fail("EVENT.SYS",state==MODULE_READY?"SELF-TEST FAILED":module_fail_reason(state));}
    if(fat_mounted){boot_loading("INPUT.SYS");static const char input_name[11]={'I','N','P','U','T',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(input_name,0x0106u,1);input_service=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);input_ready=state==MODULE_READY&&input_service&&input_service->magic==KERNEL_INPUT_MAGIC&&input_service->version>=KERNEL_INPUT_VERSION&&input_service->size>=sizeof(*input_service)&&input_service->poll&&input_service->classify&&input_service->dropped_events&&input_service->classify(0x1eu)==KERNEL_INPUT_KEY_DOWN&&input_service->classify(0x9eu)==KERNEL_INPUT_KEY_UP&&input_service->classify(0xf8u)==0u&&input_service->dropped_events()==0u;if(input_ready)boot_loaded("INPUT.SYS");else boot_fail("INPUT.SYS",state==MODULE_READY?"SELF-TEST FAILED":module_fail_reason(state));}
    if(fat_mounted){boot_loading("SHELL.SYS");static const char shell_name[11]={'S','H','E','L','L',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(shell_name,0x0107u,1);const kernel_shell_service_t *shell=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);shell_ready=state==MODULE_READY&&timer_ready&&input_ready&&shell&&shell->magic==KERNEL_SHELL_MAGIC&&shell->version>=KERNEL_SHELL_VERSION&&shell->size>=sizeof(*shell)&&shell->start&&shell->pump&&shell->key_count&&shell->redraw_prompt&&shell->line_active&&shell->select_session&&shell->key_count()==0u;if(shell_ready)boot_loaded("SHELL.SYS");else boot_fail("SHELL.SYS",state!=MODULE_READY?module_fail_reason(state):"UNAVAILABLE");
    /* Rozmiar ikon wybrany w SETTINGS (\SYSTEM\ICONSZ.CFG: "32"/"48"/"64").
       Czytamy PRZED wczytaniem ICONS.DAT, zeby jadro rozwineło od razu wlasciwa
       sekcje formatu v5 - inaczej trzeba by dekodowac dwa razy.  Brak pliku
       albo smiec = rozmiar domyslny (48), czyli dotychczasowy wyglad. */
    if(fat_mounted){
        static const char iconsz_path[]="\\SYSTEM\\ICONSZ.CFG";
        uint32_t iconsz_size=0,iconsz_got=0;uint8_t iconsz_attr=0;char iconsz[8];
        if(kfat_stat_path(iconsz_path,&iconsz_size,&iconsz_attr)==FAT_FILE_FOUND&&
           !(iconsz_attr&0x10u)&&iconsz_size<sizeof iconsz&&
           kfat_load_path(iconsz_path,iconsz,sizeof iconsz,&iconsz_got)==FAT_FILE_FOUND&&
           iconsz_got){
            unsigned px=0,i2=0,mode=9u;
            while(i2<iconsz_got&&iconsz[i2]>='0'&&iconsz[i2]<='9'){px=px*10u+(unsigned)(iconsz[i2]-'0');++i2;}
            while(i2<iconsz_got&&iconsz[i2]!='\n')++i2;      /* do konca linii */
            if(i2<iconsz_got)++i2;
            if(i2<iconsz_got&&iconsz[i2]>='0'&&iconsz[i2]<='9')mode=(unsigned)(iconsz[i2]-'0');
            if(px==32u||px==48u||px==64u)kgfx_icons_request((uint16_t)px);
            if(mode<=2u)kgfx_icon_grid_set((uint8_t)mode);
        }
    }
    /* Ikony pulpitu: 255 KB danych z jadra przeniesione do \SYSTEM\ICONS.DAT,
       wczytywane do TT-RAM (w ST-RAM jest tylko 4 MB).  Brak pliku nie zatrzymuje
       startu - pulpit rysuje sie wtedy bez ikon.  Stan ladownia trafia do
       \SYSTEM\ICONLOG.TXT, bo bledy startu znikaja z ekranu po pierwszym flushu. */
    if(fat_mounted&&!kgfx_icons_loaded()){
        static const char icons_path[]="\\SYSTEM\\ICONS.DAT";
        uint32_t icons_size=0;uint8_t icons_attr=0;int icons_state=0;uint32_t icons_got=0;
        if(kfat_stat_path(icons_path,&icons_size,&icons_attr)!=FAT_FILE_FOUND||(icons_attr&0x10u)){
            icons_state=1;                                   /* brak pliku */
        }else if(icons_size<sizeof(icons_blob_header_t)||icons_size>0x100000u){
            icons_state=2;                                   /* zly rozmiar */
        }else{
            uint8_t *icons_blob=(uint8_t *)kmem_alloc(icons_size,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
            if(!icons_blob)icons_state=3;                    /* brak pamieci */
            else if(kfat_load_path(icons_path,icons_blob,icons_size,&icons_got)!=FAT_FILE_FOUND||icons_got!=icons_size)icons_state=4;
            else if(!kgfx_icons_load(icons_blob,icons_size)){icons_state=5;}   /* zly format */
            else {
                icons_state=6;boot_loaded("ICONS.DAT");
                /* Drugi cache: 16 px dla paska na dole (ten sam blob, osobne
                   bufory).  Brak sekcji 16 px w starszym formacie nie jest
                   bledem - pasek po prostu nie bedzie mial malych ikon. */
                {extern int kgfx_icons_small_prepare(const void *,uint32_t);
                 (void)kgfx_icons_small_prepare(icons_blob,icons_size);}
                /* Wersje 2/3 sa spakowane: loader rozwinal dane do WLASNEGO
                   bufora (staly rozmiar ~255 KB), wiec bufor PLIKU jest juz
                   niepotrzebny.  Wersja 1 rysuje WPROST z tego bufora, wiec
                   tylko dla niej nie wolno go zwalniac.  Bez tego przepadal
                   caly plik (107 KB) w TT-RAM obok rozwinietego bloba. */
                {uint32_t ver=((uint32_t)icons_blob[4]<<24)|((uint32_t)icons_blob[5]<<16)|
                              ((uint32_t)icons_blob[6]<<8)|(uint32_t)icons_blob[7];
                 if(ver>=2u)(void)kmem_free(icons_blob,KMEM_OWNER_KERNEL);}
            }
        }
        if(icons_state!=6){
            /* Bez snprintf (jadro go nie ma): stan + rozmiar w hex. */
            char icons_log[40];int k=0,sh;
            {static const char hex[]="0123456789ABCDEF";
             icons_log[k++]='S';icons_log[k++]=(char)('0'+(icons_state%10));
             icons_log[k++]=' ';icons_log[k++]='0';icons_log[k++]='x';
             for(sh=28;sh>=0;sh-=4)icons_log[k++]=hex[(icons_size>>sh)&15u];
             icons_log[k++]='\n';}
            (void)kfat_save_path("\\SYSTEM\\ICONLOG.TXT",icons_log,(uint32_t)k);
            boot_fail("ICONS.DAT",icons_state==1?"NOT FOUND":(icons_state==2?"BAD SIZE":
                     (icons_state==3?"NO MEMORY":(icons_state==4?"READ FAILED":"BAD FORMAT"))));
        }
    }
    boot_phase_ticks[1]=timer_service?timer_service->ticks():0;
    kconsole_flush();     /* the status row is short: flush it instead of batching */
    kconsole_status_row(-1);          /* back to normal appending after the bring-up */
    if(shell_ready){
        /* Reaching the shell means the video path works, so the ROM's persistent
           failover latch must be cleared - and it must be cleared in BOTH cases.
           The latch makes the ROM skip the adapter on the NEXT boot, so if it
           were only cleared when the ROM itself brought the card up, a single
           failed card boot would latch the machine into "Shifter only" forever:
           the ROM never retries the card, nova_initialized stays clear, and the
           condition that would clear the latch is never met again. That is
           exactly what left the boot screen black while the desktop (which
           programs the card itself) worked. */
        if(runtime_handoff.video_boot_ok)
            runtime_handoff.video_boot_ok();
        /* Tell the video layer which card the BIOS found, so it can load the
           matching driver from \SYSTEM\DRV instead of assuming ET4000. */
        boot_show(82u,"VIDEO DRIVER");
        kvideo_set_detected(runtime_handoff.reserved0,runtime_handoff.reserved1);
        if(runtime_handoff.video_force_shifter)
            runtime_handoff.console_puts("VIDEO: SHIFTER (FORCED - ALT OR SETUP)\n");
        else if(runtime_handoff.nova_initialized){
            /* Report the mode the card is really in (BIOS.CFG drives it). */
            if(h->card_video_mode==3u)runtime_handoff.console_puts("VIDEO: VOFA (ROM INIT) / 800x600 256 COLOURS\n");
            else if(h->card_video_mode==4u)runtime_handoff.console_puts("VIDEO: VOFA (ROM INIT) / 800x600 MONO\n");
            else if(h->card_video_mode==5u)runtime_handoff.console_puts("VIDEO: VOFA (ROM INIT) / 1024x768 MONO\n");
            else runtime_handoff.console_puts("VIDEO: VOFA (ROM INIT) / 640x400 MONO\n");
            /* The card is already scanning; publish VOFA.SYS so the OPTIONS
               menu can switch 800x600 / 1024x768 live. */
            (void)kvideo_module_attach(timer_service);
            /* The VOFA service exists only now, so this is the first moment the
               card's RAMDAC can receive the system palette. */
            kgfx_palette_defaults();
            /* Adopt the ROM mode already set above. Reprogramming it here
               cleared VRAM and made the logo disappear for a second time. */
        }
        else
            kvideo_boot(runtime_handoff.video_mode,runtime_handoff.cpu_type,runtime_handoff.st_ram_bytes,timer_service);
        /* The card may have come up in 8 bpp (VIDEO CFG  VOFA ... 256); the
           compositor has to adopt that depth before the first paint. */
        if(kvideo_is_vofa())kgfx_set_depth(kvideo_mode_width(),kvideo_mode_height(),kvideo_mode_bpp());
        kconsole_flush();
        kvideo_flush();
        kernel_preemption_enable();
        /* Earliest moment the kernel's own display path is live: the card format
           is adopted and VOFA.SYS is attached, so kvideo_flush() really reaches
           the card.  Before this the visible image comes from the boot layer and
           painting here is invisible (verified: even a full-screen fill had no
           effect). */
        kernel_startup_services(&runtime_handoff,timer_service);
        {uint16_t pitch=80u;uint8_t bpp=0u;uint8_t *fb;
         runtime_handoff.console_puts("COMPOSITOR (WM): ");
#if CONFIG_WM
         runtime_handoff.console_puts("READY / ON DESKTOP ENTRY\n");
#else
         runtime_handoff.console_puts("DISABLED / TEXT PROFILE\n");
#endif
#if CONFIG_TOS
         kernel_tos_set_console(kconsole_puts);
         kernel_tos_set_trace(runtime_handoff.console_puts);
         kernel_tos_set_console_clear(kconsole_clear);
         kernel_tos_set_cpu(runtime_handoff.cpu_type);
         kernel_tos_set_input(kernel_user_terminal_getchar);
         kernel_tos_set_video(tos_enter_screen,kgfx_restore_console,kgfx_clear,tos_fill,tos_text,tos_blit_mono,tos_blit_planar);
         /* The VDI workstations start zeroed and open=0 only after this call;
            it was never invoked, so v_opnvwk handed out stale handles. */
         tos_vdi_init();
#endif
         runtime_handoff.console_puts("ATOS 2.0: NATIVE TEXT SESSIONS 80x25\n");
         ksessions_init(shell,runtime_handoff.video_mode==2u?~0:0);
         shell->start();
         /* Auto-start the desktop: the user should never have to type
            'desktop.app'.  It becomes the GUI keyboard owner and enters desktop
            mode; the text shell keeps running on tts1 behind it and is reachable
            from the launcher's "Open terminal".  If the image has no desktop the
            spawn simply fails and the text shell stays in charge. */
         {extern int32_t kernel_spawn_user_async(const char *,const char *,const char *,uint32_t *);
          boot_show(96u,"LOADING DESKTOP.APP");
          int32_t desktop_rc=kernel_spawn_user_async("\\SYSTEM\\APP\\DESKTOP.APP","","\\",&kernel_desktop_pid);
          if(desktop_rc<0){kernel_desktop_pid=0u;kernel_desktop_report(desktop_rc,1);}}
         fb=kgfx_framebuffer(&pitch,&bpp);
#if CONFIG_TOS
         if(fb){tos_vdi_set_surface(fb,pitch);lm_wr32((void *)(uintptr_t)0x44Eu,(uint32_t)(uintptr_t)fb);}
         lm_wr32((void *)(uintptr_t)0x4F2u,(uint32_t)(uintptr_t)tos_syshdr);
         lm_wr16((void *)(uintptr_t)0x4BAu,0u);
#else
         (void)fb;
#endif
        }
#if CONFIG_TOS
        /* Optional TOS compatibility preload (SETTINGS checkbox).  Off by
           default, so START stays as short as before; with it on, TOS.SYS and
           its buffers are requested in TT RAM when TOS_PREFER_TT is set too. */
        tos_cfg_read();
        if(kernel_tos_cfg_at_start){
            int32_t tos_rc=kernel_tos_compat_ensure(kernel_tos_cfg_prefer_tt?1:0);
            if(tos_rc>0){
                /* Report the REAL placement: prefer_tt is a preference and the
                   allocator may fall back to ST RAM, so the address decides. */
                uint8_t *sc=kernel_tos_compat_screen();
                int in_tt=sc&&(uintptr_t)sc>=(uintptr_t)runtime_handoff.st_ram_bytes;
                runtime_handoff.console_puts(in_tt?"TOS.SYS: LOADED (TT RAM)\n"
                                                 :"TOS.SYS: LOADED (ST RAM)\n");
            }else runtime_handoff.console_puts("TOS.SYS: PRELOAD FAILED\n");
        }
#endif
        /* End of the quiet bring-up.  Until this point quiet mode kept the boot
           screen clean; leaving it on into normal operation silently swallowed
           every later kernel diagnostic (a dead desktop left no trace at all).
           Nothing after START is "boot noise" any more. */
        boot_show(99u,"STARTING DESKTOP");
        kconsole_quiet(0);
        for(;;){
            extern uint32_t kernel_user_hires_clock(void);
            extern uint32_t kdbg_loop_cycles,kdbg_loop_calls;
            uint32_t t_loop=kernel_user_hires_clock();
            uint32_t terminal_result;
            timer_service->poll();
            /* GUI command line: execute a queued shell line in the shell's own
               context, then stop capturing its output for the caller. */
            if(shell->run_pending())kconsole_capture_end();
            if(timer_service->switch_pending()){
                timer_service->acknowledge_switch();
                proc_service->timer_tick();
                proc_service->cooperate();
            }
            for(i=0;i<64u;++i)input_service->poll(0u);
            ksessions_pump();
            for(i=0;i<32u;++i){
                /* Supervisor work can retain IPL6 after a user quantum.
                   Keep raw serial capture alive between event consumers;
                   INPUT.SYS alone continues to parse the captured bytes. */
                kernel_ikbd_poll_capture();
                /* A foreground process can also capture GUI input. Deliver
                   its events to the GUI mailbox, not the console getchar path. */
                if(kernel_user_gui_keyboard_active()){
                    {extern uint32_t kdbg_route_gui; ++kdbg_route_gui;}
                    if(kernel_wm_console_visible()&&kernel_wm_console_focused())shell->pump(0u);
                    else if(!kernel_user_gui_keyboard_wake()){
                        /* Queues are empty or the recipient is backpressured.
                           Another 31 polls cannot deliver anything. Leave new
                           arrivals queued for the next supervisor pass. */
                        ksessions_pump();break;
                    }
                }else{extern uint32_t kdbg_route_shell; ++kdbg_route_shell;
                    if(kernel_user_terminal_active()){
                        terminal_result=kernel_user_terminal_wake();
                        if(terminal_result>=2){(void)kernel_user_console_dirty();shell->redraw_prompt();}
                    }else shell->pump(0u);
                }
                ksessions_pump();
            }
            /* Supervise the desktop: a successful spawn says nothing about what
               happens next, and the crash we just fixed came after a clean
               spawn.  Taking the completion here (before the generic completion
               path) reports the desktop once, with its real fault status. */
            if(kernel_desktop_pid&&!kernel_desktop_stopped){
                int32_t desktop_status=0;
                /* -1 means still alive; only 1 carries a completed exit status. */
                if(kernel_user_take_completion(kernel_desktop_pid,&desktop_status)==1){
                    kernel_desktop_report(desktop_status,0);
                    kernel_desktop_pid=0u;
                }
            }
            kernel_notify_pending(&runtime_handoff,shell);
            kernel_dispatch_ready_user();
            {
                int output=kernel_dispatch_output_user();
                /* Completion prints its own prompt after the exit message. */
                if(!kernel_handle_dispatch_completion(&runtime_handoff,shell)&&output&&
                   !kernel_user_terminal_active()&&!kernel_user_gui_keyboard_active())shell->redraw_prompt();
            }
            ksessions_pump();
            /* Audio: queued PSG effects are applied here, in the kernel's own
               loop, so sound does not depend on how many frames an application
               renders.  Only PSG register writes happen inside. */
            kaudio_pump_fallback();

#ifdef ARK_TEST_TOS_AUTORUN
            /* Test-only: run the TOS probe from the kernel once, well after the
               native game has taken the audio owner.  Needed because the RUN
               dialog cannot be reached while a client window holds the keyboard
               focus, and because the Hatari event interface cannot move the
               pointer.  This is the BLOCKING launcher (the dialog uses the async
               variant), which produces a normal, accounted TOS session. */
            {static uint32_t ar_start=0u,ar_done=0u;
             uint32_t ar_now=kernel_user_hires_clock();
             if(!ar_start)ar_start=ar_now+12288u*35u;
             if(!ar_done&&(int32_t)(ar_now-ar_start)>=0){
                 ar_done=1u;
                 (void)kernel_tos_start_async("\\SYSTEM\\APP\\F.PRG");
             }}
#endif

#ifdef ARK_TEST_FOCUS_DESKTOP
extern void kernel_wm_console_focus(void);
extern void kernel_wm_console_show(void);
extern void kernel_gui_focus_set(uint32_t pid);
extern int32_t kernel_tos_start_async(const char *path);
            /* Test-only (audit scenario B): hand the keyboard focus back to the
               desktop periodically, so its RUN dialog can be reached while a game
               window is on top.  This lives in the kernel because adding the same
               code to the desktop breaks its PC-relative addressing budget. */
#ifdef ARK_TEST_SPAWN
            /* Test-only: launch a native app by path, reliably, without the
               keyboard-driven RUN dialog (which cannot be automated here). */
            {static uint32_t sp_n;if(++sp_n==12288u*22u){
                extern int32_t kernel_spawn_user(const char *,const char *,const char *,uint32_t *);
                uint32_t sp_pid=0u;
                (void)kernel_spawn_user("\\SYSTEM\\APP\\MODPLAY.APP","","\\",&sp_pid);}}
#endif
            {static uint32_t focus_n;if(++focus_n>=8000u){focus_n=0u;kernel_wm_console_show();kernel_wm_console_focus();kernel_gui_focus_set(0u);}}
#endif

#ifdef ARK_TEST_AUDIO_LOAD
            /* Test-only load generator: 8 audio events with wait=0 land in ONE
               Timer-C tick, so kaudio_dbg_isr_max shows the real cost of a full
               batch.  This is the number that decides how much work a software
               mixer/tracker can afford per tick. */
            {static uint32_t ld_n;if(++ld_n>=6144u){ld_n=0u;
                static kaudio_event_t ld_ev[8];uint32_t k;const uint32_t ld_pid=0x7f000001u;
                (void)kaudio_claim(ld_pid,0);
                for(k=0u;k<8u;++k){
                    ld_ev[k].wait=0u;ld_ev[k].aux=(uint16_t)(120u+k*20u);
                    ld_ev[k].op=(k==7u)?KAUDIO_OP_STOP:KAUDIO_OP_TONE;
                    ld_ev[k].a=(k==7u)?7u:(uint8_t)(k%3u);
                    ld_ev[k].b=(k==7u)?0u:10u;
                    ld_ev[k].pad[0]=ld_ev[k].pad[1]=ld_ev[k].pad[2]=0u;}
                (void)kaudio_queue(ld_pid,ld_ev,8u);}}
#endif
#if CONFIG_TOS
            /* Deferred TOS window composite: the heavy planar conversion runs
               here, never in the Timer C interrupt handler. */
            kernel_tos_async_pump();
            tos_composite_work();
#endif
            /* Text profile: console or retained app frame, then session overlay. */
            if(!kgfx_desktop_active()&&!kernel_wm_console_visible())kconsole_flush();
            /* "Add Desk" asked for a desktop in the freshly created session:
               spawn it now, while that session is the current one. */
            if(ksessions_take_autostart()){
                uint32_t desk2_pid=0;int32_t sr;
                /* Session-switcher diagnostics: "Add Desk" spawn outcome. */
extern uint32_t kdbg_desk_ok,kdbg_desk_fail;
                sr=kernel_spawn_user_async("\\SYSTEM\\APP\\DESKTOP.APP","","\\",&desk2_pid);
                if(sr<0){++kdbg_desk_fail;kernel_desktop_pid=0u;kernel_desktop_report(sr,1);}
                else{++kdbg_desk_ok;kernel_desktop_pid=desk2_pid;kernel_desktop_stopped=0u;}
            }
            kernel_wm_frame_tick(timer_service->ticks());
            /* Siatka bezpieczenstwa zmiany trybu: jesli aplikacja nie potwierdzi
               nowego trybu w 4 s, jadro SAMO wraca do poprzedniego.  Wczesniej
               ten rollback byl wolany TYLKO z kvideo_guard_control(), czyli
               przez sama aplikacje - gdy SETTINGS sie zaciela (np. po wejsciu w
               nieczytelny tryb), nic nie cofalo trybu i system wygladal na
               zawieszony.  Teraz sprawdza to petla jadra, niezaleznie od pulpitu.
               Uzywamy tego samego zegara co uzbrojenie (kernel_user_clock). */
            /* Ten sam zegar co uzbrojenie: tykniecia z przerwania Timer-C.
               kernel_user_clock() (polling) gubi okresy, gdy petla zwalnia -
               w trybie-smieciach 4 bpp zegar chodzil 23 Hz i "4 s" wychodzilo
               8-14 s.  Zmierzone, patrz NAPRAWA-TRAP-4BPP-20261017.txt. */
            if(kvideo_guard_active())(void)kvideo_guard_rollback(kernel_user_wall_clock());
            kernel_wm_applet_bar(timer_service->ticks());
            ksessions_paint();
            kvideo_flush();
            kdbg_loop_cycles+=kernel_user_hires_clock()-t_loop;++kdbg_loop_calls;
        }
    }
    }
    kconsole_boot_screen(0);kconsole_quiet(0);
    runtime_handoff.console_puts("\nPRESS RESET");
    kconsole_flush();
    for(;;)__asm__ volatile("stop #0x2700");
}
