#include "kernel_module.h"
#include "kernel_fat.h"
#include "kernel_partition.h"
#include "kernel_block.h"
#include "kernel_input.h"
#include "kernel_event.h"
#include "kernel_timer.h"
#include "kernel_proc.h"
#include "hardware.h"
#include "tos_prg.h"

#define ACSI_OWNER 0x0100u
#define MAX_SERVICES 16u
typedef struct {uint32_t id;uint16_t version,owner;const void *interface;} service_t;
static kernel_module_api_t module_api;
static service_t services[MAX_SERVICES];
static uint8_t service_count;
static uint16_t active_owner;
static const uint8_t *active_image;
static uint32_t active_memory_size;
static uint8_t module_cpu_type;
static uint8_t module_fpu_type,module_machine_type,module_clock_mhz,module_clock_known;
static uint8_t mounted_drive=0xffu;
static void (*module_console_clear)(void);
static void (*module_console_puts)(const char *);
static void (*module_console_put_u32)(uint32_t);
extern int32_t kernel_tos_desktop_active(void);

/* G0-2: shadow of the last console lines.  The desktop compositor draws this
 * inside a console window, which is what lets the desktop stay on screen
 * instead of the shell returning to the raw text mode. */
#define KERNEL_LOG_LINES 24u
#define KERNEL_LOG_COLS  80u
static char kernel_log[KERNEL_LOG_LINES][KERNEL_LOG_COLS+1u];
static uint8_t kernel_log_head,kernel_log_count;

int32_t kernel_console_log_count(void){return (int32_t)kernel_log_count;}
const char *kernel_console_log_line(int32_t index)
{
    if(index<0||index>=kernel_log_count)return "";
    return kernel_log[(uint8_t)((kernel_log_head+KERNEL_LOG_LINES-kernel_log_count+index)%KERNEL_LOG_LINES)];
}

static void kernel_console_sink(const char *text)
{
    const char *p=text;
    /* The shell blinks its cursor after every pump by printing a lone "_" (or
       a space).  Those are screen furniture, not console text: forwarding them
       to the log filled it with "_ " pairs and scrambled the console window. */
    if(text&&(text[0]=='_'||text[0]==' ')&&text[1]=='\0'){
        if(module_console_puts)module_console_puts(text);
        return;
    }
    if(text){
        for(;*text;++text){
            char c=*text;
            char *line=kernel_log[kernel_log_head];
            uint8_t n=0;
            if(c=='\n'){
                kernel_log_head=(uint8_t)((kernel_log_head+1u)%KERNEL_LOG_LINES);
                kernel_log[kernel_log_head][0]='\0';
                if(kernel_log_count<KERNEL_LOG_LINES)++kernel_log_count;
                continue;
            }
            if(c<' '||c=='\x7f')continue;   /* CR and other control bytes */
            while(line[n]&&n<KERNEL_LOG_COLS)++n;
            if(n>=KERNEL_LOG_COLS){
                kernel_log_head=(uint8_t)((kernel_log_head+1u)%KERNEL_LOG_LINES);
                line=kernel_log[kernel_log_head];
                line[0]='\0';n=0;
                if(kernel_log_count<KERNEL_LOG_LINES)++kernel_log_count;
            }
            line[n]=c;line[n+1]='\0';
        }
    }
    /* While the TOS desktop is on screen the text console must stay silent:
       its text-mode writes would scribble over the 8bpp framebuffer.  The
       compositor draws the log instead. */
    if(module_console_puts&&!kernel_tos_desktop_active())module_console_puts(p);
}

/* Numbers are printed through a separate BIOS entry point; route them through
 * the same sink so the console log has no gaps. */
