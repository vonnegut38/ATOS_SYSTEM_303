#include "boot_protocol.h"
#include "kernel_memory.h"
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
#include "kernel_graphics.h"
#include "kernel_surface.h"

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

/* TOS personality hooks (tos_prg.c / tos_vdi.c). */
extern int32_t kernel_tos_run_program(const char *path);
extern void kernel_tos_set_console(void (*puts)(const char *));
extern void kernel_tos_set_trace(void (*puts)(const char *));
extern void kernel_tos_set_cpu(uint8_t cpu_type);
extern void kernel_tos_set_input(int32_t (*pop)(void));
extern void kernel_tos_set_console_clear(void (*clear)(void));
extern void kernel_tos_set_video(void (*enter)(void),void (*leave)(void),void (*clear)(uint8_t),
                                 void (*fill)(int,int,int,int,uint8_t),
                                 void (*text)(int,int,const char *,uint8_t,uint8_t),
                                 void (*blit_mono)(int,int,int,int,const uint8_t *,int,uint8_t,uint8_t));
extern void tos_vdi_set_surface(uint8_t *surface,uint16_t pitch);
extern uint8_t tos_syshdr[512];
extern uint32_t kernel_linef_pc,kernel_linef_op,kernel_linef_frame[6];

/* Compositor and session services (kernel_console.c/kernel_sessions.c/kernel_wm.c). */
extern void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                          void (*number)(uint32_t));
extern void kconsole_clear(void);
extern void kconsole_puts(const char *);
extern void kconsole_put_u32(uint32_t);
extern void kconsole_flush(void);
extern void kconsole_set_grid(unsigned cols,unsigned rows);
extern unsigned ksessions_current(void);
extern int32_t ksessions_control(uint32_t op,uint32_t value);
extern void ksessions_init(const kernel_shell_service_t *shell,int enabled);
extern void ksessions_pump(void);
extern void ksessions_paint(void);
extern int  kernel_wm_self_test(void);
extern int  kernel_wm_enabled(void);
extern int  kernel_wm_console_visible(void);
extern void kernel_wm_console_present(void);
extern void kernel_wm_damage_all(void);
extern void kernel_wm_set_desktop_session(int session);
extern void kernel_wm_frame_tick(uint32_t now);
extern void kernel_wm_applet_bar(uint32_t now);
extern void kernel_wm_debug(int *count,int *flags,int *hidden,int *damage);

/* Video output device (kernel_video.c/kernel_video_boot.c). */
extern void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer);
extern void kernel_fault_report(void (*)(const char *),void (*)(uint32_t));
extern uint8_t *kernel_wm_app_surface(uint32_t pid);
extern void kernel_tos_use_surface(uint8_t *surface,uint16_t pitch);
extern void kvideo_flush(void);
extern const void *kvideo_front(void);
extern uint32_t kvideo_dbg_active,kvideo_dbg_calls,kvideo_dbg_copies;

extern void kgfx_gem_desktop_sticky(int on);

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
static void tos_enter_screen(void)
{
    kgfx_gem_desktop_sticky(1);
    kgfx_enter_desktop();
}

typedef struct {const char *path,*name;uint32_t timeout_ticks;} startup_service_t;

static boot_handoff_t runtime_handoff;

