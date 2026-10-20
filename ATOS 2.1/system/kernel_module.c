#include "kernel_module.h"
#include "kernel_fat.h"
#include "kernel_partition.h"
#include "kernel_input.h"
#include "kernel_event.h"
#include "kernel_timer.h"
#include "kernel_proc.h"
#include "hardware.h"
#include "kernel_sessions.h"
#include "kernel_video.h"
#include "kernel_wm.h"
#include "kernel_graphics.h"
#include "tos_prg.h"
#include "kernel_block.h"
#include "kernel_shell.h"
#include "kernel_console.h"
#include "system_report.h"

#define ACSI_OWNER 0x0100u
#define MAX_SERVICES 24u
typedef struct {uint32_t id;uint16_t version,owner;const void *interface;} service_t;
static kernel_module_api_t module_api;
static service_t services[MAX_SERVICES];
static uint8_t service_count;
static uint16_t active_owner;
static const uint8_t *active_image;
static uint32_t active_memory_size;
static uint8_t module_cpu_type;
static uint32_t report_st_ram,report_tt_ram;
static uint8_t report_adapter,report_chip;
static uint8_t report_netusbee_ethernet,report_netusbee_usb;
static uint8_t module_fpu_type,module_machine_type,module_clock_mhz,module_clock_known;
/* Machine classification for drivers that must only touch mapped registers
   (VME/NVRAM pages are unmapped on plain ST/Mega ST and would bus-error). */
uint8_t kernel_machine_type(void){return module_machine_type;}
/* Each console keeps its own current drive; the FAT mount context follows it
   (kfat_select_session).  A set bit in session_drive_valid means the entry is
   known, so the first touch adopts the boot volume. */
static uint8_t session_drive[SESSION_COUNT];
static uint8_t session_drive_valid;
static void (*module_console_clear)(void);
static void (*module_console_puts)(const char *);
static void (*module_console_put_u32)(uint32_t);
extern void kernel_image_cache_sync(uint32_t cpu_type);
extern int32_t kernel_run_user(void *entry,void *stack_top,
                               const void *image_base,uint32_t image_size,
                               const char *working_directory);
extern void kernel_user_set_arguments(const char *arguments);
extern int kernel_user_suspend_parent(void);
extern uint32_t kernel_user_current_pid(void);
extern int kernel_user_process_info(uint32_t slot,uint32_t info[4]);
extern int kernel_user_process_name(uint32_t slot,char name[13]);
extern int kernel_user_kill(uint32_t pid);
extern uint32_t kernel_user_process_cpu_ticks(uint32_t slot);
extern uint32_t kernel_user_process_stack_used(uint32_t slot);
extern int kernel_user_process_map(uint32_t pid,uint32_t info[16]);
extern int kernel_exec_path_get(uint32_t index,char destination[64]);
extern int kernel_user_set_watchdog(uint32_t pid,uint32_t ticks);
extern int kernel_user_process_resources(uint32_t slot,uint32_t info[4]);
extern int kernel_user_foreground(uint32_t pid);
extern int kernel_user_resume(uint32_t pid);
extern int kernel_user_suspend(uint32_t pid);
extern int kernel_user_take_completion(uint32_t pid,int32_t *status);
extern void kernel_user_resume_parent(void);
extern int32_t kernel_user_create_loaded(void *,uint32_t,void *,uint32_t,uint32_t,
                                         const char *,const char *,const char *,uint16_t,uint16_t,uint32_t *);
static void *acsi_module_memory;
static int (*module_acsi_read_blocks)(uint8_t,uint32_t,uint16_t,void *);
static int (*fallback_acsi_read)(uint8_t,uint32_t,void *);
static int (*fallback_acsi_write)(uint8_t,uint32_t,const void *);
static const boot_handoff_t *module_services;