static void kernel_console_u32_sink(uint32_t value)
{
    char buffer[12];int i=0,n=0;char digits[12];
    if(value==0u)buffer[i++]='0';
    else{while(value){digits[n++]=(char)('0'+value%10u);value/=10u;}while(n)buffer[i++]=digits[--n];}
    buffer[i]='\0';
    kernel_console_sink(buffer);
}
extern int32_t kernel_tos_desktop_active(void);
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
static int register_acsi_read_blocks(int (*read_blocks)(uint8_t,uint32_t,uint16_t,void *));
static uint8_t user_shift_state;
static uint8_t user_mouse_buttons;
static const uint8_t user_key_plain[64]={0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,0,'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';',39,'`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0};
static const uint8_t user_key_shifted[64]={0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,0,'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0};
static volatile uint8_t ikbd_raw_data[256],ikbd_raw_status[256];
static volatile uint8_t ikbd_raw_head,ikbd_raw_tail;
static volatile uint32_t ikbd_irq_dropped,ikbd_raw_count;

/* Diagnostics: walk back through the captured bytes.  back==0 is the newest.
 * Returns (status<<8)|data, or -1 when there is nothing that far back. */
int32_t kernel_ikbd_raw_last(uint32_t back)
{
    uint8_t idx;
    /* Only slots actually written since the reset are meaningful: walking
       past ikbd_raw_count would report stale ring contents as if they were
       recent input (which made a duplicate make look real). */
    if(back>=256u||back>=ikbd_raw_count)return -1;
    idx=(uint8_t)(ikbd_raw_head-1u-back);
    return ((int32_t)ikbd_raw_status[idx]<<8)|(int32_t)ikbd_raw_data[idx];
}
int32_t kernel_ikbd_drop_count(void){return (int32_t)ikbd_irq_dropped;}
int32_t kernel_ikbd_raw_total(void){return (int32_t)ikbd_raw_count;}

void kernel_ikbd_irq_reset(void)
{ikbd_raw_head=ikbd_raw_tail=0;ikbd_irq_dropped=0;ikbd_raw_count=0;}

/* Ctrl+Alt+Del: flush the disk cache and warm-reset into the BIOS entry
 * (ROM offset 0x30, after the Atari ROM header). */
static volatile uint8_t warm_ctrl,warm_alt;
static void kernel_warm_reset(void)
{
    kblock_flush();
    __asm__ volatile("move.l #0x00e00030,%%a0\n\tjmp (%%a0)" ::: "a0","memory");
    for(;;){}
}

void kernel_ikbd_irq_capture(void)
{
    uint8_t status=REG8(IKBD_ACIA_CTRL),next,data;
    if(!(status&1u))return;
    data=REG8(IKBD_ACIA_DATA);next=(uint8_t)(ikbd_raw_head+1u);
    if(data==0x1du)warm_ctrl=1u;
    else if(data==0x9du)warm_ctrl=0u;
    else if(data==0x38u)warm_alt=1u;
    else if(data==0xb8u)warm_alt=0u;
    else if(data==0x53u&&warm_ctrl&&warm_alt){kernel_warm_reset();return;}
    if(next==ikbd_raw_tail){++ikbd_irq_dropped;return;}
    ikbd_raw_status[ikbd_raw_head]=status;ikbd_raw_data[ikbd_raw_head]=data;
    ikbd_raw_head=next;
    ++ikbd_raw_count;
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
typedef struct {char path[64],arguments[64],working_directory[64];uint32_t id;uint8_t result_slot;} async_job_t;
typedef struct {uint32_t id,owner_pid;int32_t status;uint8_t state;} async_result_t;
static async_job_t async_jobs[4];
static async_result_t async_results[4];
static uint8_t async_head,async_tail,async_count;
static uint32_t next_async_id=1u;
static int32_t kernel_dispatch_queued_user(void);

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
    uint32_t type,value,scan,ascii=0;
    if(!event_type||!scan_code||!modifiers||!character||!input||!event||
       !input->poll||!event->pending||!event->receive)return 0;
    input->poll(0u);
    if(!event->pending(0u)||!event->receive(0u,&type,&value))return 0;
    scan=value&0x7fu;
    if(scan==0x1du){if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=4u;else user_shift_state&=(uint8_t)~4u;}
    else if(scan==0x2au||scan==0x36u){uint8_t bit=scan==0x2au?1u:2u;
        if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=bit;else user_shift_state&=(uint8_t)~bit;}
    else if(scan==0x38u){if(type==KERNEL_INPUT_KEY_DOWN)user_shift_state|=8u;
        else user_shift_state&=(uint8_t)~8u;}
    else if(type==KERNEL_INPUT_KEY_DOWN&&scan<64u){
        if(user_shift_state&4u){if(scan==0x2eu)ascii=3u;else if(scan==0x2cu)ascii=26u;}
        else ascii=(user_shift_state&3u)?user_key_shifted[scan]:user_key_plain[scan];}
    *event_type=type;*scan_code=scan;*modifiers=user_shift_state;*character=ascii;
    return 1;
}

int kernel_user_mouse_event(uint32_t *buttons,int32_t *delta_x,int32_t *delta_y)
{
    const kernel_input_service_t *input=(const kernel_input_service_t *)kernel_find_service(KERNEL_SERVICE_INPUT,KERNEL_INPUT_VERSION);
    const kernel_event_service_t *event=(const kernel_event_service_t *)kernel_find_service(KERNEL_SERVICE_EVENT,KERNEL_EVENT_VERSION);
    uint32_t type,value,latest_buttons=user_mouse_buttons;
    int32_t total_x=0,total_y=0;int received=0,pressed=0;
    if(!buttons||!delta_x||!delta_y||!input||!event||!input->poll||
       !event->pending||!event->receive)return 0;
    input->poll(0u);
    /* Jak XaAES k_mouse.c ("Read a packet out of our queue"): czytamy JEDEN
       pakiet na wywolanie i zwracamy jego delte. Wczesniejsza agregacja
       wszystkich zaleglych pakietow w jedna sume powodowala skoki kursora
       (a MP_TIMEGAP w XaAES ogranicza tempo). Bit 2 pozostaje markerem
       krawedzi wcisniecia. */
    while(event->pending(1u)&&event->receive(1u,&type,&value)){
        uint32_t packet_buttons;
        if(type!=KERNEL_INPUT_MOUSE_PACKET)continue;
        packet_buttons=(value>>16)&3u;
        if(packet_buttons)pressed=1;
        latest_buttons=packet_buttons;
        total_x=(int32_t)(int8_t)((value>>8)&0xffu);
        total_y=(int32_t)(int8_t)(value&0xffu);
        received=1;
        break;
    }
    if(!received)return 0;
    user_mouse_buttons=(uint8_t)latest_buttons;
    *buttons=latest_buttons|(pressed?4u:0u);
    *delta_x=total_x;*delta_y=total_y;return 1;
}

void kernel_user_mouse_flush(void)
{
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

int32_t kernel_user_yield(void)
{
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    const kernel_proc_service_t *proc=(const kernel_proc_service_t *)kernel_find_service(KERNEL_SERVICE_PROC,KERNEL_PROC_VERSION);
    extern void kernel_user_gui_cursor_pump(void);
    (void)kernel_dispatch_queued_user();
    /* Kursor aktualizujemy przy KAZDYM oddaniu sterowania: inaczej proces
       spiac w kernel_user_sleep (pause_tick) blokuje petle glowna i wskaznik
       rusza sie tylko wtedy, gdy okno sie odswieza. */
    kernel_user_gui_cursor_pump();
    if(!timer||!proc||!timer->poll||!timer->ticks||!timer->switch_pending||
       !timer->acknowledge_switch||!proc->timer_tick||!proc->cooperate)return -1;
    timer->poll();
    if(timer->switch_pending()){timer->acknowledge_switch();proc->timer_tick();}
    if(!proc->cooperate())return -1;
    return (int32_t)timer->ticks();
}

/* Odczyt ticku BEZ oddawania sterowania. Uzywany przez petle kursora:
 * kernel_user_yield() wpuszcza scheduler (a wiec i render desktopu), przez co
 * wskaznik myszy czeka i porusza sie skokowo. */
int32_t kernel_user_tick_now(void)
{
    const kernel_timer_service_t *timer=(const kernel_timer_service_t *)kernel_find_service(KERNEL_SERVICE_TIMER,KERNEL_TIMER_VERSION);
    if(!timer||!timer->ticks)return -1;
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
    info[1]=44u;                /* highest implemented syscall */
    info[2]=module_cpu_type;
    info[3]=timer->ticks();
    info[4]=st.free;
    info[5]=tt.free;
    return 0;
}

static int system_file_size(const char name[11],uint32_t *size)
{return kfat_find_system_file(name,size)==FAT_FILE_FOUND;}

static int load_system_file(const char name[11],void *destination,
                            uint32_t capacity,uint32_t *size)
{return kfat_load_system_file(name,destination,capacity,size)==FAT_FILE_FOUND;}

static uint8_t boot_drive_index(void)
{uint8_t i;for(i=0;i<kpart_count();++i)if(kpart_is_boot(i))return i;return 0;}

static uint32_t boot_drive_letter(void)
{if(mounted_drive==0xffu)mounted_drive=boot_drive_index();return (uint32_t)('C'+mounted_drive);}

uint32_t kernel_user_drive_letter(void)
{return boot_drive_letter();}

static int select_drive(uint32_t letter)
{
    uint8_t index,old;
    if(letter>='a'&&letter<='z')letter=letter-'a'+'A';
    if(letter<(uint32_t)'C'||letter>=(uint32_t)'C'+kpart_count())return 0;
    index=(uint8_t)(letter-'C');
    if(!kpart_accessible(index))return 0;
    if(mounted_drive==0xffu)mounted_drive=boot_drive_index();
    if(index==mounted_drive)return 1;
    old=mounted_drive;
    if(kfat_mount(kpart_start(index),kpart_id(index))==FAT_MOUNTED){mounted_drive=index;return 1;}
    (void)kfat_mount(kpart_start(old),kpart_id(old));
    return 0;
}

static int stat_path(const char *path,uint32_t *size,uint8_t *attributes)
{return kfat_stat_path(path,size,attributes)==FAT_FILE_FOUND;}

static int read_path(const char *path,void *destination,uint32_t capacity,uint32_t *size)
{return kfat_load_path(path,destination,capacity,size)==FAT_FILE_FOUND;}

static int list_path(const char *path,kfat_list_callback_t callback,void *context)
{return kfat_list_path(path,callback,context)==FAT_FILE_FOUND;}

static int read_path_at(const char *path,uint32_t offset,void *destination,
                        uint32_t capacity,uint32_t *size)
{return kfat_read_path_at(path,offset,destination,capacity,size)==FAT_FILE_FOUND;}

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
    module_api.console_puts=kernel_console_sink;
    module_api.console_put_u32=kernel_console_u32_sink;
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
    module_api.run_tos_program=kernel_tos_run_program;
}

static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

static int verify_app_image(const uint8_t *image,uint32_t file_size)
{
    static const char magic[8]={'A','S','Y','S','A','P','P','0'};
    uint32_t i,sum=0,memory,entry,stack,executable_end,writable_start;
    if(!image||file_size<44u)return 0;
    for(i=0;i<8u;++i)if(image[i]!=(uint8_t)magic[i])return 0;
    if(be16(image+8)!=1u||be16(image+10)!=32u||be32(image+12)!=file_size)return 0;
    memory=be32(image+16);entry=be32(image+20);stack=be32(image+24);
    if(memory<file_size||memory>262144u||entry<32u||entry>=file_size||stack<1024u||stack>16384u||(stack&1u))return 0;
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
    if(state!=FAT_FILE_FOUND||(attributes&0x10u)||file_size<32u||file_size>262144u)return -1;
    image=(uint8_t *)kmem_alloc(file_size,16u,0x0410u,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,1);
    if(!image)return -2;
    if(kfat_load_path(path,image,file_size,&loaded)!=FAT_FILE_FOUND||loaded!=file_size||!verify_app_image(image,file_size)){result=-3;goto done;}
    memory_size=be32(image+16);stack_size=be32(image+24);entry=be32(image+20);
    if(memory_size>file_size){uint8_t *expanded=(uint8_t *)kmem_alloc(memory_size,16u,0x0410u,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,1);
        if(!expanded){result=-2;goto done;}for(i=0;i<file_size;++i)expanded[i]=image[i];for(;i<memory_size;++i)expanded[i]=0;
        kmem_free(image,0x0410u);image=expanded;}
    stack=(uint8_t *)kmem_alloc(stack_size+8u,16u,0x0411u,
                               KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
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
    job->result_slot=result_slot;
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
    async_head=(uint8_t)((async_head+1u)&3u);--async_count;
    async_results[job.result_slot].state=2u;
    status=kernel_spawn_user(job.path,job.arguments,job.working_directory,&child_pid);
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

enum module_state kernel_load_system_module(const char name[11],uint16_t owner,
                                            int prefer_tt)
{
    uint32_t file_size=0,loaded_size=0,memory_size,entry_offset,i;uint8_t old_service_count;
    uint8_t *image;uint32_t (*entry)(const kernel_module_api_t *);
    enum fat_file_state state=kfat_find_system_file(name,&file_size);
    if(state==FAT_FILE_NOT_FOUND)return MODULE_NOT_FOUND;
    if(state==FAT_FILE_IO_FAILED)return MODULE_IO_FAILED;
    if(state!=FAT_FILE_FOUND||file_size<BOOT_IMAGE_HEADER_SIZE)return MODULE_INVALID;
    image=(uint8_t *)kmem_alloc(file_size,16u,owner,
        KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,prefer_tt);
    if(!image)return MODULE_NO_MEMORY;
    state=kfat_load_system_file(name,image,file_size,&loaded_size);
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

static int register_acsi_read_blocks(int (*read_blocks)(uint8_t,uint32_t,uint16_t,void *))
{if(!read_blocks||module_acsi_read_blocks)return 0;module_acsi_read_blocks=read_blocks;return 1;}

/* --- System-wide guarded DMA staging -------------------------------------
 * The ACSI DMA engine can write past the end of the requested transfer
 * (observed: 256 bytes past a 2-sector read).  No caller buffer is therefore
 * used as a DMA target.  Every transfer lands in this static ST-RAM staging
 * area, which carries a canary margin; the payload is then copied to the
 * caller.  The margin is part of the array, so an overrun cannot reach the
 * heap, a module image or any other object.  The stage is always in 24-bit
 * ST-RAM, so transfers also work when the destination lives in TT-RAM.
 * ------------------------------------------------------------------------ */
#define DMA_STAGE_SECTORS  8u
#define DMA_STAGE_BYTES    (DMA_STAGE_SECTORS*512u)
#define DMA_GUARD_BYTES    2048u
#define DMA_GUARD_PATTERN  0x5au

static uint8_t dma_stage[DMA_STAGE_BYTES+DMA_GUARD_BYTES] __attribute__((aligned(512)));
uint32_t kernel_dma_guard_hits;

static int dma_stage_read(uint8_t id,uint32_t sector,uint16_t count,uint8_t *destination)
{
    uint32_t i;
    for(i=0u;i<DMA_GUARD_BYTES;++i)dma_stage[DMA_STAGE_BYTES+i]=DMA_GUARD_PATTERN;
    if(module_acsi_read_blocks){
        if(!module_acsi_read_blocks(id,sector,count,dma_stage))return 0;
    } else {
        uint16_t s;
        for(s=0u;s<count;++s)
            if(!fallback_acsi_read||!fallback_acsi_read(id,sector+s,dma_stage+(uint32_t)s*512u))return 0;
    }
    for(i=0u;i<DMA_GUARD_BYTES;++i)
        if(dma_stage[DMA_STAGE_BYTES+i]!=DMA_GUARD_PATTERN)break;
    if(i<DMA_GUARD_BYTES)++kernel_dma_guard_hits;
    for(i=0u;i<(uint32_t)count*512u;++i)destination[i]=dma_stage[i];
    return 1;
}

int kernel_acsi_read_blocks(uint8_t id,uint32_t sector,uint16_t count,void *buffer)
{
    uint8_t *p=(uint8_t *)buffer;
    if(!count)return 0;
    while(count){
        uint16_t chunk=(uint16_t)(count>DMA_STAGE_SECTORS?DMA_STAGE_SECTORS:count);
        if(!dma_stage_read(id,sector,chunk,p))return 0;
        p+=(uint32_t)chunk*512u;
        sector+=chunk;
        count=(uint16_t)(count-chunk);
    }
    return 1;
}

int kernel_acsi_read_sector(uint8_t id,uint32_t sector,void *buffer)
{return kernel_acsi_read_blocks(id,sector,1u,buffer);}

int kernel_acsi_write_blocks(uint8_t id,uint32_t sector,uint16_t count,const void *buffer)
{
    const uint8_t *p=(const uint8_t *)buffer;uint16_t i;
    if(!fallback_acsi_write||!buffer||!count)return 0;
    for(i=0u;i<count;++i)if(!fallback_acsi_write(id,sector+i,p+(uint32_t)i*512u))return 0;
    return 1;
}
int kernel_acsi_write_sector(uint8_t id,uint32_t sector,const void *buffer)
{return kernel_acsi_write_blocks(id,sector,1u,buffer);}

/* Scratch buffer for the module smoke test; never a direct DMA target. */
static uint8_t acsi_probe_area[1024] __attribute__((aligned(512)));

enum module_state kernel_load_acsi(const boot_handoff_t *h)
{
    static const char name[11]={'A','C','S','I',' ',' ',' ',' ','S','Y','S'};
    module_image_info_t info;uint32_t (*entry)(const kernel_module_api_t *);void *probe;
    kernel_dma_guard_hits=0;
    module_acsi_read_blocks=0;fallback_acsi_read=h?h->acsi_read_sector:0;
    fallback_acsi_write=h?h->acsi_write_sector:0;
    module_cpu_type=h?h->cpu_type:0;module_fpu_type=h?h->fpu_type:0;
    module_machine_type=h?h->machine_type:0;module_clock_mhz=h?h->clock_mhz:0;
    module_clock_known=h?h->clock_known:0;module_console_clear=h?h->console_clear:0;
    module_console_puts=h?h->console_puts:0;module_console_put_u32=h?h->console_put_u32:0;
    kernel_tos_set_console(module_console_puts);
    kernel_tos_set_cpu(h?h->cpu_type:0u);
    kernel_tos_set_input(ikbd_raw_pop);
    kernel_tos_set_console_clear(h?h->console_clear:0);
    kernel_tos_set_video(h?h->video_enter:0,h?h->video_leave:0,h?h->video_clear:0,
                         h?h->video_fill:0,h?h->video_text:0,h?h->video_blit_mono:0);
    /* IDE boot (id bit 7): the BIOS reader already speaks IDE, so the ACSI DMA
     * module does not apply and the BIOS fallback is the correct transport.
     * module_cpu_type must still be set: module image loads sync the 68030
     * instruction cache through it. */
    if(h&&(h->boot_acsi_id&0x80u))return MODULE_NOT_FOUND;
    prepare_module_api();
    if(!h||!h->load_module||!fallback_acsi_read||!h->load_module(name,0,0,&info))return MODULE_NOT_FOUND;
    if(info.memory_size<info.file_size||info.entry_offset<BOOT_IMAGE_HEADER_SIZE||info.entry_offset>=info.file_size||info.memory_size>256UL*1024UL)return MODULE_INVALID;
    acsi_module_memory=kmem_alloc(info.memory_size,16u,ACSI_OWNER,KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,0);
    if(!acsi_module_memory)return MODULE_NO_MEMORY;
    if(!h->load_module(name,acsi_module_memory,info.memory_size,&info)){kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_INVALID;}
    entry=(uint32_t (*)(const kernel_module_api_t *))((uint8_t *)acsi_module_memory+info.entry_offset);
    if(!entry(&module_api)||!module_acsi_read_blocks){module_acsi_read_blocks=0;kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_INIT_FAILED;}
    probe=acsi_probe_area;
    if(!kernel_acsi_read_blocks(h->boot_acsi_id,0,2u,probe)){module_acsi_read_blocks=0;kmem_free(acsi_module_memory,ACSI_OWNER);acsi_module_memory=0;return MODULE_IO_FAILED;}
    return MODULE_READY;
}