static void kernel_startup_services(const boot_handoff_t *h,
                                    const kernel_timer_service_t *timer)
{
    static const startup_service_t table[]={
        {"\\SYSTEM\\INIT.APP","SYSTEM.INIT",250u},
        {"\\SYSTEM\\GFX.APP","SYSTEM.GFX",250u},
        {"\\SYSTEM\\GUI.APP","SYSTEM.GUI",250u}
    };
    uint8_t i;uint32_t pid,start,now;int32_t result;
    h->console_puts("STARTUP SERVICES: ");
    for(i=0;i<(uint8_t)(sizeof(table)/sizeof(table[0]));++i){
        if(i)h->console_puts("                  ");
        pid=0;result=kernel_spawn_user_async(table[i].path,"","\\",&pid);
        if(result<0||!pid){h->console_puts(table[i].name);h->console_puts(" LOAD FAILED\n");continue;}
        timer->poll();start=timer->ticks();
        for(;;){
            if(kernel_user_service_pid(table[i].name)==pid){
                h->console_puts(table[i].name);h->console_puts(" READY\n");break;}
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

static void kernel_handle_dispatch_completion(const boot_handoff_t *h,
                                               const kernel_shell_service_t *shell)
{
    uint32_t pid;int32_t status;int first=1,had_output;
    if(!kernel_dispatch_completed_user()||kernel_user_terminal_active()||
       kernel_user_gui_keyboard_active())return;
    if(kernel_user_last_terminal()!=ksessions_current())return;
    if(shell->line_active()){
        /* Program output may have advanced the console, but completion text
           must remain queued until the user finishes the current line. */
        shell->redraw_prompt();
        return;
    }
    had_output=kernel_user_console_dirty();
    while(kernel_user_next_completion(&pid,&status)){
        if(!had_output||!first)h->console_puts("\n");
        first=0;kernel_print_completion(h,pid,status);
    }
    if(!kernel_user_ready_pending())shell->redraw_prompt();
}

void kernel_main(const boot_handoff_t *h)
{
    enum block_state block_state;
    uint8_t i,shown;
    int fat_mounted=0;
    int proc_ready=0;
    const kernel_proc_service_t *proc_service=0;
    const kernel_timer_service_t *timer_service=0;
    const kernel_input_service_t *input_service=0;
    int timer_ready=0,input_ready=0,shell_ready=0;
    char drive[4]={'C',':',' ','\0'};
    if(!h||h->magic!=BOOT_HANDOFF_MAGIC||!h->console_clear||!h->console_puts)
        for(;;)__asm__ volatile("stop #0x2700");
    kernel_exception_bind(h);
    if(h->cpu_type>1u)__asm__ volatile("moveq #0,%d0\n\t.long 0x4e7b0801");
    kernel_exception_install(h->cpu_type);
    h->console_clear();
    h->console_puts("ATARI SYSTEM KERNEL 0.03\n\n");
    h->console_puts("EXCEPTION VECTORS: INSTALLED\n");
    if(!kmem_init(h->st_ram_bytes,h->tt_ram_bytes,(uint32_t)(uintptr_t)&__image_memory_end)){h->console_puts("MEMORY MANAGER: FAILED\n");for(;;)__asm__ volatile("stop #0x2700");}
    h->console_puts("MEMORY MANAGER: READY\n");
    h->console_puts("GRAPHICS BACKEND: SHIFTER ");
    if(kgfx_init(2u,h->blitter_present,h->blitter_enabled)&&kgfx_self_test()&&
       kgfx_get_service()->magic==KGFX_SERVICE_MAGIC){
        const kgfx_info_t *gfx=kgfx_get_info();h->console_put_u32(gfx->width);
        h->console_puts("X");h->console_put_u32(gfx->height);
        h->console_puts(" / CLIP+DIRTY");
        if(gfx->capabilities&KGFX_CAP_BLITTER_ENABLED)h->console_puts(" / BLITTER READY");
        else if(gfx->capabilities&KGFX_CAP_BLITTER_PRESENT)h->console_puts(" / BLITTER CPU FALLBACK");
        h->console_puts("\n");
    }else h->console_puts("SELF-TEST FAILED\n");
    {const kgfx_info_t *gfx=kgfx_get_info();int surface_ok;
        ksurface_init(gfx->width,gfx->height);surface_ok=ksurface_self_test();
        ksurface_init(gfx->width,gfx->height);
        h->console_puts("SURFACE MANAGER: ");
        h->console_puts(surface_ok?"8 HANDLES / OWNER GUARDS OK\n":"SELF-TEST FAILED\n");}
    {kmem_stats_t st,tt;int memory_ok=kmem_self_test();kmem_get_st_stats(&st);kmem_get_tt_stats(&tt);
        h->console_puts("ST-RAM POOL: ");h->console_put_u32(st.free/1024u);
        h->console_puts(" KB FREE\nTT-RAM POOL: ");
        if(h->tt_ram_bytes){h->console_put_u32(tt.free/1024u);h->console_puts(" KB FREE\n");}
        else h->console_puts("NOT PRESENT\n");
        h->console_puts("MEMORY SELF-TEST: ");h->console_puts(memory_ok?"PASS\n":"FAIL\n");}
    kgfx_set_palette(0,0x0000u);
    kgfx_set_palette(1,0x0fffu);
    kconsole_init(1,h->console_clear,h->console_puts,h->console_put_u32);
    kconsole_set_grid(80u,18u);
    {uint32_t k;for(k=0;k<sizeof(runtime_handoff);++k)((uint8_t *)&runtime_handoff)[k]=((const uint8_t *)h)[k];}
    runtime_handoff.console_clear=kconsole_clear;
    runtime_handoff.console_puts=kconsole_puts;
    runtime_handoff.console_put_u32=kconsole_put_u32;
    kernel_exception_bind(&runtime_handoff);
    if(runtime_handoff.video_mode==2u){kconsole_clear();kconsole_puts("ATARI SYSTEM KERNEL 0.03 / TEXT CONSOLE 80x25\n");}

    {enum module_state state=kernel_load_acsi(h);runtime_handoff.console_puts("ACSI DMA DRIVER (ABI 3): ");if(state==MODULE_READY)runtime_handoff.console_puts("READY");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND - BIOS FALLBACK");else if(state==MODULE_INVALID)runtime_handoff.console_puts("INVALID IMAGE - BIOS FALLBACK");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO ST-RAM - BIOS FALLBACK");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED - BIOS FALLBACK");else runtime_handoff.console_puts("I/O FAILED - BIOS FALLBACK");runtime_handoff.console_puts("\n");}
    block_state=kblock_init(runtime_handoff.boot_acsi_id);runtime_handoff.console_puts("BLOCK LAYER: ");if(block_state==BLOCK_READY){runtime_handoff.console_puts("READY\nBLOCK CACHE: ");if(kblock_cache_sectors()){runtime_handoff.console_put_u32(kblock_cache_sectors());runtime_handoff.console_puts(" SECTORS / ");runtime_handoff.console_puts(kblock_cache_is_tt()?"TT-RAM\n":"ST-RAM\n");}else runtime_handoff.console_puts("OFF / LOW MEMORY\n");}else if(block_state==BLOCK_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else runtime_handoff.console_puts("I/O FAILED\n");
    if(block_state==BLOCK_READY){enum partition_state ps=kpart_scan(runtime_handoff.boot_partition);runtime_handoff.console_puts("AHDI VOLUMES: ");if(ps==KPART_READY){runtime_handoff.console_put_u32(kpart_count());if(kpart_truncated())runtime_handoff.console_puts(" / MORE DEFERRED");runtime_handoff.console_puts("\n");shown=kpart_count()>1u?1u:kpart_count();for(i=0;i<shown;++i){drive[0]=(char)('C'+i);runtime_handoff.console_puts(drive);runtime_handoff.console_puts(kpart_id(i));runtime_handoff.console_puts("  ");runtime_handoff.console_put_u32(kpart_size(i)/2048u);runtime_handoff.console_puts(" MB");if(kpart_is_boot(i))runtime_handoff.console_puts("  BOOT");if(!kpart_accessible(i))runtime_handoff.console_puts("  DEFERRED");runtime_handoff.console_puts("\n");}if(kpart_count()>shown){runtime_handoff.console_puts("... ");runtime_handoff.console_put_u32(kpart_count()-shown);runtime_handoff.console_puts(" MORE VOLUMES\n");}{enum fat_state state=kfat_mount(kpart_boot_start(),kpart_boot_id());runtime_handoff.console_puts("FAT LAYER: ");if(state==FAT_MOUNTED){fat_mounted=1;runtime_handoff.console_puts("FAT16 MOUNTED\nBOOT VOLUME: ");runtime_handoff.console_puts(kfat_partition_id());runtime_handoff.console_puts(" / ");runtime_handoff.console_put_u32(kfat_sector_size());runtime_handoff.console_puts(" BPS\n");}else if(state==FAT_PARTITION_INVALID)runtime_handoff.console_puts("PARTITION INVALID\n");else if(state==FAT_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==FAT_BPB_INVALID)runtime_handoff.console_puts("BPB INVALID\n");else runtime_handoff.console_puts("NOT FAT16\n");}}else runtime_handoff.console_puts(ps==KPART_IO_FAILED?"I/O FAILED\n":"INVALID TABLE\n");}
    if(fat_mounted){static const char core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};enum module_state state;runtime_handoff.console_puts("VOLUME FLAG: ");if(kfat_health()==FAT_HEALTH_CLEAN)runtime_handoff.console_puts("CLEAN\n");else if(kfat_health()==FAT_HEALTH_DIRTY)runtime_handoff.console_puts("DIRTY - READ ONLY\n");else if(kfat_health()==FAT_HEALTH_HARD_ERROR)runtime_handoff.console_puts("HARD ERROR - READ ONLY\n");else runtime_handoff.console_puts("UNKNOWN\n");state=kernel_load_system_module(core_name,0x0101u,1);runtime_handoff.console_puts("CORE MODULE/SERVICE: ");if(state==MODULE_READY&&kernel_find_service(KERNEL_SERVICE_CORE,1u))runtime_handoff.console_puts("READY\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SERVICE MISSING\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char vfs_name[11]={'V','F','S',' ',' ',' ',' ',' ','S','Y','S'},core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};uint32_t size=0;enum module_state state=kernel_load_system_module(vfs_name,0x0102u,1);const kernel_vfs_service_t *vfs=(const kernel_vfs_service_t *)kernel_find_service(KERNEL_SERVICE_VFS,1u);runtime_handoff.console_puts("VFS MODULE/SERVICE: ");if(state==MODULE_READY&&vfs&&vfs->magic==KERNEL_VFS_MAGIC&&vfs->version>=KERNEL_VFS_VERSION&&vfs->size>=sizeof(*vfs)&&vfs->stat_system&&vfs->stat_system(core_name,&size)&&size>=BOOT_IMAGE_HEADER_SIZE)runtime_handoff.console_puts("READY\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char proc_name[11]={'P','R','O','C',' ',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(proc_name,0x0103u,1);proc_service=(const kernel_proc_service_t *)kernel_find_service(KERNEL_SERVICE_PROC,KERNEL_PROC_VERSION);runtime_handoff.console_puts("PROCESS MANAGER: ");proc_ready=state==MODULE_READY&&proc_service&&proc_service->magic==KERNEL_PROC_MAGIC&&proc_service->version>=KERNEL_PROC_VERSION&&proc_service->size>=sizeof(*proc_service)&&proc_service->task_count&&proc_service->cooperate&&proc_service->dispatch_count&&proc_service->sleep_ticks&&proc_service->timer_tick&&proc_service->sleep_remaining&&proc_service->context_self_test&&(proc_service->capabilities&KERNEL_PROC_CAP_FULL_CONTEXT)&&proc_service->task_count()==2u&&proc_service->cooperate()&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==2u&&proc_service->dispatch_count(1u)==2u&&proc_service->context_self_test();if(proc_ready)runtime_handoff.console_puts("2 TASKS / FULL CTX RR OK\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char timer_name[11]={'T','I','M','E','R',' ',' ',' ','S','Y','S'};uint32_t before,guard,quantum;enum module_state state=kernel_load_system_module(timer_name,0x0104u,1);timer_service=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);timer_ready=proc_ready&&state==MODULE_READY&&timer_service&&timer_service->magic==KERNEL_TIMER_MAGIC&&timer_service->version>=KERNEL_TIMER_VERSION&&timer_service->size>=sizeof(*timer_service)&&timer_service->poll&&timer_service->ticks&&timer_service->switch_pending&&timer_service->acknowledge_switch&&timer_service->frequency_hz==50u&&proc_service->sleep_ticks(0u,2u)&&proc_service->sleep_remaining(0u)==2u;before=timer_ready?timer_service->ticks():0;for(quantum=0;timer_ready&&quantum<4u;++quantum){guard=2000000UL;while(guard--&&!timer_service->switch_pending())timer_service->poll();if(!timer_service->switch_pending()){timer_ready=0;break;}timer_service->acknowledge_switch();proc_service->timer_tick();if(timer_service->switch_pending()||!proc_service->cooperate())timer_ready=0;}if(timer_ready)timer_ready=timer_service->ticks()>=before+4u&&proc_service->sleep_remaining(0u)==0u&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==6u;runtime_handoff.console_puts("TIMER/SLEEP QUEUE: ");if(timer_ready)runtime_handoff.console_puts("4Q / WAKE OK\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char event_name[11]={'E','V','E','N','T',' ',' ',' ','S','Y','S'};uint32_t type=0,value=0;enum module_state state=kernel_load_system_module(event_name,0x0105u,1);const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);int ok=proc_ready&&state==MODULE_READY&&event&&event->magic==KERNEL_EVENT_MAGIC&&event->version>=KERNEL_EVENT_VERSION&&event->size>=sizeof(*event)&&event->receive&&event->send&&event->pending&&!event->receive(0u,&type,&value)&&proc_service->task_state(0u)==KERNEL_PROC_WAITING&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==7u&&event->send(0u,0x1234u,0x41535953UL)&&event->pending(0u)&&proc_service->task_state(0u)==KERNEL_PROC_READY&&event->receive(0u,&type,&value)&&!event->pending(0u)&&type==0x1234u&&value==0x41535953UL&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==6u&&proc_service->dispatch_count(1u)==8u;runtime_handoff.console_puts("EVENT QUEUE: ");if(ok)runtime_handoff.console_puts("WAIT / WAKE OK\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char input_name[11]={'I','N','P','U','T',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(input_name,0x0106u,1);input_service=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);input_ready=state==MODULE_READY&&input_service&&input_service->magic==KERNEL_INPUT_MAGIC&&input_service->version>=KERNEL_INPUT_VERSION&&input_service->size>=sizeof(*input_service)&&input_service->poll&&input_service->classify&&input_service->dropped_events&&input_service->classify(0x1eu)==KERNEL_INPUT_KEY_DOWN&&input_service->classify(0x9eu)==KERNEL_INPUT_KEY_UP&&input_service->classify(0xf8u)==0u&&input_service->dropped_events()==0u;runtime_handoff.console_puts("IKBD INPUT: ");if(input_ready)runtime_handoff.console_puts("POLLING READY\n");else if(state==MODULE_READY)runtime_handoff.console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)runtime_handoff.console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)runtime_handoff.console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)runtime_handoff.console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)runtime_handoff.console_puts("INIT FAILED\n");else runtime_handoff.console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char shell_name[11]={'S','H','E','L','L',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(shell_name,0x0107u,1);const kernel_shell_service_t *shell=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);shell_ready=state==MODULE_READY&&timer_ready&&input_ready&&shell&&shell->magic==KERNEL_SHELL_MAGIC&&shell->version>=KERNEL_SHELL_VERSION&&shell->size>=sizeof(*shell)&&shell->start&&shell->pump&&shell->key_count&&shell->redraw_prompt&&shell->line_active&&shell->select_session&&shell->key_count()==0u;runtime_handoff.console_puts(shell_ready?"KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY\n":"KERNEL STATUS: SHELL UNAVAILABLE\n");
    if(shell_ready){
        kvideo_boot(runtime_handoff.video_mode,runtime_handoff.cpu_type,runtime_handoff.st_ram_bytes,timer_service);
        kconsole_flush();
        kvideo_flush();
        kernel_preemption_enable();
        kernel_startup_services(&runtime_handoff,timer_service);
        {uint16_t pitch=80u;uint8_t bpp=0u;uint8_t *fb;
         runtime_handoff.console_puts("COMPOSITOR (WM): ");
         runtime_handoff.console_puts(kernel_wm_self_test()?"SELFTEST OK\n":"SELF-TEST FAILED\n");
         kernel_tos_set_console(kconsole_puts);
         kernel_tos_set_trace(runtime_handoff.console_puts);
         kernel_tos_set_console_clear(kconsole_clear);
         kernel_tos_set_cpu(runtime_handoff.cpu_type);
         kernel_tos_set_input(kernel_user_terminal_getchar);
         kernel_tos_set_video(tos_enter_screen,kgfx_restore_console,kgfx_clear,tos_fill,tos_text,0);
         runtime_handoff.console_puts("DESKTOP MODE: ");
         kgfx_enter_desktop();
         if(kgfx_desktop_active()&&kernel_wm_enabled()){
             int wmc=-2,flags=-2,hidden=-2,damage=-2;
             kgfx_session_view(1);
             kernel_wm_console_present();
             kernel_wm_damage_all();
             runtime_handoff.console_puts("ENTERED, CONSOLE WINDOW\n");
             runtime_handoff.console_puts("DBG front=");
             kconsole_put_u32((uint32_t)(uintptr_t)kvideo_front());
             runtime_handoff.console_puts(" desktopfb=");
             kconsole_put_u32((uint32_t)(uintptr_t)kgfx_framebuffer(0,0));
             runtime_handoff.console_puts(" active=");
             kconsole_put_u32((uint32_t)kgfx_desktop_active());
             runtime_handoff.console_puts(" wmc=");
             kernel_wm_debug(&wmc,&flags,&hidden,&damage);
             kconsole_put_u32((uint32_t)wmc);runtime_handoff.console_puts("/");
             kconsole_put_u32((uint32_t)flags);runtime_handoff.console_puts("/");
             kconsole_put_u32((uint32_t)hidden);runtime_handoff.console_puts("/");
             kconsole_put_u32((uint32_t)damage);
             runtime_handoff.console_puts(" vid=");
             kconsole_put_u32(kvideo_dbg_active);runtime_handoff.console_puts("/");
             kconsole_put_u32(kvideo_dbg_calls);runtime_handoff.console_puts("/");
             kconsole_put_u32(kvideo_dbg_copies);
             runtime_handoff.console_puts("\n");
         }else runtime_handoff.console_puts(kgfx_desktop_active()?"WM OFF\n":"NOT ENTERED\n");
         ksessions_init(shell,runtime_handoff.video_mode==2u?~0:0);
         shell->start();
         fb=kgfx_framebuffer(&pitch,&bpp);
         if(fb){tos_vdi_set_surface(fb,pitch);lm_wr32((void *)(uintptr_t)0x44Eu,(uint32_t)(uintptr_t)fb);}
         lm_wr32((void *)(uintptr_t)0x4F2u,(uint32_t)(uintptr_t)tos_syshdr);
         lm_wr16((void *)(uintptr_t)0x4BAu,0u);
        }
        {const char *volatile autoexec="gem";
         if(autoexec[0]=='g'&&autoexec[1]=='e'&&autoexec[2]=='m'&&autoexec[3]==0){
             runtime_handoff.console_puts("GEM: DESKTOP STARTED\n");
             (void)ksessions_control(3u,0u);
             kernel_wm_set_desktop_session(2);
             kgfx_enter_desktop();
             /* The session manager may have hidden the desktop (the shell is
                the current session), which switches the video back to the
                console and hides the compositor output.  Show the desktop so
                the TOS app window is actually displayed while the shell keeps
                running in its own session. */
             kgfx_session_view(1);
             /* Task 4: give the TOS program a compositor window surface so its
                GEM output appears on the desktop (kernel_tos_use_surface). */
             {uint8_t *tos_window=kernel_wm_app_surface(0x544f53u);
              if(tos_window)kernel_tos_use_surface(tos_window,80u);}
             if(kernel_tos_run_program("\\SYSTEM\\TEST\\NOSYS.PRG")==0)runtime_handoff.console_puts("GEM: auto-start disabled\n");
             else runtime_handoff.console_puts("GEM: auto-start disabled\n");
             kernel_fault_report(runtime_handoff.console_puts,runtime_handoff.console_put_u32);
             kgfx_enter_desktop();
         }else{
             uint32_t size;uint8_t attributes;
             if(kfat_stat_path("\\SYSTEM\\TEST\\PROG1.PRG",&size,&attributes)==FAT_FILE_FOUND){
                 runtime_handoff.console_puts("AUTOEXEC: \\SYSTEM\\TEST\\PROG1.PRG\n");
                 if(kernel_tos_run_program("\\SYSTEM\\TEST\\PROG1.PRG")==0)runtime_handoff.console_puts("AUTOEXEC: OK\n");
                 else runtime_handoff.console_puts("AUTOEXEC: program closed\n");
             }else runtime_handoff.console_puts("AUTOEXEC: none\n");
             kernel_fault_report(runtime_handoff.console_puts,runtime_handoff.console_put_u32);
         }
        }
        for(;;){
            uint32_t terminal_result;
            timer_service->poll();
            if(timer_service->switch_pending()){
                timer_service->acknowledge_switch();
                proc_service->timer_tick();
                proc_service->cooperate();
            }
            for(i=0;i<64u;++i)input_service->poll(0u);
            ksessions_pump();
            for(i=0;i<32u;++i){
                if(kernel_user_terminal_active()){
                    terminal_result=kernel_user_terminal_wake();
                    if(terminal_result>=2){(void)kernel_user_console_dirty();shell->redraw_prompt();}
                }else if(kernel_user_gui_keyboard_active()){
                    if(kernel_wm_console_visible())shell->pump(0u);
                    else (void)kernel_user_gui_keyboard_wake();
                }else shell->pump(0u);
                ksessions_pump();
            }
            kernel_notify_pending(&runtime_handoff,shell);
            kernel_dispatch_ready_user();
            if(kernel_dispatch_output_user()&&!kernel_user_terminal_active()&&!kernel_user_gui_keyboard_active())shell->redraw_prompt();
            kernel_handle_dispatch_completion(&runtime_handoff,shell);
            ksessions_pump();
            if(!kernel_wm_console_visible())kconsole_flush();
            kernel_wm_frame_tick(timer_service->ticks());
            kernel_wm_applet_bar(timer_service->ticks());
            if(!kgfx_desktop_active())ksessions_paint();
            kvideo_flush();
        }
    }
    }
    runtime_handoff.console_puts("\nPRESS RESET");
    for(;;)__asm__ volatile("stop #0x2700");
}