int kernel_module_floppy_mount(void)
{return module_services&&module_services->floppy_mount?module_services->floppy_mount():0;}
int kernel_module_floppy_dir(uint32_t index,char name[13],uint32_t *size,uint8_t *attr)
{return module_services&&module_services->floppy_dir?module_services->floppy_dir(index,name,size,attr):0;}
int kernel_module_floppy_read_file(const char *name,void *buffer,uint32_t capacity,uint32_t *size)
{return module_services&&module_services->floppy_read_file?module_services->floppy_read_file(name,buffer,capacity,size):0;}
static int register_acsi_read_blocks(int (*read_blocks)(uint8_t,uint32_t,uint16_t,void *));
static uint8_t user_shift_state;
static uint8_t user_mouse_buttons;
static const uint8_t user_key_plain[64]={0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,0,'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';',39,'`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0};
static const uint8_t user_key_shifted[64]={0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,0,'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0};
extern void kernel_ikbd_poll_capture(void);
static volatile uint8_t ikbd_raw_data[256],ikbd_raw_status[256];
static volatile uint8_t ikbd_raw_head,ikbd_raw_tail;
static volatile uint32_t ikbd_irq_dropped;

void kernel_ikbd_irq_reset(void)
{ikbd_raw_head=ikbd_raw_tail=0;ikbd_irq_dropped=0;}

void kernel_ikbd_irq_capture(void)
{
    uint8_t status=REG8(IKBD_ACIA_CTRL),next,data;
    if(!(status&1u))return;
    data=REG8(IKBD_ACIA_DATA);next=(uint8_t)(ikbd_raw_head+1u);
    if(next==ikbd_raw_tail){++ikbd_irq_dropped;return;}
    ikbd_raw_status[ikbd_raw_head]=status;ikbd_raw_data[ikbd_raw_head]=data;
    ikbd_raw_head=next;
}

static int32_t ikbd_raw_pop(void)
{
    uint8_t tail,status,data;
    if(ikbd_raw_tail==ikbd_raw_head)return -1;
    tail=ikbd_raw_tail;status=ikbd_raw_status[tail];data=ikbd_raw_data[tail];
    /* uint8_t wrap matches the full 256-entry ring. The former 64-entry mask
       replayed stale bytes 0..63 after every 64 consumed characters. */
    ikbd_raw_tail=(uint8_t)(tail+1u);
    return (int32_t)(((uint32_t)status<<8)|data);
}

static uint32_t ikbd_raw_dropped(void){return ikbd_irq_dropped;}
void kernel_user_terminal_reset_modifiers(void){user_shift_state=0;}
int32_t kernel_spawn_user_async(const char *path,const char *arguments,
                                const char *working_directory,uint32_t *child_pid);
typedef struct {char path[64],arguments[64],working_directory[64];uint32_t id;uint8_t result_slot,terminal;} async_job_t;
typedef struct {uint32_t id,owner_pid;int32_t status;uint8_t state;} async_result_t;
static async_job_t async_jobs[4];
static async_result_t async_results[4];
static uint8_t async_head,async_tail,async_count;
static uint32_t next_async_id=1u;
static int32_t kernel_dispatch_queued_user(void);
extern unsigned kernel_user_birth_terminal(void);
extern void kernel_user_assign_terminal(uint32_t,unsigned);
int kernel_queued_session_busy(unsigned t)
{unsigned i;for(i=0;i<async_count;i++)if(async_jobs[(async_head+i)&3u].terminal==t)return 1;return 0;}

static int get_cpu_info(kernel_cpu_info_t *info)
{
    if(!info)return 0;
    info->cpu_type=module_cpu_type;info->fpu_type=module_fpu_type;
    info->machine_type=module_machine_type;
    info->address_bits=module_cpu_type>=2u?32u:24u;
    info->has_mmu=module_cpu_type>=3u;
    info->has_instruction_cache=module_cpu_type>=2u;
    info->has_data_cache=module_cpu_type>=3u;
    info->clock_known=module_clock_known;info->clock_mhz=module_clock_mhz;
    info->reserved[0]=info->reserved[1]=info->reserved[2]=0;return 1;
}

int kernel_user_keyboard_event(uint32_t *event_type,uint32_t *scan_code,
                               uint32_t *modifiers,uint32_t *character)
{
    const kernel_input_service_t *input=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);
    const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);
    uint32_t type=0,value=0,scan=0,ascii=0;int got=0;
    if(!event_type||!scan_code||!modifiers||!character)return 0;
    if(input&&event&&input->poll&&event->pending&&event->receive){
        input->poll(0u);
        if(event->pending(0u)&&event->receive(0u,&type,&value)){scan=value&0x7fu;got=1;}
    }
    /* INPUT.SYS exclusively owns the raw byte stream and mouse framing.
       An empty key queue is normal while a mouse packet is arriving. */
    if(!got)return 0;
    if(scan==0x1du){if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=4u;else user_shift_state&=(uint8_t)~4u;}
    else if(scan==0x2au||scan==0x36u){uint8_t bit=scan==0x2au?1u:2u;
        if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=bit;else user_shift_state&=(uint8_t)~bit;}
    else if(scan==0x38u){if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=8u;
        else user_shift_state&=(uint8_t)~8u;}
    else if(type==KERNEL_INPUT_KEY_DOWN&&scan<64u){
        if(user_shift_state&4u){if(scan==0x2eu)ascii=3u;else if(scan==0x2cu)ascii=26u;}
        else ascii=(user_shift_state&3u)?user_key_shifted[scan]:user_key_plain[scan];}
    *event_type=type;*scan_code=scan;*modifiers=user_shift_state;*character=ascii;
    /* Ctrl+Alt+Esc = kill-switch programu TOS, niezaleznie od tego, kto go
       uruchomil.  Wczesniej dzialal tylko przez pulpit (zamkniecie okna ->
       kernel_tos_window_closed), a przy starcie z KONSOLI kombinacja trafiala
       do programu jako zwykle ESC - SYSINFO pokazywal "Exit SysInfo ?" i test
       nie mogl wrocic do powloki.  Zdarzenie jest tu pochlaniane (return 0),
       zeby ESC nie dotarlo do programu. */
    if(type==KERNEL_INPUT_KEY_DOWN&&scan==0x01u&&(user_shift_state&12u)==12u){
        extern int kernel_tos_running(void);
        extern void kernel_tos_request_exit(void);
        if(kernel_tos_running()){kernel_tos_request_exit();return 0;}
    }
    /* Shift+Alt+F10 must switch sessions even when a TOS GUI owns the keyboard
       (the shell module only handles it when the shell has focus). */
    return 1;
}

int kernel_raw_mouse_event(uint32_t *buttons,int32_t *delta_x,int32_t *delta_y)
{
    const kernel_input_service_t *input=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);
    const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);
    uint32_t type,value;
    if(!buttons||!delta_x||!delta_y||!input||!event||!input->poll||
       !event->pending||!event->receive)return 0;
    input->poll(0u);
    /* Keep packet order: an edge belongs to its position, never to the end
       of a later motion burst. Repeated held-button packets are not presses. */
    while(event->pending(1u)&&event->receive(1u,&type,&value)){
        uint32_t down,pressed;
        if(type!=KERNEL_INPUT_MOUSE_PACKET)continue;
        /* The IKBD mouse status byte uses bit0 = RIGHT, bit1 = LEFT.  ATOS -
           and the scripted injection hook - use bit0 = LEFT (the primary
           button) everywhere, so swap the two bits here.  Without this the
           primary button was ignored (its bit is not the one the compositor
           and the applications test) and the secondary button behaved as the
           primary: "clicking FILE does nothing" on real hardware. */
        down=(value>>16)&3u;
        down=((down&1u)<<1)|((down>>1)&1u);
        pressed=down&~user_mouse_buttons;
        user_mouse_buttons=(uint8_t)down;
        *buttons=down|(pressed?(4u|(pressed<<3)):0u);
        *delta_x=(int32_t)(int8_t)(value>>8);
        *delta_y=(int32_t)(int8_t)value;
        return 1;
    }
    return 0;
}

int kernel_user_mouse_event(uint32_t *b,int32_t *x,int32_t *y)
{return ksessions_mouse_event(b,x,y);}

void kernel_user_mouse_flush(void)
{
    ksessions_mouse_flush();
    const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);
    uint32_t type,value;
    user_mouse_buttons=0;
    if(!event||!event->pending||!event->receive)return;
    while(event->pending(1u))if(!event->receive(1u,&type,&value))break;
}

int32_t kernel_user_terminal_getchar(void)
{
    uint32_t type,scan,modifiers,character;
    if(!kernel_user_keyboard_event(&type,&scan,&modifiers,&character))return -1;
    (void)scan;(void)modifiers;
    return type==KERNEL_INPUT_KEY_DOWN&&character?(int32_t)character:-1;
}

/* Scheduler clock also runs when every user process is blocked. */
uint32_t kernel_user_clock(void)
{
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    if(!timer||!timer->poll||!timer->ticks)return 0;
    timer->poll();return timer->ticks();
}

/* Zegar "scienny" dla zabezpieczen, ktore MUSZA wygasnac w czasie: tykniecia
 * liczy przerwanie Timer-C (kernel_entry.S -> kernel_timer_ticks), wiec nie
 * gubi okresow, gdy petla jadra zwalnia.
 *
 * kernel_user_clock() czyta TEN SAM timer przez polling 8-bitowego licznika
 * (timer_module.S, timer_poll): miedzy dwoma odczytami wolno wykryc tylko
 * JEDNO zawiniecie.  Zmierzone w trybie-smieciach 4 bpp: 23 Hz zamiast 50 Hz,
 * czyli straznik "4 s" wygasal po 8-14 s.  Dla zegara scheduler-a to bez
 * znaczenia (i tak chodzi czesto), ale dla straznika zmiany trybu - kluczowe. */
uint32_t kernel_user_wall_clock(void)
{
    extern volatile uint32_t kernel_timer_ticks;
    return kernel_timer_ticks;
}

/* Timer-C IRQs count elapsed periods even while applications are blocked.
 * Polling the 8-bit countdown alone loses whole periods between samples.
 * A pending level-6 IRQ is included when called with IPL6 (e.g. from the
 * scheduler); retry if a reload or IRQ races the countdown snapshot. */
uint32_t kernel_user_hires_clock(void)
{
    extern volatile uint32_t kernel_timer_ticks;
    static uint32_t last;
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    uint32_t ticks,value;uint8_t before,after,pending;
    if(!timer||!timer->frequency_hz)return 0;
    do {
        ticks=kernel_timer_ticks;
        before=*(volatile uint8_t *)(uintptr_t)MFP_TCDR;
        pending=*(volatile uint8_t *)(uintptr_t)0xfffffa0du;
        after=*(volatile uint8_t *)(uintptr_t)MFP_TCDR;
    } while(ticks!=kernel_timer_ticks||after>before);
    if(pending&0x20u)++ticks;
    if(after>246u)after=246u;
    value=ticks*246u+(uint32_t)(246u-after);
    /* A caller interrupted by a newer sample must not move time backwards.
       Signed subtraction also permits the normal 32-bit clock wrap. */
    if((int32_t)(value-last)<0)value=last;
    last=value;
    return value;
}

int32_t kernel_user_yield(void)
{
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    const kernel_proc_service_t *proc=(const kernel_proc_service_t *)kernel_find_service(KERNEL_SERVICE_PROC,KERNEL_PROC_VERSION);
    {extern uint32_t kdbg_yield_calls;++kdbg_yield_calls;}
    /* A GUI command line may be waiting inside pause_tick(): this yield path is
       the only supervisor code that runs then, so the queued shell line is
       executed here too (run_pending clears the flag before executing, so the
       nested yields of the command itself are no-ops). */
    {const kernel_shell_service_t *sh=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);
     if(sh&&sh->run_pending&&sh->run_pending())kconsole_capture_end();}
    (void)kernel_dispatch_queued_user();
    if(!timer||!proc||!timer->poll||!timer->ticks||!timer->switch_pending||
       !timer->acknowledge_switch||!proc->timer_tick||!proc->cooperate)return -1;
    timer->poll();
    if(timer->switch_pending()){timer->acknowledge_switch();proc->timer_tick();}
    if(!proc->cooperate()){extern uint32_t kdbg_yield_bad;++kdbg_yield_bad;return -1;}
    {extern uint32_t kdbg_yield_ok;++kdbg_yield_ok;}
    kernel_wm_frame_tick(timer->ticks());        /* at most one composite per tick */
    return (int32_t)timer->ticks();
}

int32_t kernel_user_sleep(uint32_t ticks)
{
    int32_t start,current;
    if(ticks>3000u)return -1;
    start=kernel_user_yield();
    if(start<0)return -1;
    do {
        current=kernel_user_yield();
        if(current<0)return -1;
    } while((uint32_t)(current-start)<ticks);
    return current;
}

int32_t kernel_user_system_info(uint32_t *info)
{
    kmem_stats_t st,tt;
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    if(!info||!timer||!timer->poll||!timer->ticks)return -1;
    timer->poll();
    kmem_get_st_stats(&st);kmem_get_tt_stats(&tt);
    info[0]=1u;                 /* user syscall ABI version */
    info[1]=67u;                /* highest implemented syscall */
    info[2]=module_cpu_type;
    info[3]=timer->ticks();
    info[4]=st.free;
    info[5]=tt.free;
    return 0;
}

int kernel_system_report(system_report_t *out)
{
 kmem_stats_t st,tt;const kgfx_info_t *g=kgfx_get_info();unsigned i,j;
 uint8_t *p=(uint8_t *)out;for(i=0;i<sizeof(*out);i++)p[i]=0;
 kmem_get_st_stats(&st);kmem_get_tt_stats(&tt);
 out->magic=SYSREPORT_MAGIC;out->version=SYSREPORT_VERSION;out->size=sizeof(*out);
 out->machine=module_machine_type;out->cpu=module_cpu_type;out->fpu=module_fpu_type;
 out->clock_mhz=module_clock_mhz;out->clock_source=(module_clock_known<=2u)?module_clock_known:(module_clock_known?1u:0u);
 if(!out->clock_source&&module_cpu_type==0&&module_machine_type<=2){out->clock_mhz=8;out->clock_source=2;}
 out->st_total=report_st_ram;out->tt_total=report_tt_ram;
 out->st_free=st.free;out->st_largest=st.largest;out->tt_free=tt.free;out->tt_largest=tt.largest;
 out->tt_reserved=tt.reserved;
 out->hardware_report=(report_adapter&128)!=0;out->detected_adapter=report_adapter&127;
 out->detected_chip=report_chip;out->active_video=kvideo_is_vofa()?1:0;
 out->width=g->width;out->height=g->height;out->planes=g->planes;out->blitter=g->capabilities;
 out->output_width=g->width;out->output_height=kvideo_is_vofa()?480:g->height;
 out->bus=1;out->device=kblock_device();
 if(module_services&&module_services->disk_known){
  /* The ROM already queried the boot device (ACSI INQUIRY / ATA IDENTIFY); the
     kernel's own SCSI diagnostic path cannot talk to a TF536 IDE disk. */
  for(i=0;i<sizeof(out->disk_name);i++)out->disk_name[i]=module_services->disk_name[i];
  out->disk_name[sizeof(out->disk_name)-1u]=0;
  out->disk_sectors=module_services->disk_sectors;
  out->sector_bytes=module_services->disk_sector_bytes;
 }else{
  kblock_identify(out->disk_name,&out->disk_sectors,&out->sector_bytes);
 }
 out->disk_capacity_known=out->disk_sectors!=0;
 out->cache_sectors=kblock_cache_sectors();out->cache_tt=kblock_cache_is_tt();
 out->netusbee_ethernet=report_netusbee_ethernet;out->netusbee_usb=report_netusbee_usb;
 out->volumes=kpart_count();if(out->volumes>SYSREPORT_VOLUMES)out->volumes=SYSREPORT_VOLUMES;
 for(i=0;i<out->volumes;i++){
  const char *id=kpart_id((uint8_t)i);out->volume[i].start=kpart_start((uint8_t)i);
  out->volume[i].sectors=kpart_size((uint8_t)i);out->volume[i].boot=kpart_is_boot((uint8_t)i);
  for(j=0;j<3;j++)out->volume[i].id[j]=id[j];
  if(out->volume[i].boot){out->boot_start=out->volume[i].start;out->boot_sectors=out->volume[i].sectors;}
 }
 kernel_wm_stats(&out->wm_frames,&out->wm_damage_pixels,&out->wm_console_cells);
 kernel_wm_request_stats(&out->wm_requests_total,&out->wm_requests_last,&out->wm_requests_max);
 {extern uint32_t kmem_list_walks;extern uint32_t kblock_search_count(void);
  out->mem_list_walks=kmem_list_walks;out->blk_searches=kblock_search_count();}
 out->tick_hz=50;out->wall_ticks=kernel_user_clock();return 0;
}

static int system_file_size(const char name[11],uint32_t *size)
{(void)kfat_select_session(ksessions_current());return kfat_find_system_file(name,size)==FAT_FILE_FOUND;}

static int load_system_file(const char name[11],void *destination,
                            uint32_t capacity,uint32_t *size)
{(void)kfat_select_session(ksessions_current());return kfat_load_system_file(name,destination,capacity,size)==FAT_FILE_FOUND;}

static uint8_t boot_drive_index(void)
{uint8_t i;for(i=0;i<kpart_count();++i)if(kpart_is_boot(i))return i;return 0;}

static uint8_t session_current_drive(void)
{
    unsigned s=ksessions_current();
    if(!(session_drive_valid&(1u<<s))){session_drive[s]=boot_drive_index();session_drive_valid|=1u<<s;}
    return session_drive[s];
}
static uint32_t boot_drive_letter(void)
{return (uint32_t)('C'+session_current_drive());}

uint32_t kernel_user_drive_letter(void)
{return boot_drive_letter();}

/* Change only the calling console's volume: the mount context is per session,
   so another console can stay on its own drive (the old code returned 0 as
   soon as more than one session was active). */
static int select_drive(uint32_t letter)
{
    uint8_t index,old;unsigned s=ksessions_current();
    if(letter>='a'&&letter<='z')letter=letter-'a'+'A';
    if(letter<(uint32_t)'C'||letter>=(uint32_t)'C'+kpart_count())return 0;
    index=(uint8_t)(letter-'C');
    if(!kpart_accessible(index))return 0;
    old=session_current_drive();
    if(index==old)return 1;
    (void)kfat_select_session(s);
    /* volume may be on another transport.  The select must succeed first: a
       failed one leaves the block layer not-ready, so the mount below would
       report an I/O error while the console was already deselected from its
       previous drive.  Short-circuiting keeps the rollback the only failure
       path and leaves the old volume mounted and reachable. */
    if(kblock_select(kpart_device(index))&&
       kfat_mount(kpart_start(index),kpart_id(index))==FAT_MOUNTED){
        session_drive[s]=index;session_drive_valid|=1u<<s;return 1;}
    (void)kblock_select(kpart_device(old));
    (void)kfat_mount(kpart_start(old),kpart_id(old));
    return 0;
}

static int stat_path(const char *path,uint32_t *size,uint8_t *attributes)
{(void)kfat_select_session(ksessions_current());return kfat_stat_path(path,size,attributes)==FAT_FILE_FOUND;}

static int read_path(const char *path,void *destination,uint32_t capacity,uint32_t *size)
{(void)kfat_select_session(ksessions_current());return kfat_load_path(path,destination,capacity,size)==FAT_FILE_FOUND;}

static int list_path(const char *path,kfat_list_callback_t callback,void *context)
{(void)kfat_select_session(ksessions_current());return kfat_list_path(path,callback,context)==FAT_FILE_FOUND;}

static int read_path_at(const char *path,uint32_t offset,void *destination,
                        uint32_t capacity,uint32_t *size)
{(void)kfat_select_session(ksessions_current());return kfat_read_path_at(path,offset,destination,capacity,size)==FAT_FILE_FOUND;}

/* Filesystem mutations act on the calling console's own mount. */
static int copy_path(const char *source,const char *target)
{(void)kfat_select_session(ksessions_current());return kfat_copy_path(source,target);}
static int mkdir_path(const char *path)
{(void)kfat_select_session(ksessions_current());return kfat_mkdir_path(path);}
static int delete_path(const char *path)
{(void)kfat_select_session(ksessions_current());return kfat_delete_path(path);}
static int move_path(const char *source,const char *target)
{(void)kfat_select_session(ksessions_current());return kfat_move_path(source,target);}

static void sync_instruction_cache(void)
{kernel_image_cache_sync(module_cpu_type);}

void kernel_sync_user_image(void)
{kernel_image_cache_sync(module_cpu_type);}

const void *kernel_find_service(uint32_t id,uint16_t minimum_version)
{
    uint8_t i;for(i=0;i<service_count;++i)
        if(services[i].id==id&&services[i].version>=minimum_version)return services[i].interface;
    return 0;
}

static int register_service(uint32_t id,uint16_t version,const void *interface)
{
    uint8_t i;const uint8_t *p=(const uint8_t *)interface;
    if(!id||!version||!interface||!active_owner||!active_image||
       p<active_image||p>=active_image+active_memory_size||service_count>=MAX_SERVICES)return 0;
    for(i=0;i<service_count;++i)if(services[i].id==id)return 0;
    services[service_count].id=id;services[service_count].version=version;
    services[service_count].owner=active_owner;services[service_count].interface=interface;
    ++service_count;return 1;
}
/* Kernel-published service: same table, but not tied to a module load window.
   register_service() above requires active_owner/active_image (it validates that
   the interface lives inside the module image being loaded), so the kernel
   cannot use it for its own interfaces. */
int kernel_publish_service(uint32_t id,uint16_t version,const void *interface)
{
    uint8_t i;
    if(!id||!version||!interface||service_count>=MAX_SERVICES)return 0;
    for(i=0;i<service_count;++i)if(services[i].id==id)return 0;
    services[service_count].id=id;services[service_count].version=version;
    services[service_count].owner=0u;services[service_count].interface=interface;
    ++service_count;return 1;
}

static void prepare_module_api(void)
{
    module_api.magic=KERNEL_MODULE_API_MAGIC;
    module_api.abi_version=MODULE_ABI_VERSION;
    module_api.size=sizeof(module_api);
    module_api.alloc=kmem_alloc;
    module_api.free=kmem_free;
    module_api.bios_acsi_read=fallback_acsi_read;
    module_api.register_acsi_read_blocks=register_acsi_read_blocks;
    module_api.register_service=register_service;
    module_api.find_service=kernel_find_service;
    module_api.system_file_size=system_file_size;
    module_api.load_system_file=load_system_file;
    module_api.console_clear=module_console_clear;
    module_api.console_puts=module_console_puts;
    module_api.console_put_u32=module_console_put_u32;
    module_api.get_st_stats=kmem_get_st_stats;
    module_api.get_tt_stats=kmem_get_tt_stats;
    module_api.boot_drive_letter=boot_drive_letter;
    module_api.stat_path=stat_path;
    module_api.read_path=read_path;
    module_api.list_path=list_path;
    module_api.read_path_at=read_path_at;
    module_api.sync_instruction_cache=sync_instruction_cache;
    module_api.select_drive=select_drive;
    module_api.run_user=kernel_run_user;
    module_api.set_user_arguments=kernel_user_set_arguments;
    module_api.spawn_user_async=kernel_spawn_user_async;
    module_api.user_process_info=kernel_user_process_info;
    module_api.user_process_name=kernel_user_process_name;
    module_api.kill_user_process=kernel_user_kill;
    module_api.user_process_resources=kernel_user_process_resources;
    module_api.foreground_user_process=kernel_user_foreground;
    module_api.resume_user_process=kernel_user_resume;
    module_api.suspend_user_process=kernel_user_suspend;
    module_api.take_user_completion=kernel_user_take_completion;
    module_api.user_process_cpu_ticks=kernel_user_process_cpu_ticks;
    module_api.set_user_watchdog=kernel_user_set_watchdog;
    module_api.get_cpu_info=get_cpu_info;
    module_api.user_process_stack_used=kernel_user_process_stack_used;
    module_api.ikbd_raw_pop=ikbd_raw_pop;
    module_api.ikbd_raw_dropped=ikbd_raw_dropped;
    module_api.user_process_map=kernel_user_process_map;
    module_api.get_exec_path=kernel_exec_path_get;
    module_api.terminal_control=ksessions_control;
    module_api.terminal_key=ksessions_key;
    module_api.ikbd_capture=kernel_ikbd_poll_capture;
    module_api.floppy_mount=kernel_module_floppy_mount;
    module_api.floppy_dir=kernel_module_floppy_dir;
    module_api.floppy_read_file=kernel_module_floppy_read_file;
    module_api.tos_run=kernel_tos_run_program;
    module_api.copy_path=copy_path;
    module_api.mkdir_path=mkdir_path;
    module_api.delete_path=delete_path;
    module_api.move_path=move_path;
}

static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

/* Native application limits.  ABI 1.0/1.1 programs are far below them; 1.2
   raised them for large ports (FLASHBK.APP is ~0.5 MB of code and tables):
   file and in-memory image up to 1 MB / 2 MB (both are allocated preferring
   fast RAM) and a runtime stack of up to 64 KB. */
#define APP_IMAGE_MAX_FILE   (1024u*1024u)
#define APP_IMAGE_MAX_MEMORY (2048u*1024u)
#define APP_STACK_MAX        (64u*1024u)
static int verify_app_image(const uint8_t *image,uint32_t file_size)
{
    static const char magic[8]={'A','S','Y','S','A','P','P','0'};
    uint32_t i,sum=0,memory,entry,stack,executable_end,writable_start;
    if(!image||file_size<44u)return 0;
    for(i=0;i<8u;++i)if(image[i]!=(uint8_t)magic[i])return 0;
    if(be16(image+8)!=1u||be16(image+10)!=32u||be32(image+12)!=file_size)return 0;
    memory=be32(image+16);entry=be32(image+20);stack=be32(image+24);
    if(memory<file_size||memory>APP_IMAGE_MAX_MEMORY||entry<32u||entry>=file_size||stack<1024u||stack>APP_STACK_MAX||(stack&1u))return 0;
    if(be32(image+file_size-12u)!=0x41505732u)return 0;
    executable_end=be32(image+file_size-8u);
    writable_start=be32(image+file_size-4u);
    if(executable_end<=32u||executable_end>writable_start||
       writable_start>file_size-12u||entry>=executable_end)return 0;
    for(i=0;i<file_size;++i)if(i<28u||i>=32u)sum+=image[i];
    return sum==be32(image+28);
}

int32_t kernel_spawn_user(const char *path,const char *arguments,
                          const char *working_directory,uint32_t *child_pid)
{
    /* The old foreground loader returned to its caller at the first scheduler
       quantum, which made the loader free a still-live image and stack.  Use
       the atomic loaded-process path for RUN as well; foreground/background
       policy belongs to the shell and terminal owner, not to allocation. */
    return kernel_spawn_user_async(path,arguments,working_directory,child_pid);
}

int32_t kernel_spawn_user_async(const char *path,const char *arguments,
                                const char *working_directory,uint32_t *child_pid)
{
    enum fat_file_state state;uint32_t file_size=0,loaded=0,memory_size,stack_size,i,entry;
    uint8_t attributes=0,*image=0,*stack=0;int32_t result=-1;
    if(child_pid)*child_pid=0;
    state=kfat_stat_path(path,&file_size,&attributes);
    if(state!=FAT_FILE_FOUND||(attributes&0x10u)||file_size<32u||file_size>APP_IMAGE_MAX_FILE)return -1;
    image=(uint8_t *)kmem_alloc(file_size,16u,0x0410u,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,1);
    if(!image)return -2;
    if(kfat_load_path(path,image,file_size,&loaded)!=FAT_FILE_FOUND||loaded!=file_size||!verify_app_image(image,file_size)){result=-3;goto done;}
    memory_size=be32(image+16);stack_size=be32(image+24);entry=be32(image+20);
    if(memory_size>file_size){uint8_t *expanded=(uint8_t *)kmem_alloc(memory_size,16u,0x0410u,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,1);
        if(!expanded){result=-2;goto done;}for(i=0;i<file_size;++i)expanded[i]=image[i];for(;i<memory_size;++i)expanded[i]=0;
        kmem_free(image,0x0410u);image=expanded;}
    stack=(uint8_t *)kmem_alloc(stack_size+8u,16u,0x0411u,
                               KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);
    if(!stack){result=-2;goto done;}
    kernel_image_cache_sync(module_cpu_type);
    result=kernel_user_create_loaded(image,memory_size,stack,stack_size,
        (uint32_t)(uintptr_t)(image+entry),working_directory,arguments,path,
        0x0410u,0x0411u,child_pid);
    if(result==0){image=0;stack=0;}
done:
    if(stack)kmem_free(stack,0x0411u);
    if(image)kmem_free(image,0x0410u);
    return result;
}

int32_t kernel_queue_user(const char *path,const char *arguments,
                          const char *working_directory,uint32_t *job_id)
{
    async_job_t *job;uint8_t i,result_slot;
    if(job_id)*job_id=0;
    if(!path||!path[0]||async_count>=4u)return -0x321;
    for(result_slot=0;result_slot<4u&&async_results[result_slot].state;++result_slot){}
    if(result_slot==4u)return -0x323;
    job=&async_jobs[async_tail];
    for(i=0;i<63u&&path[i];++i)job->path[i]=path[i];
    job->path[i]='\0';
    if(path[i])return -0x322;
    for(i=0;i<63u&&arguments&&arguments[i];++i)job->arguments[i]=arguments[i];
    job->arguments[i]='\0';
    for(i=0;i<63u&&working_directory&&working_directory[i];++i)job->working_directory[i]=working_directory[i];
    job->working_directory[i]='\0';if(!i){job->working_directory[0]='\\';job->working_directory[1]='\0';}
    job->id=next_async_id++;if(!next_async_id)next_async_id=1u;
    job->result_slot=result_slot;job->terminal=(uint8_t)kernel_user_birth_terminal();
    async_results[result_slot].id=job->id;
    async_results[result_slot].owner_pid=kernel_user_current_pid();
    async_results[result_slot].status=0;
    async_results[result_slot].state=1u;
    if(job_id)*job_id=job->id;
    async_tail=(uint8_t)((async_tail+1u)&3u);++async_count;return 0;
}

static int32_t kernel_dispatch_queued_user(void)
{
    async_job_t job;uint32_t child_pid=0;uint16_t i;int32_t status;
    if(!async_count)return 0;
    for(i=0;i<64u;++i){job.path[i]=async_jobs[async_head].path[i];
        job.arguments[i]=async_jobs[async_head].arguments[i];
        job.working_directory[i]=async_jobs[async_head].working_directory[i];}
    job.id=async_jobs[async_head].id;
    job.result_slot=async_jobs[async_head].result_slot;
    job.terminal=async_jobs[async_head].terminal;
    async_head=(uint8_t)((async_head+1u)&3u);--async_count;
    async_results[job.result_slot].state=2u;
    status=kernel_spawn_user(job.path,job.arguments,job.working_directory,&child_pid);
    if(!status)kernel_user_assign_terminal(child_pid,job.terminal);
    async_results[job.result_slot].status=status;
    async_results[job.result_slot].state=3u;
    return 1;
}

int32_t kernel_query_user(uint32_t job_id,int reap,uint32_t *state,int32_t *status)
{
    uint8_t i;uint32_t owner=kernel_user_current_pid();
    if(!job_id||!state||!status||reap<0||reap>1)return -0x330;
    for(i=0;i<4u;++i)if(async_results[i].state&&async_results[i].id==job_id){
        if(async_results[i].owner_pid!=owner)return -0x332;
        *state=async_results[i].state;*status=async_results[i].status;
        if(reap&&async_results[i].state==3u){async_results[i].state=0;
            async_results[i].id=0;async_results[i].owner_pid=0;}
        return 0;
    }
    return -0x331;
}

int32_t kernel_wait_user(uint32_t job_id,int32_t *status)
{
    uint8_t i;uint32_t owner=kernel_user_current_pid();
    if(!job_id||!status)return -0x340;
    for(;;){
        for(i=0;i<4u;++i)if(async_results[i].state&&async_results[i].id==job_id)break;
        if(i==4u)return -0x341;
        if(async_results[i].owner_pid!=owner)return -0x342;
        if(async_results[i].state==3u){*status=async_results[i].status;
            async_results[i].state=0;async_results[i].id=0;async_results[i].owner_pid=0;return 0;}
        if(async_results[i].state==2u)return -0x343;
        if(!kernel_dispatch_queued_user())return -0x344;
    }
}

int kernel_verify_module_image(const void *image,uint32_t size)
{
    static const char magic[8]={'A','S','Y','S','M','O','D','0'};const uint8_t *p=(const uint8_t *)image;uint32_t i,sum=0,entry,memory;
    if(!p||size<BOOT_IMAGE_HEADER_SIZE)return 0;
    for(i=0;i<8u;++i)if(p[i]!=(uint8_t)magic[i])return 0;
    if(be16(p+8)!=MODULE_ABI_VERSION||be16(p+10)!=BOOT_IMAGE_HEADER_SIZE||be32(p+12)!=size||be32(p+16)!=0)return 0;
    entry=be32(p+20);memory=be32(p+24);if(entry<BOOT_IMAGE_HEADER_SIZE||entry>=size||memory<size||memory>256UL*1024UL)return 0;
    for(i=0;i<size;++i){
        if(i<28u||i>=32u)sum+=p[i];
    }
    return sum==be32(p+28);
}

static enum module_state load_module_image(int driver,const char name[11],uint16_t owner,
                                           int prefer_tt)
{
    uint32_t file_size=0,loaded_size=0,memory_size,entry_offset,i;uint8_t old_service_count;
    uint8_t *image;uint32_t (*entry)(const kernel_module_api_t *);
    enum fat_file_state state=driver?kfat_find_driver_file(name,&file_size)
                                   :kfat_find_system_file(name,&file_size);
    if(state==FAT_FILE_NOT_FOUND)return MODULE_NOT_FOUND;
    if(state==FAT_FILE_IO_FAILED)return MODULE_IO_FAILED;
    if(state!=FAT_FILE_FOUND||file_size<BOOT_IMAGE_HEADER_SIZE)return MODULE_INVALID;
    image=(uint8_t *)kmem_alloc(file_size,16u,owner,
        KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,prefer_tt);
    if(!image)return MODULE_NO_MEMORY;
    state=driver?kfat_load_driver_file(name,image,file_size,&loaded_size)
                :kfat_load_system_file(name,image,file_size,&loaded_size);
    if(state!=FAT_FILE_FOUND||loaded_size!=file_size||
       !kernel_verify_module_image(image,loaded_size)){
        kmem_free(image,owner);return state==FAT_FILE_IO_FAILED?MODULE_IO_FAILED:MODULE_INVALID;
    }
    memory_size=be32(image+24);entry_offset=be32(image+20);
    if(memory_size>file_size){
        uint8_t *expanded=(uint8_t *)kmem_alloc(memory_size,16u,owner,
            KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,prefer_tt);
        if(!expanded){kmem_free(image,owner);return MODULE_NO_MEMORY;}
        for(i=0;i<file_size;++i)expanded[i]=image[i];
        for(;i<memory_size;++i)expanded[i]=0;
        kmem_free(image,owner);image=expanded;
    }
    prepare_module_api();old_service_count=service_count;active_owner=owner;
    active_image=image;active_memory_size=memory_size;
    kernel_image_cache_sync(module_cpu_type);
    entry=(uint32_t (*)(const kernel_module_api_t *))(image+entry_offset);
    if(!entry(&module_api)){service_count=old_service_count;active_owner=0;active_image=0;
        active_memory_size=0;kmem_free(image,owner);return MODULE_INIT_FAILED;}
    active_owner=0;active_image=0;active_memory_size=0;
    return MODULE_READY;
}


enum module_state kernel_load_system_module(const char name[11],uint16_t owner,int prefer_tt)
{return load_module_image(0,name,owner,prefer_tt);}
/* Graphics (and other) drivers live in \SYSTEM\DRV, not in \SYSTEM flat. */
enum module_state kernel_load_driver_module(const char name[11],uint16_t owner,int prefer_tt)
{return load_module_image(1,name,owner,prefer_tt);}

/* --- TOS compatibility module ----------------------------------------------
   The personality itself is compiled into the kernel, but its working set (the
   private screen it renders into) is owned by TOS.SYS, loaded from \SYSTEM on
   demand: when a TOS program is started, or right after START when the user
   ticked "Load TOS Compatibility after START to TT RAM" - then prefer_tt=1 puts
   both the module image and its buffer in TT RAM.  Keeping this out of the boot
   path is what keeps startup short. */
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    uint32_t prefer_tt;      /* 1: the module should place its buffers in TT RAM */
    uint32_t reserved;
} kernel_tos_cfg_t;
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    uint32_t screen;         /* the private TOS screen buffer */
    uint32_t reserved;
} kernel_tos_module_service_t;

static kernel_tos_cfg_t tos_cfg_service;      /* static: the service table keeps the pointer */
static uint8_t tos_compat_ready;
static const kernel_tos_module_service_t *tos_compat_service;
static uint8_t *tos_compat_screen;

/* Diagnostics: the module test must be able to prove that the persona really
   adopted the module's buffer, and that no second allocation happened.  These
   are read from the running guest by the scenario. */
uint32_t kdbg_tos_compat_loads;      /* successful module loads (must be 1) */
uint32_t kdbg_tos_screen_fallbacks;  /* times the persona allocated its own screen */
uint8_t *kdbg_tos_module_screen;     /* buffer the module published */
uint32_t kdbg_tos_adopted_module;    /* times the persona used the module buffer */

int kernel_tos_compat_loaded(void){return tos_compat_ready?1:0;}
const void *kernel_tos_compat_service(void){return tos_compat_service;}
uint8_t *kernel_tos_compat_screen(void){return tos_compat_screen;}

/* Load the TOS compatibility module.  Returns 1 when it is ready, or a NEGATIVE
   error code.  MODULE_NOT_FOUND is 0, so the enum value itself can never be
   returned: every failure below is mapped onto a distinct negative code. */
int32_t kernel_tos_compat_ensure(int prefer_tt)
{
    static const char tos_name[11]={'T','O','S',' ',' ',' ',' ',' ','S','Y','S'};
    enum module_state state;
    /* Publishing is a ONE-TIME action.  kernel_publish_service() refuses a
       duplicate id, so re-publishing on a later attempt made a retry fail with
       -110 ("interface error") BEFORE the module was ever looked at again - a
       transient failure (no memory, full service table) poisoned TOS support
       for the rest of the session and hid the real cause. */
    static uint8_t tos_cfg_published;
    if(tos_compat_ready)return 1;
    /* The placement request is refreshed on every attempt. */
    tos_cfg_service.magic=KERNEL_SERVICE_TOS_CFG;
    tos_cfg_service.version=1u;
    tos_cfg_service.size=(uint16_t)sizeof(tos_cfg_service);
    tos_cfg_service.prefer_tt=(uint32_t)(prefer_tt?1:0);
    tos_cfg_service.reserved=0u;
    if(!tos_cfg_published){
        if(!kernel_publish_service(KERNEL_SERVICE_TOS_CFG,1u,&tos_cfg_service))return -110;
        tos_cfg_published=1u;
    }
    state=kernel_load_system_module(tos_name,KMEM_OWNER_KERNEL,prefer_tt);
    if(state!=MODULE_READY)return -(int32_t)state-100;   /* -100..-105, never 0 */
    tos_compat_service=(const kernel_tos_module_service_t *)kernel_find_service(KERNEL_SERVICE_TOS,1u);
    if(!tos_compat_service)return -120;
    if(tos_compat_service->magic!=KERNEL_SERVICE_TOS)return -121;
    if(tos_compat_service->size<(uint16_t)sizeof(*tos_compat_service))return -122;
    if(!tos_compat_service->screen)return -123;
    tos_compat_screen=(uint8_t *)(uintptr_t)tos_compat_service->screen;
    kdbg_tos_module_screen=tos_compat_screen;
    tos_compat_ready=1u;
    ++kdbg_tos_compat_loads;
    return 1;
}
static int register_acsi_read_blocks(int (*read_blocks)(uint8_t,uint32_t,uint16_t,void *))
{if(!read_blocks||module_acsi_read_blocks)return 0;module_acsi_read_blocks=read_blocks;return 1;}

int kernel_acsi_write_blocks(uint8_t id,uint32_t sector,uint16_t count,const void *buffer)
{
    const uint8_t *p=(const uint8_t *)buffer;
    if(!count)return 0;
    while(count--){if(!fallback_acsi_write||!fallback_acsi_write(id,sector++,p))return 0;p+=512u;}
    return 1;
}

int kernel_acsi_read_blocks(uint8_t id,uint32_t sector,uint16_t count,void *buffer)
{
    uint8_t *p=(uint8_t *)buffer;
    if(!count)return 0;
    if(module_acsi_read_blocks)return module_acsi_read_blocks(id,sector,count,buffer);
    while(count--){if(!fallback_acsi_read||!fallback_acsi_read(id,sector++,p))return 0;p+=512u;}
    return 1;
}

int kernel_acsi_read_sector(uint8_t id,uint32_t sector,void *buffer)
{return kernel_acsi_read_blocks(id,sector,1u,buffer);}

enum module_state kernel_load_acsi(const boot_handoff_t *h)
{
    static const char name[11]={'A','C','S','I',' ',' ',' ',' ','S','Y','S'};
    module_image_info_t info;uint32_t (*entry)(const kernel_module_api_t *);void *probe;
    module_acsi_read_blocks=0;fallback_acsi_read=h?h->acsi_read_sector:0;
    fallback_acsi_write=h?h->acsi_write_sector:0;module_services=h;
    report_st_ram=h?h->st_ram_bytes:0;report_tt_ram=h?h->tt_ram_bytes:0;
    report_adapter=h?h->reserved0:0;report_chip=h?h->reserved1:0;
    report_netusbee_ethernet=h?h->netusbee_ethernet:0;report_netusbee_usb=h?h->netusbee_usb:0;
    module_cpu_type=h?h->cpu_type:0;module_fpu_type=h?h->fpu_type:0;
    module_machine_type=h?h->machine_type:0;module_clock_mhz=h?h->clock_mhz:0;
    module_clock_known=h?h->clock_known:0;module_console_clear=h?h->console_clear:0;
    module_console_puts=h?h->console_puts:0;module_console_put_u32=h?h->console_put_u32:0;
    prepare_module_api();
    if(!h||!h->load_module||!fallback_acsi_read||!h->load_module(name,0,0,&info))return MODULE_NOT_FOUND;
    if(info.memory_size<info.file_size||info.entry_offset<BOOT_IMAGE_HEADER_SIZE||info.entry_offset>=info.file_size||info.memory_size>256UL*1024UL)return MODULE_INVALID;
    acsi_module_memory=kmem_alloc(info.memory_size,16u,ACSI_OWNER,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,0);
    if(!acsi_module_memory)return MODULE_NO_MEMORY;
    if(!h->load_module(name,acsi_module_memory,info.memory_size,&info)){kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_INVALID;}
    entry=(uint32_t (*)(const kernel_module_api_t *))((uint8_t *)acsi_module_memory+info.entry_offset);
    if(!entry(&module_api)||!module_acsi_read_blocks){module_acsi_read_blocks=0;kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_INIT_FAILED;}
    probe=kmem_alloc(1024u,512u,ACSI_OWNER,KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
    if(!probe||!kernel_acsi_read_blocks(h->boot_acsi_id,0,2u,probe)){if(probe)kmem_free(probe,ACSI_OWNER);module_acsi_read_blocks=0;kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_IO_FAILED;}
    kmem_free(probe,ACSI_OWNER);
    return MODULE_READY;
}

int kernel_input_pending(void)
{const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);return event&&event->pending&&event->pending(0u);}