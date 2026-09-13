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
extern int32_t kernel_spawn_user_async(const char *,const char *,const char *,uint32_t *);
extern uint32_t kernel_user_service_pid(const char *);
extern int kernel_user_kill(uint32_t pid);

typedef struct {const char *path,*name;uint32_t timeout_ticks;} startup_service_t;

static void kernel_startup_services(const boot_handoff_t *h,
                                    const kernel_timer_service_t *timer)
{
    static const startup_service_t table[]={
        {"\\SYSTEM\\KERNEL\\INIT.APP","SYSTEM.INIT",250u},
        {"\\SYSTEM\\KERNEL\\GFX.APP","SYSTEM.GFX",250u},
        {"\\SYSTEM\\KERNEL\\GUI.APP","SYSTEM.GUI",250u}
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
    kernel_exception_install(h->cpu_type);
    h->console_clear();
    h->console_puts("ATARI SYSTEM KERNEL 0.03\n\n");
    h->console_puts("EXCEPTION VECTORS: INSTALLED\n");
    if(!kmem_init(h->st_ram_bytes,h->tt_ram_bytes,(uint32_t)(uintptr_t)&__image_memory_end)){h->console_puts("MEMORY MANAGER: FAILED\n");for(;;)__asm__ volatile("stop #0x2700");}
    h->console_puts("MEMORY MANAGER: READY\n");
    h->console_puts("BOOT DEVICE: ");h->console_puts((h->boot_acsi_id&0x80u)?"IDE / BIOS FALLBACK":"ACSI");h->console_puts("\n");
    h->console_puts("GRAPHICS BACKEND: SHIFTER ");
    if(kgfx_init(h->video_mode,h->blitter_present,h->blitter_enabled)&&kgfx_self_test()&&
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
    {kmem_stats_t st,tt;int memory_ok=kmem_self_test();kmem_get_st_stats(&st);kmem_get_tt_stats(&tt);h->console_puts("ST-RAM POOL: ");h->console_put_u32(st.free/1024u);h->console_puts(" KB FREE / LARGEST ");h->console_put_u32(st.largest);h->console_puts(" B\nTT-RAM POOL: ");if(h->tt_ram_bytes){h->console_put_u32(tt.free/1024u);h->console_puts(" KB FREE\n");}else h->console_puts("NOT PRESENT\n");h->console_puts("MEMORY SELF-TEST: ");h->console_puts(memory_ok?"PASS\n":"FAIL\n");}
    {enum module_state state=kernel_load_acsi(h);h->console_puts("ACSI DMA DRIVER (ABI 3): ");if(state==MODULE_READY)h->console_puts("READY");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND - BIOS FALLBACK");else if(state==MODULE_INVALID)h->console_puts("INVALID IMAGE - BIOS FALLBACK");else if(state==MODULE_NO_MEMORY)h->console_puts("NO ST-RAM - BIOS FALLBACK");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED - BIOS FALLBACK");else h->console_puts("I/O FAILED - BIOS FALLBACK");h->console_puts("\n");}
    block_state=kblock_init(h->boot_acsi_id);h->console_puts("BLOCK LAYER: ");if(block_state==BLOCK_READY){h->console_puts("READY\nBLOCK CACHE: 8 SECTORS / ");h->console_puts(kblock_cache_is_tt()?"TT-RAM\n":"ST-RAM\n");}else if(block_state==BLOCK_NO_MEMORY)h->console_puts("NO MEMORY\n");else h->console_puts("I/O FAILED\n");
    if(block_state==BLOCK_READY){enum partition_state ps=kpart_scan(h->boot_partition);h->console_puts("AHDI VOLUMES: ");if(ps==KPART_READY){h->console_put_u32(kpart_count());if(kpart_truncated())h->console_puts(" / MORE DEFERRED");h->console_puts("\n");shown=kpart_count()>1u?1u:kpart_count();for(i=0;i<shown;++i){drive[0]=(char)('C'+i);h->console_puts(drive);h->console_puts(kpart_id(i));h->console_puts("  ");h->console_put_u32(kpart_size(i)/2048u);h->console_puts(" MB");if(kpart_is_boot(i))h->console_puts("  BOOT");if(!kpart_accessible(i))h->console_puts("  DEFERRED");h->console_puts("\n");}if(kpart_count()>shown){h->console_puts("... ");h->console_put_u32(kpart_count()-shown);h->console_puts(" MORE VOLUMES\n");}{enum fat_state state=kfat_mount(kpart_boot_start(),kpart_boot_id());h->console_puts("FAT LAYER: ");if(state==FAT_MOUNTED){fat_mounted=1;h->console_puts(kfat_fat32()?"FAT32 MOUNTED\nBOOT VOLUME: ":"FAT16 MOUNTED\nBOOT VOLUME: ");h->console_puts(kfat_partition_id());h->console_puts(" / ");h->console_put_u32(kfat_sector_size());h->console_puts(" BPS\n");}else if(state==FAT_PARTITION_INVALID)h->console_puts("PARTITION INVALID\n");else if(state==FAT_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==FAT_BPB_INVALID)h->console_puts("BPB INVALID\n");else h->console_puts("NOT FAT16\n");}}else h->console_puts(ps==KPART_IO_FAILED?"I/O FAILED\n":"INVALID TABLE\n");}
    if(fat_mounted){static const char core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};enum module_state state;h->console_puts("VOLUME FLAG: ");if(kfat_health()==FAT_HEALTH_CLEAN)h->console_puts("CLEAN\n");else if(kfat_health()==FAT_HEALTH_DIRTY)h->console_puts("DIRTY - READ ONLY\n");else if(kfat_health()==FAT_HEALTH_HARD_ERROR)h->console_puts("HARD ERROR - READ ONLY\n");else h->console_puts("UNKNOWN\n");state=kernel_load_system_module(core_name,0x0101u,1);h->console_puts("CORE MODULE/SERVICE: ");if(state==MODULE_READY&&kernel_find_service(KERNEL_SERVICE_CORE,1u))h->console_puts("READY\n");else if(state==MODULE_READY)h->console_puts("SERVICE MISSING\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char vfs_name[11]={'V','F','S',' ',' ',' ',' ',' ','S','Y','S'},core_name[11]={'C','O','R','E',' ',' ',' ',' ','S','Y','S'};uint32_t size=0;enum module_state state=kernel_load_system_module(vfs_name,0x0102u,1);const kernel_vfs_service_t *vfs=(const kernel_vfs_service_t *)kernel_find_service(KERNEL_SERVICE_VFS,1u);h->console_puts("VFS MODULE/SERVICE: ");if(state==MODULE_READY&&vfs&&vfs->magic==KERNEL_VFS_MAGIC&&vfs->version>=KERNEL_VFS_VERSION&&vfs->size>=sizeof(*vfs)&&vfs->stat_system&&vfs->stat_system(core_name,&size)&&size>=BOOT_IMAGE_HEADER_SIZE)h->console_puts("READY\n");else if(state==MODULE_READY)h->console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char proc_name[11]={'P','R','O','C',' ',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(proc_name,0x0103u,1);proc_service=(const kernel_proc_service_t *)kernel_find_service(KERNEL_SERVICE_PROC,KERNEL_PROC_VERSION);h->console_puts("PROCESS MANAGER: ");proc_ready=state==MODULE_READY&&proc_service&&proc_service->magic==KERNEL_PROC_MAGIC&&proc_service->version>=KERNEL_PROC_VERSION&&proc_service->size>=sizeof(*proc_service)&&proc_service->task_count&&proc_service->cooperate&&proc_service->dispatch_count&&proc_service->sleep_ticks&&proc_service->timer_tick&&proc_service->sleep_remaining&&proc_service->context_self_test&&(proc_service->capabilities&KERNEL_PROC_CAP_FULL_CONTEXT)&&proc_service->task_count()==2u&&proc_service->cooperate()&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==2u&&proc_service->dispatch_count(1u)==2u&&proc_service->context_self_test();if(proc_ready)h->console_puts("2 TASKS / FULL CTX RR OK\n");else if(state==MODULE_READY)h->console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char timer_name[11]={'T','I','M','E','R',' ',' ',' ','S','Y','S'};uint32_t before,guard,quantum;enum module_state state=kernel_load_system_module(timer_name,0x0104u,1);timer_service=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);timer_ready=proc_ready&&state==MODULE_READY&&timer_service&&timer_service->magic==KERNEL_TIMER_MAGIC&&timer_service->version>=KERNEL_TIMER_VERSION&&timer_service->size>=sizeof(*timer_service)&&timer_service->poll&&timer_service->ticks&&timer_service->switch_pending&&timer_service->acknowledge_switch&&timer_service->frequency_hz==50u&&proc_service->sleep_ticks(0u,2u)&&proc_service->sleep_remaining(0u)==2u;before=timer_ready?timer_service->ticks():0;for(quantum=0;timer_ready&&quantum<4u;++quantum){guard=2000000UL;while(guard--&&!timer_service->switch_pending())timer_service->poll();if(!timer_service->switch_pending()){timer_ready=0;break;}timer_service->acknowledge_switch();proc_service->timer_tick();if(timer_service->switch_pending()||!proc_service->cooperate())timer_ready=0;}if(timer_ready)timer_ready=timer_service->ticks()>=before+4u&&proc_service->sleep_remaining(0u)==0u&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==6u;h->console_puts("TIMER/SLEEP QUEUE: ");if(timer_ready)h->console_puts("4Q / WAKE OK\n");else if(state==MODULE_READY)h->console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char event_name[11]={'E','V','E','N','T',' ',' ',' ','S','Y','S'};uint32_t type=0,value=0;enum module_state state=kernel_load_system_module(event_name,0x0105u,1);const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);int ok=proc_ready&&state==MODULE_READY&&event&&event->magic==KERNEL_EVENT_MAGIC&&event->version>=KERNEL_EVENT_VERSION&&event->size>=sizeof(*event)&&event->receive&&event->send&&event->pending&&!event->receive(0u,&type,&value)&&proc_service->task_state(0u)==KERNEL_PROC_WAITING&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==5u&&proc_service->dispatch_count(1u)==7u&&event->send(0u,0x1234u,0x41535953UL)&&event->pending(0u)&&proc_service->task_state(0u)==KERNEL_PROC_READY&&event->receive(0u,&type,&value)&&!event->pending(0u)&&type==0x1234u&&value==0x41535953UL&&proc_service->cooperate()&&proc_service->dispatch_count(0u)==6u&&proc_service->dispatch_count(1u)==8u;h->console_puts("EVENT QUEUE: ");if(ok)h->console_puts("WAIT / WAKE OK\n");else if(state==MODULE_READY)h->console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
    if(fat_mounted){static const char input_name[11]={'I','N','P','U','T',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(input_name,0x0106u,1);input_service=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);input_ready=state==MODULE_READY&&input_service&&input_service->magic==KERNEL_INPUT_MAGIC&&input_service->version>=KERNEL_INPUT_VERSION&&input_service->size>=sizeof(*input_service)&&input_service->poll&&input_service->classify&&input_service->dropped_events&&input_service->classify(0x1eu)==KERNEL_INPUT_KEY_DOWN&&input_service->classify(0x9eu)==KERNEL_INPUT_KEY_UP&&input_service->classify(0xf8u)==0u&&input_service->dropped_events()==0u;h->console_puts("IKBD INPUT: ");if(input_ready)h->console_puts("POLLING READY\n");else if(state==MODULE_READY)h->console_puts("SELF-TEST FAILED\n");else if(state==MODULE_NOT_FOUND)h->console_puts("NOT FOUND\n");else if(state==MODULE_NO_MEMORY)h->console_puts("NO MEMORY\n");else if(state==MODULE_IO_FAILED)h->console_puts("I/O FAILED\n");else if(state==MODULE_INIT_FAILED)h->console_puts("INIT FAILED\n");else h->console_puts("INVALID FILE\n");}
        if(fat_mounted){static const char shell_name[11]={'S','H','E','L','L',' ',' ',' ','S','Y','S'};enum module_state state=kernel_load_system_module(shell_name,0x0107u,1);const kernel_shell_service_t *shell=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);shell_ready=state==MODULE_READY&&timer_ready&&input_ready&&shell&&shell->magic==KERNEL_SHELL_MAGIC&&shell->version>=KERNEL_SHELL_VERSION&&shell->size>=sizeof(*shell)&&shell->start&&shell->pump&&shell->key_count&&shell->redraw_prompt&&shell->line_active&&shell->key_count()==0u;h->console_puts(shell_ready?"KERNEL STATUS: RUNNING / PREEMPTIVE / SHELL READY\n":"KERNEL STATUS: SHELL UNAVAILABLE\n");
        h->console_puts("DMA GUARD: ");h->console_put_u32(kernel_dma_guard_hits);h->console_puts(" OVERRUN(S) CONTAINED BY STAGING\n");if(shell_ready){extern void tos_console_refresh(void);kernel_preemption_enable();kernel_startup_services(h,timer_service);shell->start();for(;;){timer_service->poll();if(timer_service->switch_pending()){timer_service->acknowledge_switch();proc_service->timer_tick();proc_service->cooperate();}for(i=0;i<64u;++i)input_service->poll(0u);for(i=0;i<32u;++i){int terminal_result;if(kernel_user_terminal_active()){terminal_result=kernel_user_terminal_wake();if(terminal_result>=2){(void)kernel_user_console_dirty();shell->redraw_prompt();}}else if(kernel_user_gui_keyboard_active())(void)kernel_user_gui_keyboard_wake();else shell->pump(0u);}kernel_notify_pending(h,shell);kernel_dispatch_ready_user();if(kernel_dispatch_output_user()&&!kernel_user_terminal_active()&&!kernel_user_gui_keyboard_active())shell->redraw_prompt();kernel_handle_dispatch_completion(h,shell);}}}
    h->console_puts("\nPRESS RESET");
    for(;;)__asm__ volatile("stop #0x2700");
}
