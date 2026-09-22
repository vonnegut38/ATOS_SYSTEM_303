#include "kernel_console.h"
#include "kernel_sessions.h"
#include "kernel_wm.h"
#include "system_report.h"
#include "kernel_block.h"
#include "kernel_partition.h"
extern volatile uint32_t kernel_wall_ticks;
#include "boot_protocol.h"
#include "kernel_fat.h"
#include "kernel_memory.h"
#include "kernel_graphics.h"
#include "kernel_surface.h"
#include "gui_draw.h"
#define USER_FILE_HANDLES 4u
#define USER_MEMORY_BLOCKS 8u
#define USER_MEMORY_QUOTA (1024u*1024u)
#define USER_PROCESS_SLOTS 6u
#define USER_MAILBOX_CLIENT_DEPTH 4u
#define USER_MAILBOX_INPUT_DEPTH 6u
#define USER_MAILBOX_DEPTH 8u
#define USER_SERVICE_SLOTS 8u
#define USER_PROCESS_FREE 0u
#define USER_PROCESS_READY 1u
#define USER_PROCESS_RUNNING 2u
#define USER_PROCESS_SLEEPING 3u
#define USER_PROCESS_WAITING 4u
#define USER_PROCESS_TERMINATED 5u
#define USER_PROCESS_INPUT 6u
#define USER_PROCESS_SUSPENDED 7u
#define USER_PROCESS_MESSAGE 8u
#define USER_STACK_GUARD_BYTES 4u
#define USER_STACK_GUARD 0x5354414bu
#define USER_STACK_UNUSED 0xa5u
#define USER_STACK_FAULT (-0x3b0)
#define USER_STACK_HIGH_FAULT (-0x3b2)
#define USER_STACK_POINTER_FAULT (-0x3b3)
#define USER_HEAP_GUARD_BYTES 16u
#define USER_HEAP_GUARD 0x48454150u
#define USER_HEAP_FAULT (-0x3b1)
typedef struct {char path[64];uint32_t size,offset;uint8_t open;} user_file_t;
typedef struct {void *pointer,*allocation;uint32_t size;} user_memory_t;
typedef struct {char name[13];uint8_t attributes,padding[2];uint32_t size;} user_dir_entry_t;
typedef struct {uint32_t wanted,current;user_dir_entry_t *entry;uint8_t found;} user_dir_context_t;
typedef struct {char path[64],arguments[64];uint32_t pid;int32_t status;} user_spawn_request_t;
typedef struct {uint32_t sender,value[4];} user_message_t;
typedef struct {uint32_t opcode,xy,wh,color;} user_gfx_request_t;
typedef struct {
    const uint8_t *image;
    uint32_t image_size,pid,parent_pid;
    uint32_t saved_usp,saved_pc;
    uint32_t saved_registers[15];
    void *runtime_stack,*runtime_image;
    uint32_t runtime_stack_size;
    uint16_t runtime_stack_owner,runtime_image_owner;
    uint32_t wake_tick;
    uint32_t cpu_ticks;
    uint32_t watchdog_ticks;
    uint32_t wait_pid;
    int32_t *wait_status_pointer;
    user_message_t *wait_message_pointer;
    int32_t syscall_result;
    uint16_t memory_owner;
    uint16_t saved_sr;
    uint8_t state,terminal,registers_valid;
    uint8_t quantum_left;              /* ticks left in the current slice */
    user_file_t files[USER_FILE_HANDLES];
    user_memory_t memory[USER_MEMORY_BLOCKS];
    char working_directory[64];
    char arguments[64];
    char program_name[13];
    user_message_t mailbox[USER_MAILBOX_DEPTH];
    uint8_t mailbox_head,mailbox_tail,mailbox_count;
} user_process_t;
static const boot_handoff_t *services;
static user_process_t process_table[USER_PROCESS_SLOTS];
static user_process_t *current_process=process_table;
static int32_t user_stack_fault=USER_STACK_FAULT;
static user_process_t *suspended_parents[USER_PROCESS_SLOTS];
static uint8_t suspended_depth;
static char pending_arguments[64];
static uint32_t next_user_pid=1u;
static uint8_t next_process_slot;
static uint8_t user_console_flags[SESSION_COUNT];
#define user_console_dirty user_console_flags[ksessions_current()]
static uint32_t terminal_owners[SESSION_COUNT];
#define terminal_owner_pid terminal_owners[ksessions_current()]
static uint32_t gui_keyboard_pids[SESSION_COUNT];
#define gui_keyboard_pid gui_keyboard_pids[ksessions_current()]
static uint32_t gui_keyboard_ready_tick;
static uint32_t gui_keyboard_dropped;
static uint8_t gui_keyboard_persistent;
static int32_t gui_mouse_x,gui_mouse_y;
static char system_exec_path[64]="\\SYSTEM;\\SYSTEM\\COMMAND;\\SYSTEM\\APP;\\SYSTEM\\TEST";

int kernel_exec_path_get(uint32_t index,char destination[64])
{uint8_t in=0,out=0;if(!destination)return 0;
 while(index){while(system_exec_path[in]&&system_exec_path[in]!=';')++in;if(!system_exec_path[in])return 0;++in;--index;}
 while(system_exec_path[in]&&system_exec_path[in]!=';'&&out<63u)destination[out++]=system_exec_path[in++];
 destination[out]='\0';return out!=0;}
static uint32_t gui_desktop_pids[SESSION_COUNT];
#define gui_desktop_pid gui_desktop_pids[ksessions_current()]
typedef struct {uint32_t pid;int32_t status;uint8_t terminal;} user_completion_t;
static user_completion_t user_completions[4];
static uint8_t user_completion_next;
static uint8_t user_completion_notified[4];
typedef struct {char name[13];uint32_t pid;} user_service_t;
static user_service_t user_services[USER_SERVICE_SLOTS];
#define user_image current_process->image
#define user_image_size current_process->image_size
#define user_active (current_process->state==USER_PROCESS_RUNNING)
#define user_syscall_result current_process->syscall_result
#define user_files current_process->files
#define user_memory current_process->memory
#define user_working_directory current_process->working_directory
#define user_arguments current_process->arguments
#define user_memory_owner current_process->memory_owner
extern int32_t kernel_user_terminal_getchar(void);
extern int kernel_user_keyboard_event(uint32_t *,uint32_t *,uint32_t *,uint32_t *);
extern int kernel_user_mouse_event(uint32_t *,int32_t *,int32_t *);
extern void kernel_user_mouse_flush(void);
extern void kernel_user_terminal_reset_modifiers(void);
extern int32_t kernel_user_yield(void);
extern uint32_t kernel_user_clock(void);
extern int32_t kernel_user_sleep(uint32_t ticks);
extern int32_t kernel_user_system_info(uint32_t *info);
extern uint32_t kernel_user_drive_letter(void);
extern int kernel_dispatch_active_user(void);
extern void kernel_sync_user_image(void);
extern int32_t kernel_spawn_user(const char *path,const char *arguments,
                                 const char *working_directory,uint32_t *child_pid);
static int user_range(const void *pointer,uint32_t bytes);
static int user_process_range(const user_process_t *,const void *,uint32_t);
static int user_write_range(const void *pointer,uint32_t bytes);
static int user_write_aligned(const void *pointer,uint32_t bytes,uint32_t alignment);
static int user_pc_valid_image(const uint8_t *image,uint32_t memory_size,uint32_t pc);
void kernel_user_end(void);
void kernel_exception_bind(const boot_handoff_t *h){services=h;}

static void user_mailbox_reset(user_process_t *process)
{process->mailbox_head=0;process->mailbox_tail=0;process->mailbox_count=0;}

static int32_t user_message_send(uint32_t pid,const uint32_t value[4])
{
    uint8_t i,j,index;user_process_t *target=0;user_message_t *message;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE&&
       process_table[i].state!=USER_PROCESS_TERMINATED){target=&process_table[i];break;}
    if(!target)return -0x3c1;
    if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
        message=target->wait_message_pointer;message->sender=current_process->pid;
        for(j=0;j<4u;++j)message->value[j]=value[j];
        target->wait_message_pointer=0;target->saved_registers[0]=0;
        target->state=USER_PROCESS_READY;return 0;
    }
    /* Keep half of the queue reserved for non-forgeable kernel lifecycle
       events. A client flood must not hide process death from SYSTEM.GUI. */
    if(target->mailbox_count>=USER_MAILBOX_CLIENT_DEPTH){
        /* A synchronous service response must not be lost behind disposable
           pointer motion. Replace the newest KMOU entry in-place, preserving
           keyboard and non-forgeable lifecycle events. This also prevents a
           GUI client from waiting forever after an input burst. */
        index=target->mailbox_tail;
        for(i=0;i<target->mailbox_count;++i){
            index=(uint8_t)((index+USER_MAILBOX_DEPTH-1u)%USER_MAILBOX_DEPTH);
            message=&target->mailbox[index];
            if(message->sender==0u&&message->value[0]==0x4b4d4f55u){
                message->sender=current_process->pid;
                for(j=0;j<4u;++j)message->value[j]=value[j];
                return 0;
            }
        }
        return -0x3c2;
    }
    message=&target->mailbox[target->mailbox_tail];message->sender=current_process->pid;
    for(j=0;j<4u;++j)message->value[j]=value[j];
    target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
    ++target->mailbox_count;return 0;
}

static int32_t user_message_receive(user_message_t *destination)
{
    uint8_t j;user_message_t *message;
    if(!current_process->mailbox_count)return -0x3c3;
    message=&current_process->mailbox[current_process->mailbox_head];
    destination->sender=message->sender;
    for(j=0;j<4u;++j)destination->value[j]=message->value[j];
    current_process->mailbox_head=(uint8_t)((current_process->mailbox_head+1u)%USER_MAILBOX_DEPTH);
    --current_process->mailbox_count;return 0;
}

static void user_service_remove_pid(uint32_t pid)
{
    uint8_t i;if(!pid)return;
    for(i=0;i<USER_SERVICE_SLOTS;++i)if(user_services[i].pid==pid){
        user_services[i].pid=0;user_services[i].name[0]='\0';}
}

/* Deliver one process-lifecycle event with sender PID zero.  There are at
   most four GUI clients, so the reserved half of the eight-entry mailbox can
   retain every simultaneous exit even while the normal client half is full. */
static void user_gui_notify_exit(uint32_t pid,int32_t status)
{
    uint8_t i;user_process_t *target=0;user_message_t *message;
    uint32_t gui_pid=0;
    if(!pid)return;
    for(i=0;i<USER_SERVICE_SLOTS;++i)if(user_services[i].pid&&
       user_services[i].name[0]=='S'&&user_services[i].name[1]=='Y'&&
       user_services[i].name[2]=='S'&&user_services[i].name[3]=='T'&&
       user_services[i].name[4]=='E'&&user_services[i].name[5]=='M'&&
       user_services[i].name[6]=='.'&&user_services[i].name[7]=='G'&&
       user_services[i].name[8]=='U'&&user_services[i].name[9]=='I'&&
       !user_services[i].name[10]){gui_pid=user_services[i].pid;break;}
    if(!gui_pid||gui_pid==pid)return;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==gui_pid&&
       process_table[i].state!=USER_PROCESS_FREE&&
       process_table[i].state!=USER_PROCESS_TERMINATED){target=&process_table[i];break;}
    if(!target)return;
    if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
        message=target->wait_message_pointer;target->wait_message_pointer=0;
        target->saved_registers[0]=0;target->state=USER_PROCESS_READY;
    }else{
        if(target->mailbox_count>=USER_MAILBOX_DEPTH)return;
        message=&target->mailbox[target->mailbox_tail];
        target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
        ++target->mailbox_count;
    }
    message->sender=0;message->value[0]=0x4b455849u; /* KEXI */
    message->value[1]=pid;message->value[2]=(uint32_t)status;message->value[3]=0;
}

static int user_service_name(const char *source,char destination[13])
{
    uint8_t i;char value;
    if(!source)return 0;
    for(i=0;i<13u;++i){
        if(!user_range(source+i,1u))return 0;
        value=source[i];
        if(!value){destination[i]='\0';return i!=0;}
        if(i==12u)return 0;
        if(value>='a'&&value<='z')value=(char)(value-'a'+'A');
        if(!((value>='A'&&value<='Z')||(value>='0'&&value<='9')||
             value=='_'||value=='-'||value=='.'))return 0;
        destination[i]=value;
    }
    return 0;
}

static int32_t user_service_register(const char *source)
{
    char name[13];uint8_t i,j,free_slot=USER_SERVICE_SLOTS;
    if(!user_service_name(source,name))return -0x3d0;
    for(i=0;i<USER_SERVICE_SLOTS;++i){
        if(!user_services[i].pid){if(free_slot==USER_SERVICE_SLOTS)free_slot=i;continue;}
        for(j=0;j<13u&&user_services[i].name[j]==name[j]&&name[j];++j){}
        if(j<13u&&user_services[i].name[j]==name[j])
            return user_services[i].pid==current_process->pid?0:-0x3d2;
    }
    if(free_slot==USER_SERVICE_SLOTS)return -0x3d1;
    for(j=0;j<13u;++j){user_services[free_slot].name[j]=name[j];if(!name[j])break;}
    user_services[free_slot].pid=current_process->pid;return 0;
}

static int32_t user_service_lookup(const char *source)
{
    char name[13];uint8_t i,j;
    if(!user_service_name(source,name))return -0x3d0;
    for(i=0;i<USER_SERVICE_SLOTS;++i)if(user_services[i].pid){
        for(j=0;j<13u&&user_services[i].name[j]==name[j]&&name[j];++j){}
        if(j<13u&&user_services[i].name[j]==name[j])return (int32_t)user_services[i].pid;
    }
    return -0x3d4;
}

static int32_t user_service_unregister(const char *source)
{
    char name[13];uint8_t i,j;
    if(!user_service_name(source,name))return -0x3d0;
    for(i=0;i<USER_SERVICE_SLOTS;++i)if(user_services[i].pid){
        for(j=0;j<13u&&user_services[i].name[j]==name[j]&&name[j];++j){}
        if(j<13u&&user_services[i].name[j]==name[j]){
            if(user_services[i].pid!=current_process->pid)return -0x3d3;
            user_services[i].pid=0;user_services[i].name[0]='\0';return 0;
        }
    }
    return -0x3d4;
}

uint32_t kernel_user_service_pid(const char *name)
{
    uint8_t i,j;if(!name||!name[0])return 0;
    for(i=0;i<USER_SERVICE_SLOTS;++i)if(user_services[i].pid){
        for(j=0;j<13u&&user_services[i].name[j]==name[j]&&name[j];++j){}
        if(j<13u&&user_services[i].name[j]==name[j])return user_services[i].pid;
    }
    return 0;
}

static uint32_t user_allocate_pid(void)
{
    uint8_t attempts,i;uint32_t candidate;
    for(attempts=0;attempts<=USER_PROCESS_SLOTS;++attempts){int used=0;
        candidate=next_user_pid++;if(!next_user_pid)next_user_pid=1u;
        if(!candidate)continue;
        for(i=0;i<USER_PROCESS_SLOTS;++i)
            if(process_table[i].state!=USER_PROCESS_FREE&&
               process_table[i].pid==candidate){used=1;break;}
        if(!used)return candidate;
    }
    return 0;
}

static uint16_t user_owner_for_slot(uint8_t slot)
{return (uint16_t)(0x0301u+slot);}

static void user_stack_init(user_process_t *process)
{
    uint8_t *base=(uint8_t *)process->runtime_stack;uint32_t i;
    *(uint32_t *)(void *)base=USER_STACK_GUARD;
    for(i=0;i<process->runtime_stack_size;++i)
        base[USER_STACK_GUARD_BYTES+i]=USER_STACK_UNUSED;
    *(uint32_t *)(void *)(base+USER_STACK_GUARD_BYTES+
                          process->runtime_stack_size)=USER_STACK_GUARD;
}

static int user_stack_valid(const user_process_t *process,uint32_t usp)
{
    const uint8_t *base=(const uint8_t *)process->runtime_stack;
    uint32_t low,high;
    user_stack_fault=USER_STACK_FAULT;
    if(!base)return 1;
    low=(uint32_t)(uintptr_t)(base+USER_STACK_GUARD_BYTES);
    high=low+process->runtime_stack_size;
    if((usp&1u)||usp<low||usp>high){user_stack_fault=USER_STACK_POINTER_FAULT;return 0;}
    if(*(const uint32_t *)(const void *)base!=USER_STACK_GUARD)return 0;
    if(*(const uint32_t *)(const void *)(base+USER_STACK_GUARD_BYTES+
                                        process->runtime_stack_size)!=USER_STACK_GUARD){
        user_stack_fault=USER_STACK_HIGH_FAULT;return 0;}
    return 1;
}

static void user_stack_fail_current(void)
{
    current_process->syscall_result=user_stack_fault;
    kernel_user_end();
}

static uint32_t user_be32(const uint8_t *value)
{return ((uint32_t)value[0]<<24)|((uint32_t)value[1]<<16)|
        ((uint32_t)value[2]<<8)|value[3];}

static int user_pc_valid_image(const uint8_t *image,uint32_t memory_size,uint32_t pc)
{
    uint32_t base=(uint32_t)(uintptr_t)image,file_size,executable_end;
    if(!image||memory_size<44u||(pc&1u)||pc<base)return 0;
    file_size=user_be32(image+12u);
    if(file_size<44u||file_size>memory_size||
       user_be32(image+file_size-12u)!=0x41505732u)return 0;
    executable_end=user_be32(image+file_size-8u);
    return executable_end>32u&&executable_end<=file_size-12u&&
           pc-base>=32u&&pc-base<executable_end;
}

static void user_heap_init(user_memory_t *memory)
{
    uint32_t i,*low=(uint32_t *)memory->allocation;
    uint32_t *high=(uint32_t *)((uint8_t *)memory->pointer+memory->size);
    for(i=0;i<USER_HEAP_GUARD_BYTES/4u;++i){low[i]=USER_HEAP_GUARD;high[i]=USER_HEAP_GUARD;}
}

static int user_heap_valid(const user_memory_t *memory)
{
    uint32_t i;const uint32_t *low,*high;
    if(!memory->pointer||!memory->allocation)return 0;
    low=(const uint32_t *)memory->allocation;
    high=(const uint32_t *)((const uint8_t *)memory->pointer+memory->size);
    for(i=0;i<USER_HEAP_GUARD_BYTES/4u;++i)
        if(low[i]!=USER_HEAP_GUARD||high[i]!=USER_HEAP_GUARD)return 0;
    return 1;
}

static int user_heap_release(user_memory_t *memory,uint16_t owner)
{
    int result=kmem_free(memory->allocation,owner);
    memory->pointer=0;memory->allocation=0;memory->size=0;return result;
}

static int user_heaps_valid_current(void)
{
    uint8_t i;
    for(i=0;i<USER_MEMORY_BLOCKS;++i)
        if(user_memory[i].pointer&&!user_heap_valid(&user_memory[i]))return 0;
    return 1;
}

static void user_heap_fail_current(void)
{
    current_process->syscall_result=USER_HEAP_FAULT;
    kernel_user_end();
}

int32_t kernel_user_session_getchar(void)
{if(current_process->terminal!=ksessions_current())return -1;return kernel_user_terminal_getchar();}
unsigned kernel_user_pid_terminal(uint32_t pid)
{unsigned i;for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==pid&&process_table[i].state!=USER_PROCESS_FREE)return process_table[i].terminal;return 0;}
unsigned kernel_user_last_terminal(void){return current_process->terminal;}
unsigned kernel_user_birth_terminal(void)
{return user_active?current_process->terminal:ksessions_current();}
void kernel_user_assign_terminal(uint32_t pid,unsigned terminal)
{unsigned i;if(terminal>=SESSION_COUNT)return;for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==pid)process_table[i].terminal=(uint8_t)terminal;}
int kernel_user_session_busy(unsigned terminal)
{unsigned i;for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].terminal==terminal&&process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED)return 1;return 0;}
int kernel_user_desktop_visible(void)
{return gui_desktop_pid&&kernel_user_pid_terminal(gui_desktop_pid)==ksessions_current();}
static void user_terminal_release(uint32_t pid)
{unsigned i;for(i=0;i<SESSION_COUNT;i++)if(terminal_owners[i]==pid)terminal_owners[i]=0;}

static void user_record_completion(uint32_t pid,int32_t status)
{
    if(!pid)return;
    user_completions[user_completion_next].pid=pid;
    user_completions[user_completion_next].status=status;
    user_completions[user_completion_next].terminal=(uint8_t)kernel_user_pid_terminal(pid);
    user_completion_notified[user_completion_next]=0u;
    user_completion_next=(uint8_t)((user_completion_next+1u)&3u);
}

int kernel_user_next_completion(uint32_t *pid,int32_t *status)
{
    uint8_t n,index;
    if(!pid||!status)return 0;
    for(n=0;n<4u;++n){
        index=(uint8_t)((user_completion_next+n)&3u);
        if(user_completions[index].pid&&!user_completion_notified[index]&&user_completions[index].terminal==ksessions_current()){
            *pid=user_completions[index].pid;
            *status=user_completions[index].status;
            user_completion_notified[index]=1u;
            return 1;
        }
    }
    return 0;
}

int kernel_user_take_completion(uint32_t pid,int32_t *status)
{
    uint8_t i;if(!pid||!status)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE)return -1;
    for(i=0;i<4u;++i)if(user_completions[i].pid==pid){
        *status=user_completions[i].status;user_completions[i].pid=0;
        user_completions[i].status=0;user_completion_notified[i]=0u;return 1;}
    return 0;
}

int kernel_user_console_dirty(void)
{int dirty=user_console_dirty;user_console_dirty=0;return dirty;}

int kernel_user_ready_pending(void)
{
    uint8_t i;
    for(i=0;i<USER_PROCESS_SLOTS;++i)
        if(process_table[i].state==USER_PROCESS_READY)return 1;
    return 0;
}

static void user_console_begin_output(void)
{
 unsigned t=current_process->terminal;
 if(!user_console_flags[t]&&kernel_dispatch_active_user()&&terminal_owners[t]!=current_process->pid)
     kconsole_puts_to(t,"\n");
 user_console_flags[t]=1;
}

int kernel_user_process_info(uint32_t slot,uint32_t info[4])
{
    user_process_t *process;
    if(slot>=USER_PROCESS_SLOTS||!info)return 0;
    process=&process_table[slot];
    if(process->state==USER_PROCESS_FREE)return 0;
    info[0]=process->pid;info[1]=process->parent_pid;
    info[2]=process->state;info[3]=(uint32_t)process->syscall_result;
    return 1;
}

int kernel_user_process_name(uint32_t slot,char name[13])
{
    uint8_t i;user_process_t *process;
    if(slot>=USER_PROCESS_SLOTS||!name)return 0;
    process=&process_table[slot];
    if(process->state==USER_PROCESS_FREE)return 0;
    for(i=0;i<12u&&process->program_name[i];++i)name[i]=process->program_name[i];
    name[i]='\0';return 1;
}

int kernel_user_kill(uint32_t pid)
{
    uint8_t i,j;user_process_t *process=0;
    if(!pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){process=&process_table[i];break;}
    if(!process||process->parent_pid!=0u)return 0;
    if(process->state==USER_PROCESS_RUNNING||process->state==USER_PROCESS_WAITING)return -1;
    for(i=0;i<USER_MEMORY_BLOCKS;++i)if(process->memory[i].pointer)
        (void)user_heap_release(&process->memory[i],process->memory_owner);
    if(process->runtime_stack){(void)kmem_release(process->runtime_stack,process->runtime_stack_owner);process->runtime_stack=0;}
    if(process->runtime_image){(void)kmem_release(process->runtime_image,process->runtime_image_owner);process->runtime_image=0;}
    for(i=0;i<USER_FILE_HANDLES;++i)process->files[i].open=0;
    for(j=0;j<USER_PROCESS_SLOTS;++j)if(process_table[j].state!=USER_PROCESS_FREE&&
        process_table[j].parent_pid==pid)process_table[j].parent_pid=0;
    user_terminal_release(pid);
    if(gui_keyboard_pids[process->terminal]==pid||kernel_user_service_pid("SYSTEM.GUI")==pid){
        if(gui_desktop_pids[process->terminal]==pid||kernel_user_service_pid("SYSTEM.GUI")==pid){
            kgfx_restore_console();gui_desktop_pids[process->terminal]=0;}
        kgfx_cursor_hide();
        gui_keyboard_pids[process->terminal]=0;gui_keyboard_ready_tick=0;gui_keyboard_dropped=0;
        gui_keyboard_persistent=0;}
    user_gui_notify_exit(pid,-3);user_service_remove_pid(pid);ksurface_release_owner(pid);
    process->image=0;process->image_size=0;process->runtime_stack_size=0;
    process->pid=0;process->parent_pid=0;
    process->program_name[0]='\0';process->arguments[0]='\0';
    user_record_completion(pid,-3);process->state=USER_PROCESS_FREE;return 1;
}

int kernel_user_process_resources(uint32_t slot,uint32_t info[4])
{
    uint8_t i;uint32_t private_bytes=0,open_files=0;user_process_t *process;
    if(slot>=USER_PROCESS_SLOTS||!info)return 0;
    process=&process_table[slot];if(process->state==USER_PROCESS_FREE)return 0;
    for(i=0;i<USER_MEMORY_BLOCKS;++i)if(process->memory[i].pointer)
        private_bytes+=process->memory[i].size;
    for(i=0;i<USER_FILE_HANDLES;++i)if(process->files[i].open)++open_files;
    info[0]=process->image_size;info[1]=process->runtime_stack_size;
    info[2]=private_bytes;info[3]=open_files;return 1;
}

uint32_t kernel_user_process_cpu_ticks(uint32_t slot)
{
    if(slot>=USER_PROCESS_SLOTS||process_table[slot].state==USER_PROCESS_FREE)return 0;
    return process_table[slot].cpu_ticks;
}

uint32_t kernel_user_process_stack_used(uint32_t slot)
{
    const user_process_t *process;const uint8_t *stack;uint32_t untouched=0;
    if(slot>=USER_PROCESS_SLOTS)return 0;
    process=&process_table[slot];
    if(process->state==USER_PROCESS_FREE||!process->runtime_stack)return 0;
    stack=(const uint8_t *)process->runtime_stack+USER_STACK_GUARD_BYTES;
    while(untouched<process->runtime_stack_size&&
          stack[untouched]==USER_STACK_UNUSED)++untouched;
    return process->runtime_stack_size-untouched;
}

/* Return a validated, read-only description of one process address space.
   The layout deliberately describes policy before an MMU starts enforcing it. */
int kernel_user_process_map(uint32_t pid,uint32_t info[16])
{
    uint8_t i;const user_process_t *process=0;const uint8_t *image;
    uint32_t base,file_size,executable_end,writable_start,policy_start;
    if(!pid||!info)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){process=&process_table[i];break;}
    if(!process||!process->image||!process->runtime_stack)return 0;
    image=process->image;base=(uint32_t)image;file_size=user_be32(image+12u);
    if(file_size<44u||file_size>process->image_size||
       user_be32(image+file_size-12u)!=0x41505732u)return 0;
    executable_end=user_be32(image+file_size-8u);
    writable_start=user_be32(image+file_size-4u);
    policy_start=file_size-12u;
    if(executable_end<32u||executable_end>writable_start||
       writable_start>policy_start)return 0;
    info[0]=pid;info[1]=(uint32_t)process->memory_owner;
    info[2]=base+32u;info[3]=base+executable_end;
    info[4]=base+executable_end;info[5]=base+writable_start;
    info[6]=base+writable_start;info[7]=base+policy_start;
    info[8]=base+policy_start;info[9]=base+file_size;
    info[10]=base+file_size;info[11]=base+process->image_size;
    info[12]=(uint32_t)process->runtime_stack+USER_STACK_GUARD_BYTES;
    info[13]=info[12]+process->runtime_stack_size;
    info[14]=process->runtime_image?0u:1u;
    info[15]=kernel_user_process_stack_used(i);
    return 1;
}

int kernel_user_set_watchdog(uint32_t pid,uint32_t ticks)
{
    uint8_t i;
    if(!pid||ticks>180000u)return -1;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){
        if(process_table[i].parent_pid!=0u||process_table[i].state==USER_PROCESS_RUNNING)return -1;
        process_table[i].watchdog_ticks=ticks;return 1;
    }
    return 0;
}

int kernel_user_foreground(uint32_t pid)
{
    uint8_t i;if(!pid||terminal_owner_pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&process_table[i].terminal==ksessions_current()){
        if(process_table[i].state==USER_PROCESS_SUSPENDED)
            process_table[i].state=USER_PROCESS_READY;
        if(process_table[i].state==USER_PROCESS_READY||
           process_table[i].state==USER_PROCESS_SLEEPING||
           process_table[i].state==USER_PROCESS_INPUT){terminal_owner_pid=pid;return 1;}}
    return 0;
}

int kernel_user_resume(uint32_t pid)
{
    uint8_t i;if(!pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){
        if(process_table[i].state!=USER_PROCESS_SUSPENDED)return -1;
        process_table[i].state=USER_PROCESS_READY;return 1;}
    return 0;
}

int kernel_user_suspend(uint32_t pid)
{
    uint8_t i;if(!pid)return 0;
    for(i=0;i<SESSION_COUNT;i++)if(terminal_owners[i]==pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){
        if(process_table[i].state!=USER_PROCESS_READY&&
           process_table[i].state!=USER_PROCESS_SLEEPING&&
           process_table[i].state!=USER_PROCESS_INPUT)return -1;
        process_table[i].state=USER_PROCESS_SUSPENDED;return 1;}
    return 0;
}

int kernel_user_terminal_active(void){return terminal_owner_pid!=0u;}

int kernel_user_gui_keyboard_active(void){return gui_keyboard_pid&&kernel_user_pid_terminal(gui_keyboard_pid)==ksessions_current();}

int kernel_user_gui_keyboard_wake(void)
{
    uint8_t i;int32_t now,delta_x,delta_y;uint32_t type,scan,modifiers,character,buttons;
    user_process_t *target=0;user_message_t *message;
    uint32_t pid=gui_keyboard_pid;
    if(!pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE&&
       process_table[i].state!=USER_PROCESS_TERMINATED){target=&process_table[i];break;}
    if(!target){kgfx_cursor_hide();gui_keyboard_pid=0;gui_keyboard_ready_tick=0;
        gui_keyboard_dropped=0;gui_keyboard_persistent=0;return 0;}
    now=kernel_user_yield();
    if(gui_keyboard_ready_tick&&
       (now<0||(int32_t)((uint32_t)now-gui_keyboard_ready_tick)<0)){
        /* This runs after the main loop has polled IKBD, unlike syscall 45.
           Drain complete make/break traffic from the command that launched
           the GUI application before accepting a fresh key. */
        for(i=0;i<255u;++i)(void)kernel_user_terminal_getchar();
        kernel_user_mouse_flush();
        return 0;
    }
    if(gui_keyboard_ready_tick){
        for(i=0;i<255u;++i)(void)kernel_user_terminal_getchar();
        kernel_user_mouse_flush();
        gui_keyboard_ready_tick=0;return 0;
    }
    if(gui_keyboard_persistent&&kernel_user_mouse_event(&buttons,&delta_x,&delta_y)){
        const kgfx_info_t *gfx=kgfx_get_info();
        gui_mouse_x=ksessions_mouse_x();gui_mouse_y=ksessions_mouse_y();
        if(gui_mouse_x<0)gui_mouse_x=0;
        else if((uint32_t)gui_mouse_x>=gfx->width)gui_mouse_x=(int32_t)(gfx->width-1u);
        if(gui_mouse_y<0)gui_mouse_y=0;
        else if((uint32_t)gui_mouse_y>=gfx->height)gui_mouse_y=(int32_t)(gfx->height-1u);
        kgfx_cursor_move(gui_mouse_x,gui_mouse_y);
        if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
            message=target->wait_message_pointer;target->wait_message_pointer=0;
            target->saved_registers[0]=0;target->state=USER_PROCESS_READY;
        }else{
            if(target->mailbox_count>=USER_MAILBOX_INPUT_DEPTH){uint8_t index=target->mailbox_tail;
                ++gui_keyboard_dropped;
                /* Motion is state, not a lossless byte stream. Preserve the
                   newest position and especially button release by replacing
                   the newest queued mouse event. Never overwrite a key. */
                for(i=0;i<target->mailbox_count;++i){
                    index=(uint8_t)((index+USER_MAILBOX_DEPTH-1u)%USER_MAILBOX_DEPTH);
                    message=&target->mailbox[index];
                    if(message->sender==0u&&message->value[0]==0x4b4d4f55u){
                        uint32_t pending_press=message->value[2]&0x1c000000u;
                        message->value[1]=((uint32_t)gui_mouse_x<<16)|(uint32_t)gui_mouse_y;
                        message->value[2]=pending_press|((buttons&31u)<<24)|
                            ((uint32_t)(uint8_t)delta_x<<8)|(uint8_t)delta_y;
                        message->value[3]=gui_keyboard_dropped;gui_keyboard_dropped=0;return 1;
                    }
                }
                return 0;
            }
            message=&target->mailbox[target->mailbox_tail];
            target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
            ++target->mailbox_count;
        }
        message->sender=0;message->value[0]=0x4b4d4f55u; /* KMOU */
        message->value[1]=((uint32_t)gui_mouse_x<<16)|(uint32_t)gui_mouse_y;
        message->value[2]=((buttons&31u)<<24)|((uint32_t)(uint8_t)delta_x<<8)|(uint8_t)delta_y;
        message->value[3]=gui_keyboard_dropped;gui_keyboard_dropped=0;
        return 1;
    }
    if(target->state!=USER_PROCESS_MESSAGE&&target->mailbox_count>=USER_MAILBOX_INPUT_DEPTH)return 0;
    if(!kernel_user_keyboard_event(&type,&scan,&modifiers,&character))return 0;
    if(!gui_keyboard_persistent){gui_keyboard_pid=0;gui_keyboard_ready_tick=0;}
    if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
        message=target->wait_message_pointer;target->wait_message_pointer=0;
        target->saved_registers[0]=0;target->state=USER_PROCESS_READY;
    }else{
        if(target->mailbox_count>=USER_MAILBOX_INPUT_DEPTH){++gui_keyboard_dropped;return 0;}
        message=&target->mailbox[target->mailbox_tail];
        target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
        ++target->mailbox_count;
    }
    message->sender=0;message->value[0]=0x4b4b4559u; /* KKEY */
    message->value[1]=(type<<24)|((modifiers&0xffu)<<16)|(scan&0x7fu);
    message->value[2]=character;message->value[3]=gui_keyboard_dropped;
    gui_keyboard_dropped=0;
    return 1;
}

int kernel_user_terminal_wake(void)
{
    uint8_t i;int32_t character;uint32_t owner;user_process_t *process=0;
    if(!terminal_owner_pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==terminal_owner_pid&&
       process_table[i].state!=USER_PROCESS_FREE){process=&process_table[i];break;}
    if(!process){terminal_owner_pid=0;kernel_user_terminal_reset_modifiers();return 0;}
    character=kernel_user_terminal_getchar();if(character<0)return 0;
    if(character==3){
        owner=terminal_owner_pid;
        kernel_user_terminal_reset_modifiers();
        if(services&&services->console_puts){services->console_puts("\n^C\n");user_console_dirty=1u;}
        return kernel_user_kill(owner)>0?2:0;
    }
    if(character==26){
        process->state=USER_PROCESS_SUSPENDED;terminal_owner_pid=0;
        kernel_user_terminal_reset_modifiers();
        if(services&&services->console_puts){services->console_puts("\n^Z\n");user_console_dirty=1u;}
        return 3;
    }
    if(process->state!=USER_PROCESS_INPUT)return 0;
    process->saved_registers[0]=(uint32_t)character;
    process->state=USER_PROCESS_READY;return 1;
}

static void user_set_program_name(user_process_t *process,const char *path)
{
    const char *base=path;uint8_t i;
    if(!path){process->program_name[0]='\0';return;}
    for(i=0;path[i];++i)if(path[i]=='\\'||path[i]=='/')base=path+i+1;
    for(i=0;i<12u&&base[i];++i)process->program_name[i]=base[i];
    process->program_name[i]='\0';
}

int kernel_user_suspend_parent(void)
{if(!user_active||suspended_depth>=USER_PROCESS_SLOTS-1u)return 0;
 suspended_parents[suspended_depth++]=current_process;current_process->state=USER_PROCESS_WAITING;return 1;}

uint32_t kernel_user_current_pid(void)
{return current_process->pid;}

void kernel_user_resume_parent(void)
{if(suspended_depth){current_process=suspended_parents[--suspended_depth];
 suspended_parents[suspended_depth]=0;current_process->state=USER_PROCESS_RUNNING;}}

void kernel_user_set_arguments(const char *arguments)
{uint8_t i;for(i=0;i<63u&&arguments&&arguments[i];++i)pending_arguments[i]=arguments[i];pending_arguments[i]='\0';}

int kernel_user_begin(const void *image,uint32_t size,const char *working_directory,
                      void *stack_top)
{uint8_t i,slot;uint32_t stack_size;
 stack_size=(image&&size>=32u)?user_be32((const uint8_t *)image+24u):0u;
 for(i=0;i<USER_PROCESS_SLOTS;++i){slot=(uint8_t)((next_process_slot+i)%USER_PROCESS_SLOTS);
     if(process_table[slot].state==USER_PROCESS_FREE)break;}
 if(i==USER_PROCESS_SLOTS)return 0;
 current_process=&process_table[slot];current_process->terminal=(uint8_t)ksessions_current();next_process_slot=(uint8_t)((slot+1u)%USER_PROCESS_SLOTS);
 user_image=(const uint8_t *)image;user_image_size=size;user_syscall_result=0;
 current_process->saved_usp=0;current_process->saved_pc=0;current_process->saved_sr=0;
 current_process->registers_valid=0;
 current_process->runtime_stack=stack_top&&stack_size?
     (uint8_t *)stack_top-stack_size-USER_STACK_GUARD_BYTES:0;
 current_process->runtime_stack_size=current_process->runtime_stack?stack_size:0u;
 current_process->runtime_image=(void *)image;
 /* SHELL.SYS owns foreground RUN allocations as 0x0200/0x0201.  Pin them
    before entering user mode; the shell may regain control at any scheduler
    quantum, but only kernel_user_end is allowed to release these blocks. */
 current_process->runtime_stack_owner=current_process->runtime_stack?0x0201u:0u;
 current_process->runtime_image_owner=image?0x0200u:0u;
 current_process->wake_tick=0;
 current_process->cpu_ticks=0;
 current_process->watchdog_ticks=0;
 current_process->wait_pid=0;current_process->wait_status_pointer=0;
 current_process->wait_message_pointer=0;
 user_mailbox_reset(current_process);
 current_process->pid=user_allocate_pid();
 current_process->parent_pid=0;
 user_set_program_name(current_process,"RUN.APP");
 user_memory_owner=user_owner_for_slot(slot);
 for(i=0;i<63u&&working_directory&&working_directory[i];++i)
     user_working_directory[i]=working_directory[i];
 user_working_directory[i]='\0';
 if(!i){user_working_directory[0]='\\';user_working_directory[1]='\0';}
 for(i=0;i<USER_FILE_HANDLES;++i)user_files[i].open=0;
 for(i=0;i<USER_MEMORY_BLOCKS;++i){user_memory[i].pointer=0;user_memory[i].allocation=0;user_memory[i].size=0;}
 for(i=0;i<63u&&pending_arguments[i];++i){user_arguments[i]=pending_arguments[i];}
 user_arguments[i]='\0';pending_arguments[0]='\0';
 if(current_process->runtime_stack)user_stack_init(current_process);
 if(image&&!kmem_pin((void *)image,current_process->runtime_image_owner))return 0;
 if(current_process->runtime_stack&&
    !kmem_pin(current_process->runtime_stack,current_process->runtime_stack_owner)){
     (void)kmem_release((void *)image,current_process->runtime_image_owner);return 0;}
 current_process->state=USER_PROCESS_READY;current_process->state=USER_PROCESS_RUNNING;
 return 1;}

void kernel_user_end(void)
{uint8_t i;for(i=0;i<USER_MEMORY_BLOCKS;++i)if(user_memory[i].pointer){
    if(!user_heap_valid(&user_memory[i]))user_syscall_result=USER_HEAP_FAULT;
    (void)user_heap_release(&user_memory[i],user_memory_owner);}
 if(current_process->runtime_stack){
    if(current_process->runtime_stack_owner)
        (void)kmem_release(current_process->runtime_stack,current_process->runtime_stack_owner);
    current_process->runtime_stack=0;}
 current_process->runtime_stack_size=0;
 if(current_process->runtime_image){(void)kmem_release(current_process->runtime_image,current_process->runtime_image_owner);current_process->runtime_image=0;}
 /* Children must never retain a PID whose process record is leaving.  PID 0
    denotes the kernel/shell reaper and lets an orphan finish autonomously. */
 for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].state!=USER_PROCESS_FREE&&
     process_table[i].parent_pid==current_process->pid)process_table[i].parent_pid=0;
 user_terminal_release(current_process->pid);
 if(gui_keyboard_pid==current_process->pid||
     kernel_user_service_pid("SYSTEM.GUI")==current_process->pid){
     if(gui_desktop_pid==current_process->pid||
        kernel_user_service_pid("SYSTEM.GUI")==current_process->pid){
         kgfx_restore_console();gui_desktop_pid=0;}
     kgfx_cursor_hide();
     gui_keyboard_pid=0;gui_keyboard_ready_tick=0;gui_keyboard_dropped=0;
     gui_keyboard_persistent=0;}
 user_gui_notify_exit(current_process->pid,user_syscall_result);
 user_service_remove_pid(current_process->pid);ksurface_release_owner(current_process->pid);
 current_process->state=USER_PROCESS_TERMINATED;user_image=0;user_image_size=0;user_arguments[0]='\0';}

static void user_wake_sleepers(uint32_t now)
{uint8_t i;for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].state==USER_PROCESS_SLEEPING&&
    (int32_t)(now-process_table[i].wake_tick)>=0){process_table[i].state=USER_PROCESS_READY;
        process_table[i].saved_registers[0]=now;}}

/* Weighted round robin. The task whose session is on screen gets a big
 * slice (priority 3), the background ones small slices (priority 1), so a
 * background GEM app cannot monopolise the CPU while its window is hidden.
 * Because the weight is derived from the active session, no new API is
 * needed: switching windows/sessions re-weights the tasks automatically. */
static uint8_t user_task_quantum(const user_process_t *p)
{
    uint8_t priority=(uint8_t)(p->terminal==(uint8_t)ksessions_current()?3u:1u);
    return (uint8_t)(2u<<priority);          /* 8 or 16 ticks = 160 / 320 ms */
}
static user_process_t *user_next_ready(void)
{
 uint8_t start=(uint8_t)(current_process-process_table),i,slot;
 if(current_process->state==USER_PROCESS_RUNNING){
     if(current_process->quantum_left){--current_process->quantum_left;return current_process;}
     current_process->quantum_left=user_task_quantum(current_process);
 }
 for(i=1;i<=USER_PROCESS_SLOTS;++i){slot=(uint8_t)((start+i)%USER_PROCESS_SLOTS);
     if(process_table[slot].state==USER_PROCESS_READY){
         process_table[slot].quantum_left=user_task_quantum(&process_table[slot]);
         return &process_table[slot];}}
 return 0;}

static void user_finalize_ended_current(void)
{
    uint8_t i;uint32_t ended_pid=current_process->pid;
    int32_t ended_status=current_process->syscall_result;int reaped=0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].state==USER_PROCESS_WAITING&&
        process_table[i].wait_pid==ended_pid){
        if(process_table[i].wait_status_pointer)
            *process_table[i].wait_status_pointer=ended_status;
        process_table[i].wait_pid=0;process_table[i].wait_status_pointer=0;
        process_table[i].saved_registers[0]=0;process_table[i].state=USER_PROCESS_READY;
        current_process->state=USER_PROCESS_FREE;current_process->pid=0;
        current_process->parent_pid=0;reaped=1;break;
    }
    if(!reaped&&current_process->parent_pid==0){
        user_record_completion(ended_pid,ended_status);
        current_process->state=USER_PROCESS_FREE;current_process->pid=0;
    }
}

void kernel_user_finish_fault(void)
{user_finalize_ended_current();}

int32_t kernel_user_clone(uint32_t entry,uint32_t *child_pid)
{
    user_process_t *parent=current_process,*child=0;uint8_t i,slot,j;
    uint8_t *image,*stack;uint32_t entry_offset,copy_offset,parent_stack,parent_stack_end;
    if(child_pid)*child_pid=0;
    if(!user_active||!user_pc_valid_image(user_image,user_image_size,entry))return -0x360;
    if(parent->runtime_image&&kmem_owner(parent->runtime_image)!=parent->runtime_image_owner)
        return -0x366;
    if(parent->runtime_stack&&kmem_owner(parent->runtime_stack)!=parent->runtime_stack_owner)
        return -0x366;
    for(i=0;i<USER_PROCESS_SLOTS;++i){slot=(uint8_t)((next_process_slot+i)%USER_PROCESS_SLOTS);
        if(process_table[slot].state==USER_PROCESS_FREE){child=&process_table[slot];break;}}
    if(!child)return -0x361;
    child->memory_owner=user_owner_for_slot(slot);
    entry_offset=entry-(uint32_t)(uintptr_t)user_image;
    image=(uint8_t *)kmem_alloc(user_image_size,16u,child->memory_owner,
                                KMEM_READ|KMEM_WRITE|KMEM_EXEC|KMEM_SUPERVISOR,1);
    if(!image)return -0x362;
    for(copy_offset=0;copy_offset<user_image_size;++copy_offset)
        image[copy_offset]=user_image[copy_offset];
    /* Stack is CPU-only (never DMA), so it can live in the faster TT-RAM. */
    stack=kmem_alloc(2048u+2u*USER_STACK_GUARD_BYTES,16u,child->memory_owner,
                     KMEM_READ|KMEM_WRITE,1);
    if(!stack){(void)kmem_free(image,child->memory_owner);return -0x362;}
    parent_stack=(uint32_t)(uintptr_t)parent->runtime_stack;
    parent_stack_end=parent_stack+parent->runtime_stack_size+2u*USER_STACK_GUARD_BYTES;
    if(parent_stack&&((uint32_t)(uintptr_t)stack<parent_stack_end&&
       (uint32_t)(uintptr_t)stack+2048u+2u*USER_STACK_GUARD_BYTES>parent_stack)){
        (void)kmem_free(stack,child->memory_owner);
        (void)kmem_free(image,child->memory_owner);return -0x365;
    }
    child->pid=user_allocate_pid();
    if(!child->pid){(void)kmem_free(stack,child->memory_owner);
        (void)kmem_free(image,child->memory_owner);return -0x361;}
    child->parent_pid=parent->pid;child->terminal=parent->terminal;
    child->image=image;child->image_size=parent->image_size;
    child->runtime_image=image;child->runtime_image_owner=child->memory_owner;
    child->runtime_stack=stack;
    child->saved_usp=(uint32_t)(uintptr_t)stack+USER_STACK_GUARD_BYTES+2048u;
    child->runtime_stack_size=2048u;
    child->runtime_stack_owner=child->memory_owner;
    user_stack_init(child);
    (void)kmem_pin(image,child->memory_owner);(void)kmem_pin(stack,child->memory_owner);
    child->wake_tick=0;
    child->cpu_ticks=0;
    child->watchdog_ticks=0;
    child->wait_pid=0;child->wait_status_pointer=0;
    child->wait_message_pointer=0;
    user_mailbox_reset(child);
    child->saved_pc=(uint32_t)(uintptr_t)(image+entry_offset);
    child->saved_sr=0x0500u;child->syscall_result=0;
    for(j=0;j<15u;++j)child->saved_registers[j]=0;
    for(j=0;j<USER_FILE_HANDLES;++j)child->files[j].open=0;
    for(j=0;j<USER_MEMORY_BLOCKS;++j){child->memory[j].pointer=0;child->memory[j].allocation=0;child->memory[j].size=0;}
    for(j=0;j<64u;++j){child->working_directory[j]=parent->working_directory[j];child->arguments[j]='\0';}
    for(j=0;j<13u;++j)child->program_name[j]=parent->program_name[j];
    kernel_sync_user_image();child->registers_valid=1u;child->state=USER_PROCESS_READY;
    next_process_slot=(uint8_t)((slot+1u)%USER_PROCESS_SLOTS);
    if(child_pid)*child_pid=child->pid;
    return 0;
}

int32_t kernel_user_create_loaded(void *image,uint32_t image_size,void *stack,
                                  uint32_t stack_size,uint32_t entry,
                                  const char *working_directory,const char *arguments,
                                  const char *program_path,uint16_t image_owner,
                                  uint16_t stack_owner,uint32_t *pid)
{
    user_process_t *parent=current_process,*child=0;uint8_t i,slot,j;
    if(pid)*pid=0;
    if(!image||!stack||!stack_size||!user_pc_valid_image((const uint8_t *)image,image_size,entry))return -0x390;
    for(i=0;i<USER_PROCESS_SLOTS;++i){slot=(uint8_t)((next_process_slot+i)%USER_PROCESS_SLOTS);
        if(process_table[slot].state==USER_PROCESS_FREE){child=&process_table[slot];break;}}
    if(!child)return -0x391;
    child->pid=user_allocate_pid();if(!child->pid)return -0x391;
    child->parent_pid=user_active?parent->pid:0u;
    child->terminal=(uint8_t)kernel_user_birth_terminal();
    user_set_program_name(child,program_path);
    child->memory_owner=user_owner_for_slot(slot);
    if(!kmem_change_owner(image,image_owner,child->memory_owner)){
        child->pid=0;child->state=USER_PROCESS_FREE;return -0x392;
    }
    if(!kmem_change_owner(stack,stack_owner,child->memory_owner)){
        (void)kmem_change_owner(image,child->memory_owner,image_owner);
        child->pid=0;child->state=USER_PROCESS_FREE;
        return -0x392;
    }
    child->image=(const uint8_t *)image;child->image_size=image_size;
    child->runtime_image=image;child->runtime_image_owner=child->memory_owner;
    child->runtime_stack=stack;child->runtime_stack_owner=child->memory_owner;
    child->runtime_stack_size=stack_size;
    child->saved_usp=(uint32_t)(uintptr_t)stack+USER_STACK_GUARD_BYTES+stack_size;
    user_stack_init(child);(void)kmem_pin(image,child->memory_owner);
    (void)kmem_pin(stack,child->memory_owner);child->saved_pc=entry;
    child->saved_sr=0x0500u;child->wake_tick=0;child->cpu_ticks=0;child->watchdog_ticks=0;
    child->wait_pid=0;child->wait_status_pointer=0;
    child->wait_message_pointer=0;
    user_mailbox_reset(child);
    child->syscall_result=0;
    for(j=0;j<15u;++j)child->saved_registers[j]=0;
    for(j=0;j<USER_FILE_HANDLES;++j)child->files[j].open=0;
    for(j=0;j<USER_MEMORY_BLOCKS;++j){child->memory[j].pointer=0;child->memory[j].allocation=0;child->memory[j].size=0;}
    for(j=0;j<63u&&working_directory&&working_directory[j];++j)child->working_directory[j]=working_directory[j];
    child->working_directory[j]='\0';if(!j){child->working_directory[0]='\\';child->working_directory[1]='\0';}
    for(j=0;j<63u&&arguments&&arguments[j];++j)child->arguments[j]=arguments[j];
    child->arguments[j]='\0';
    child->registers_valid=1u;child->state=USER_PROCESS_READY;
    next_process_slot=(uint8_t)((slot+1u)%USER_PROCESS_SLOTS);
    if(pid)*pid=child->pid;
    return 0;
}

uint32_t kernel_user_take_ready(uint32_t *registers,uint32_t *pc,uint16_t *sr)
{
    uint8_t i,j,slot;
    if(!registers||!pc||!sr)return 0;
    /* No user IRQ can wake sleepers when all users are blocked. */
    user_wake_sleepers(kernel_user_clock());
    for(i=0;i<USER_PROCESS_SLOTS;++i){slot=(uint8_t)((next_process_slot+i)%USER_PROCESS_SLOTS);
        if(process_table[slot].state==USER_PROCESS_READY){
            current_process=&process_table[slot];current_process->state=USER_PROCESS_RUNNING;
            next_process_slot=(uint8_t)((slot+1u)%USER_PROCESS_SLOTS);
            for(j=0;j<15u;++j)registers[j]=current_process->saved_registers[j];
            *pc=current_process->saved_pc;*sr=current_process->saved_sr;
            return current_process->saved_usp;}}
    return 0;
}

static uint32_t user_switch_frame(uint32_t *registers,uint32_t usp,uint32_t *pc,
                                  uint16_t *sr,int save_current,int return_tick)
{
    user_process_t *next;uint8_t i;int32_t now=kernel_user_yield();
    if(now<0||!registers||!pc||!sr)return 0;
    if(save_current&&!user_heaps_valid_current()){
        user_heap_fail_current();save_current=0;
    }
    if(save_current&&!user_stack_valid(current_process,usp)){
        user_stack_fail_current();save_current=0;
    }
    if(!save_current)user_finalize_ended_current();
    user_wake_sleepers((uint32_t)now);next=user_next_ready();
    if(!next){
        if(save_current&&kernel_dispatch_active_user()){
            for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
            if(return_tick)current_process->saved_registers[0]=(uint32_t)now;
            current_process->saved_usp=usp;current_process->saved_pc=*pc;
            current_process->saved_sr=*sr;current_process->registers_valid=1u;
            current_process->state=USER_PROCESS_READY;
            if(user_console_flags[current_process->terminal]){
                unsigned t=current_process->terminal;user_console_flags[t]=0;return t==ksessions_current()?2u:1u;}
            return 1u; /* Reserved sentinels: return to supervisor scheduler. */
        }
        return 0;
    }
    if(save_current){for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
        if(return_tick)current_process->saved_registers[0]=(uint32_t)now;
        current_process->saved_usp=usp;current_process->saved_pc=*pc;current_process->saved_sr=*sr;
        current_process->registers_valid=1u;current_process->state=USER_PROCESS_READY;}
    current_process=next;current_process->state=USER_PROCESS_RUNNING;
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;
    return current_process->saved_usp;
}

uint32_t kernel_user_switch_context(uint32_t *registers,uint32_t usp,uint32_t *pc,uint16_t *sr)
{return user_switch_frame(registers,usp,pc,sr,1,1);}

uint32_t kernel_user_preempt_context(uint32_t *registers,uint32_t usp,uint32_t *pc,uint16_t *sr)
{
    uint8_t i;int32_t now;
    if(!user_active||!registers||!pc||!sr)return 0;
    if(user_active){
        ++current_process->cpu_ticks;
        if(current_process->watchdog_ticks&&!--current_process->watchdog_ticks){
            current_process->syscall_result=-4;
            kernel_user_end();
            return user_switch_frame(registers,0,pc,sr,0,0);
        }
    }
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    now=kernel_user_yield();
    if(now<0)return 0;
    user_wake_sleepers((uint32_t)now);
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_usp=usp;current_process->saved_pc=*pc;
    current_process->saved_sr=*sr;current_process->registers_valid=1u;
    current_process->state=USER_PROCESS_READY;
    /* Always pass an IRQ quantum through the supervisor loop. Directly
       hopping forever between two READY CPU-bound contexts would starve the
       shell, IKBD polling and administrative watchdog controls. */
    if(user_console_flags[current_process->terminal]){
                unsigned t=current_process->terminal;user_console_flags[t]=0;return t==ksessions_current()?2u:1u;}
    return 1u;
}

uint32_t kernel_user_switch_after_exit(uint32_t *registers,uint32_t *pc,uint16_t *sr)
{return user_switch_frame(registers,0,pc,sr,0,0);}

uint32_t kernel_user_sleep_context(uint32_t *registers,uint32_t usp,uint32_t *pc,
                                   uint16_t *sr,uint32_t ticks)
{
    user_process_t *next;uint8_t i;int32_t now;
    if(!ticks||ticks>3000u||!registers||!pc||!sr)return 0;
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    now=kernel_user_yield();if(now<0)return 0;
    user_wake_sleepers((uint32_t)now);next=user_next_ready();if(!next)return 0;
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_registers[0]=(uint32_t)now+ticks;
    current_process->saved_usp=usp;current_process->saved_pc=*pc;current_process->saved_sr=*sr;
    current_process->wake_tick=(uint32_t)now+ticks;current_process->registers_valid=1u;
    current_process->state=USER_PROCESS_SLEEPING;
    current_process=next;current_process->state=USER_PROCESS_RUNNING;
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;return current_process->saved_usp;
}

uint32_t kernel_user_wait_input_context(uint32_t *registers,uint32_t usp,
                                        uint32_t *pc,uint16_t *sr)
{
    user_process_t *next;uint8_t i;
    if(!user_active||!registers||!pc||!sr||!current_process->runtime_image||
       !kernel_dispatch_active_user())return 0;
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_registers[0]=0;
    current_process->saved_usp=usp;current_process->saved_pc=*pc;
    current_process->saved_sr=*sr;current_process->registers_valid=1u;
    current_process->state=USER_PROCESS_INPUT;next=user_next_ready();
    if(!next){if(user_console_flags[current_process->terminal]){
                unsigned t=current_process->terminal;user_console_flags[t]=0;return t==ksessions_current()?2u:1u;}return 1u;}
    current_process=next;current_process->state=USER_PROCESS_RUNNING;
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;
    return current_process->saved_usp;
}

uint32_t kernel_user_wait_message_context(uint32_t *registers,uint32_t usp,
                                          uint32_t *pc,uint16_t *sr,
                                          user_message_t *destination)
{
    user_process_t *next;uint8_t i;
    if(!user_active||!registers||!pc||!sr||
       !user_write_aligned(destination,sizeof(*destination),2u))return 0;
    if(current_process->mailbox_count)return 0; /* regular syscall receives it */
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_registers[0]=0;
    current_process->saved_usp=usp;current_process->saved_pc=*pc;
    current_process->saved_sr=*sr;current_process->registers_valid=1u;
    current_process->wait_message_pointer=destination;
    current_process->state=USER_PROCESS_MESSAGE;next=user_next_ready();
    if(!next){if(user_console_flags[current_process->terminal]){
                unsigned t=current_process->terminal;user_console_flags[t]=0;return t==ksessions_current()?2u:1u;}return 1u;}
    current_process=next;current_process->state=USER_PROCESS_RUNNING;
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;
    return current_process->saved_usp;
}

uint32_t kernel_user_waitpid_context(uint32_t *registers,uint32_t usp,uint32_t *pc,
                                     uint16_t *sr,uint32_t pid,int32_t *status)
{
    user_process_t *child=0;uint8_t i;
    if(!pid||!registers||!pc||!sr||!user_write_aligned(status,4u,2u))return 0;
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
        process_table[i].state!=USER_PROCESS_FREE){child=&process_table[i];break;}
    if(!child||child->parent_pid!=current_process->pid||child->state!=USER_PROCESS_READY)return 0;
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_registers[0]=0;current_process->saved_usp=usp;
    current_process->saved_pc=*pc;current_process->saved_sr=*sr;
    current_process->wait_pid=pid;current_process->wait_status_pointer=status;
    current_process->registers_valid=1u;current_process->state=USER_PROCESS_WAITING;
    current_process=child;current_process->state=USER_PROCESS_RUNNING;
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;return current_process->saved_usp;
}

int kernel_user_fault(uint32_t vector)
{int32_t status;if(!user_active)return 0;status=-(int32_t)(0x100u+vector);
 user_syscall_result=status;kernel_user_end();return status;}

static int32_t kernel_user_child_query(uint32_t pid,int reap,uint32_t *state,int32_t *status)
{
    uint8_t i;uint32_t owner=current_process->pid;
    if(!pid||!state||!status||reap<0||reap>1)return -0x370;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
        process_table[i].state!=USER_PROCESS_FREE){
        if(process_table[i].parent_pid!=owner)return -0x372;
        *state=process_table[i].state;*status=process_table[i].syscall_result;
        if(reap&&process_table[i].state==USER_PROCESS_TERMINATED){
            process_table[i].state=USER_PROCESS_FREE;process_table[i].pid=0;
            process_table[i].parent_pid=0;}
        return 0;
    }
    return -0x371;
}

/* Overflow-safe half-open interval check: [address,address+bytes) must fit
   wholly inside [base,base+size). Subtraction is performed only after the
   lower bound has been established, so neither endpoint needs to be added. */
static int user_subrange(uint32_t address,uint32_t bytes,uint32_t base,uint32_t size)
{
    return address>=base&&bytes<=size&&address-base<=size-bytes;
}

static int user_process_range(const user_process_t *process,const void *pointer,uint32_t bytes)
{uint32_t address=(uint32_t)(uintptr_t)pointer,base=(uint32_t)(uintptr_t)process->image,i;
 if(user_subrange(address,bytes,base,process->image_size))return 1;
 if(process->runtime_stack){uint32_t stack=(uint32_t)(uintptr_t)process->runtime_stack+USER_STACK_GUARD_BYTES;
     if(user_subrange(address,bytes,stack,process->runtime_stack_size))return 1;}
 for(i=0;i<USER_MEMORY_BLOCKS;++i)if(process->memory[i].pointer){uint32_t block=(uint32_t)(uintptr_t)process->memory[i].pointer;
     if(user_subrange(address,bytes,block,process->memory[i].size))return 1;}
 return 0;}
static int user_range(const void *pointer,uint32_t bytes)
{return user_process_range(current_process,pointer,bytes);}

static int user_write_range(const void *pointer,uint32_t bytes)
{
 uint32_t address=(uint32_t)(uintptr_t)pointer,base=(uint32_t)(uintptr_t)user_image,i;
 uint32_t file_size,writable_start;
 /* New applications end their file-backed image with APW2, executable end
    and writable offset.
    Keep metadata itself read-only and admit the separately zeroed BSS tail. */
 if(user_image_size>=44u){
     file_size=user_be32(user_image+12u);
     if(file_size>=44u&&file_size<=user_image_size&&
        user_be32(user_image+file_size-12u)==0x41505732u){
         writable_start=user_be32(user_image+file_size-4u);
         if(writable_start>=32u&&writable_start<=file_size-12u){
             if(user_subrange(address,bytes,base+writable_start,
                              file_size-12u-writable_start))return 1;
             if(user_subrange(address,bytes,base+file_size,
                              user_image_size-file_size))return 1;
         }
     }
 }
 if(current_process->runtime_stack){uint32_t stack=(uint32_t)(uintptr_t)current_process->runtime_stack+USER_STACK_GUARD_BYTES;
     if(user_subrange(address,bytes,stack,current_process->runtime_stack_size))return 1;}
 for(i=0;i<USER_MEMORY_BLOCKS;++i)if(user_memory[i].pointer){uint32_t block=(uint32_t)(uintptr_t)user_memory[i].pointer;
     if(user_subrange(address,bytes,block,user_memory[i].size))return 1;}
 return 0;
}

static int user_write_aligned(const void *pointer,uint32_t bytes,uint32_t alignment)
{
 uint32_t address=(uint32_t)(uintptr_t)pointer;
 if(!alignment||(alignment&(alignment-1u))||(address&(alignment-1u)))return 0;
 return user_write_range(pointer,bytes);
}

static int user_string(const char *text,uint32_t limit)
{uint32_t length;for(length=0;length<limit;++length){if(!user_range(text+length,1u))return 0;if(!text[length])return 1;}return 0;}

static int user_path(const char *source,char destination[64])
{uint8_t i=0,j=0;if(!user_range(source,1u))return 0;
 if(source[0]!='\\'&&source[0]!='/'){
     while(user_working_directory[i]&&i<63u){destination[i]=user_working_directory[i];++i;}
     if(i&&destination[i-1u]!='\\'){if(i>=63u)return 0;destination[i++]='\\';}
 }
 while(i<63u){if(!user_range(source+j,1u))return 0;destination[i]=source[j];if(!source[j])return i!=0;++i;++j;}
 destination[63]='\0';return 0;}

static int normalize_path(const char *source,char destination[64])
{
    uint8_t in=0,out=1,start,length,depth=0;
    destination[0]='\\';destination[1]='\0';
    while(source[in]){
        while(source[in]=='\\'||source[in]=='/')++in;
        if(!source[in])break;
        start=in;while(source[in]&&source[in]!='\\'&&source[in]!='/')++in;
        length=(uint8_t)(in-start);
        if(length==1u&&source[start]=='.')continue;
        if(length==2u&&source[start]=='.'&&source[start+1u]=='.'){
            if(out>1u){--out;while(out>1u&&destination[out-1u]!='\\')--out;destination[out]='\0';if(depth)--depth;}
            continue;
        }
        if(!length||length>12u||depth>=8u)return 0;
        if(out>1u){if(out>=63u)return 0;destination[out++]='\\';}
        if((uint16_t)out+length>=64u)return 0;
        while(length--)destination[out++]=source[start++];
        destination[out]='\0';++depth;
    }
    return 1;
}

static int user_dir_callback(const char *name,uint32_t size,uint32_t attributes,
                             void *opaque)
{
    user_dir_context_t *context=(user_dir_context_t *)opaque;uint8_t i;
    if(context->current++!=context->wanted)return 1;
    for(i=0;i<12u&&name[i];++i)context->entry->name[i]=name[i];
    context->entry->name[i]='\0';
    context->entry->attributes=(uint8_t)attributes;
    context->entry->padding[0]=context->entry->padding[1]=0;
    context->entry->size=size;context->found=1;return 0;
}

/* A page of directory entries is collected in one FAT traversal. */
typedef struct {uint32_t skip,seen,count,capacity;user_dir_entry_t *entries;} user_dir_page_t;
static int user_dir_page_callback(const char *name,uint32_t size,uint32_t attr,void *opaque)
{
    user_dir_page_t *page=opaque;user_dir_entry_t *e;unsigned i;
    if(page->seen++<page->skip)return 1;
    e=&page->entries[page->count++];
    for(i=0;i<12u&&name[i];i++)e->name[i]=name[i];
    e->name[i]=0;e->attributes=(uint8_t)attr;e->padding[0]=e->padding[1]=0;e->size=size;
    return page->count<page->capacity;
}

int32_t kernel_user_syscall_result(void){return user_syscall_result;}

int kernel_user_validate_entry(uint32_t usp,uint32_t pc,uint32_t sr)
{
    if(!user_active)return 0;
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return 1;
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return 1;
    }
    if((sr&0x2000u)||(sr&0xffff0000u)||
       !user_pc_valid_image(user_image,user_image_size,pc)){
        user_syscall_result=-0x3b2;kernel_user_end();return 1;
    }
    return 0;
}

int32_t kernel_user_capture_context(uint32_t usp,uint32_t pc,uint32_t sr)
{
    if(!user_active||(usp&1u)||!user_pc_valid_image(user_image,user_image_size,pc)||
       (sr&0x2000u)||(sr&0xffff0000u)||!user_stack_valid(current_process,usp))return -0x350;
    current_process->saved_usp=usp;current_process->saved_pc=pc;
    current_process->saved_sr=(uint16_t)sr;return 0;
}

int32_t kernel_user_capture_full_context(const uint32_t *registers,uint32_t usp,
                                         uint32_t pc,uint32_t sr)
{
    uint8_t i;int32_t result=kernel_user_capture_context(usp,pc,sr);
    if(result<0||!registers)return result<0?result:-0x352;
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->registers_valid=1u;return 0;
}

uint32_t kernel_user_restore_full_context(uint32_t *registers)
{
    uint8_t i;
    if(!user_active||!registers||!current_process->registers_valid)return 0;
    if(!user_stack_valid(current_process,current_process->saved_usp)){
        user_stack_fail_current();return 0;
    }
    for(i=0;i<15u;++i)registers[i]=current_process->saved_registers[i];
    return current_process->saved_usp;
}

int kernel_user_syscall(uint32_t call,uint32_t argument0,uint32_t argument1,
                        void *pointer0,void *pointer1)
{
    char path[64];uint32_t size=0,got=0;uint8_t attributes=0,i;
    if(!user_active){user_syscall_result=-0x200;return 1;}
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return 1;
    }
    user_syscall_result=0;
    if(call==0u){user_syscall_result=(int32_t)argument0;kernel_user_end();return 1;}
    if(call==1u){
        if(!services||!services->console_puts||!user_string((const char *)pointer0,4096u)){user_syscall_result=-0x201;return 0;}
        user_console_begin_output();kconsole_puts_to(current_process->terminal,(const char *)pointer0);return 0;}
    if(call==2u){if(!user_path((const char *)pointer0,path)){user_syscall_result=-0x210;return 0;}
        if(kfat_stat_path(path,&size,&attributes)!=FAT_FILE_FOUND||(attributes&0x10u)){user_syscall_result=-0x211;return 0;}
        for(i=0;i<USER_FILE_HANDLES;++i)if(!user_files[i].open){uint8_t j;for(j=0;j<64u;++j)user_files[i].path[j]=path[j];user_files[i].size=size;user_files[i].offset=0;user_files[i].open=1;user_syscall_result=i;return 0;}
        user_syscall_result=-0x212;return 0;}
    if(call==3u){if(argument0>=USER_FILE_HANDLES||!user_files[argument0].open){user_syscall_result=-0x220;return 0;}
        if(!user_write_range(pointer0,argument1)){user_syscall_result=-0x221;return 0;}
        if(kfat_read_path_at(user_files[argument0].path,user_files[argument0].offset,pointer0,argument1,&got)!=FAT_FILE_FOUND){user_syscall_result=-0x222;return 0;}
        user_files[argument0].offset+=got;user_syscall_result=(int32_t)got;return 0;}
    if(call==4u){if(argument0>=USER_FILE_HANDLES||!user_files[argument0].open){user_syscall_result=-0x230;return 0;}user_files[argument0].open=0;return 0;}
    if(call==5u){uint32_t *result=(uint32_t *)pointer1;if(!user_path((const char *)pointer0,path)||!user_write_aligned(result,8u,2u)){user_syscall_result=-0x240;return 0;}
        if(kfat_stat_path(path,&size,&attributes)!=FAT_FILE_FOUND){user_syscall_result=-0x241;return 0;}result[0]=size;result[1]=attributes;return 0;}
    if(call==6u){if(argument0>=USER_FILE_HANDLES||!user_files[argument0].open){user_syscall_result=-0x250;return 0;}
        if(argument1>user_files[argument0].size){user_syscall_result=-0x251;return 0;}
        user_files[argument0].offset=argument1;user_syscall_result=(int32_t)argument1;return 0;}
    if(call==7u){char *destination=(char *)pointer0;uint32_t length=0;
        while(user_arguments[length])++length;
        if(!argument0||argument0<=length||!user_write_range(destination,argument0)){user_syscall_result=-0x260;return 0;}
        for(i=0;i<=length;++i)destination[i]=user_arguments[i];
        user_syscall_result=(int32_t)length;return 0;}
    if(call==8u){char character[2];uint8_t value=(uint8_t)argument0;
        if(!services||!services->console_puts||(!value)||(value<32u&&value!='\n'&&value!='\t'&&value!='\b')){user_syscall_result=-0x270;return 0;}
        character[0]=(char)value;character[1]='\0';user_console_begin_output();
        kconsole_puts_to(current_process->terminal,character);user_syscall_result=1;return 0;}
    if(call==9u){user_syscall_result=kernel_user_session_getchar();return 0;}
    if(call==10u){user_syscall_result=kernel_user_yield();return 0;}
    if(call==11u){user_syscall_result=kernel_user_sleep(argument0);return 0;}
    if(call==56u){
        if(argument0!=sizeof(system_report_t)||!user_write_aligned(pointer0,sizeof(system_report_t),2u)){user_syscall_result=-0x280;return 0;}
        user_syscall_result=kernel_system_report((system_report_t *)pointer0);return 0;}
    if(call==57u){uint32_t start=0,limit=0,j;
        for(j=0;j<kpart_count();j++)if(kpart_is_boot((uint8_t)j)){start=kpart_start((uint8_t)j);limit=kpart_size((uint8_t)j);break;}
        if(!argument1||argument1>8||argument0>=limit||argument1>limit-argument0||argument0>0xffffffffUL-start||
           !user_write_range(pointer0,argument1*512u)){user_syscall_result=-0x281;return 0;}
        user_syscall_result=kblock_read_uncached(start+argument0,(uint16_t)argument1,pointer0)?(int32_t)(argument1*512u):-0x282;return 0;}
    if(call==58u){user_syscall_result=(int32_t)kernel_wall_ticks;return 0;}
    if(call==12u){if(!user_write_aligned(pointer0,24u,2u)){user_syscall_result=-0x280;return 0;}
        user_syscall_result=kernel_user_system_info((uint32_t *)pointer0);return 0;}
    if(call>=49u&&call<=51u){char target[64];
        if(!user_path((const char *)pointer0,path)){user_syscall_result=-FAT_WRITE_INVALID;return 0;}
        if(call==49u){
            if(!user_path((const char *)pointer1,target)){user_syscall_result=-FAT_WRITE_INVALID;return 0;}
            user_syscall_result=kfat_copy_path(path,target);
        }else if(call==50u)user_syscall_result=kfat_mkdir_path(path);
        else user_syscall_result=kfat_delete_path(path);
        return 0;
    }
    if(call>=53u&&call<=55u){char target[64];
        if(!user_path((const char *)pointer0,path)){user_syscall_result=-FAT_WRITE_INVALID;return 0;}
        if(call==54u){if(!user_path((const char *)pointer1,target)){user_syscall_result=-FAT_WRITE_INVALID;return 0;}
            user_syscall_result=kfat_move_path(path,target);}
        else if(call==53u)user_syscall_result=kfat_trash_path(path);
        else user_syscall_result=kfat_purge_path(path);
        return 0;
    }
    if(call==52u){user_dir_page_t page;enum fat_file_state state;
        if(argument0>65535u||!argument1||argument1>32u||
           !user_path((const char *)pointer0,path)||
           !user_write_aligned(pointer1,argument1*sizeof(user_dir_entry_t),2u)){
            user_syscall_result=-0x290;return 0;}
        page.skip=argument0;page.seen=page.count=0;page.capacity=argument1;page.entries=pointer1;
        state=kfat_list_path(path,user_dir_page_callback,&page);
        user_syscall_result=state==FAT_FILE_FOUND?(int32_t)page.count:-0x291;return 0;
    }
    if(call==13u){user_dir_entry_t *entry=(user_dir_entry_t *)pointer1;user_dir_context_t context;
        enum fat_file_state state;if(argument0>65535u||!user_path((const char *)pointer0,path)||!user_write_aligned(entry,sizeof(*entry),2u)){user_syscall_result=-0x290;return 0;}
        context.wanted=argument0;context.current=0;context.entry=entry;context.found=0;
        state=kfat_list_path(path,user_dir_callback,&context);
        if(state!=FAT_FILE_FOUND){user_syscall_result=-0x291;return 0;}
        user_syscall_result=context.found?0:1;return 0;}
    if(call==14u){char *destination=(char *)pointer0;uint32_t length=0;uint8_t drive;
        while(user_working_directory[length])++length;
        if(argument0<4u||argument0<=length+2u||!user_write_range(destination,argument0)){user_syscall_result=-0x2a0;return 0;}
        drive=(uint8_t)kernel_user_drive_letter();destination[0]=(char)drive;destination[1]=':';
        for(i=0;i<=length;++i)destination[i+2u]=user_working_directory[i];
        user_syscall_result=(int32_t)(length+2u);return 0;}
    if(call==15u){char combined[64],normalized[64];uint32_t path_size;uint8_t path_attributes,j;
        if(!user_path((const char *)pointer0,combined)||!normalize_path(combined,normalized)){user_syscall_result=-0x2b0;return 0;}
        if(kfat_stat_path(normalized,&path_size,&path_attributes)!=FAT_FILE_FOUND||!(path_attributes&0x10u)){user_syscall_result=-0x2b1;return 0;}
        for(j=0;j<64u;++j){user_working_directory[j]=normalized[j];if(!normalized[j])break;}
        user_syscall_result=0;return 0;}
    if(call==16u){void *allocation;uint32_t private_bytes=0;
        if(!argument0||argument0>USER_MEMORY_QUOTA||argument1>1u){user_syscall_result=-0x2c0;return 0;}
        for(i=0;i<USER_MEMORY_BLOCKS;++i)private_bytes+=user_memory[i].size;
        if(private_bytes>USER_MEMORY_QUOTA||argument0>USER_MEMORY_QUOTA-private_bytes){user_syscall_result=-0x2c3;return 0;}
        for(i=0;i<USER_MEMORY_BLOCKS&&user_memory[i].pointer;++i){}
        if(i==USER_MEMORY_BLOCKS){user_syscall_result=-0x2c1;return 0;}
        allocation=kmem_alloc_user(argument0+2u*USER_HEAP_GUARD_BYTES,16u,user_memory_owner,KMEM_READ|KMEM_WRITE,argument1!=0u);
        if(!allocation){user_syscall_result=-0x2c2;return 0;}
        user_memory[i].allocation=allocation;
        user_memory[i].pointer=(uint8_t *)allocation+USER_HEAP_GUARD_BYTES;
        user_memory[i].size=argument0;user_heap_init(&user_memory[i]);
        user_syscall_result=(int32_t)(uintptr_t)user_memory[i].pointer;return 0;}
    if(call==17u){for(i=0;i<USER_MEMORY_BLOCKS;++i)if(user_memory[i].pointer==pointer0){
            int valid=user_heap_valid(&user_memory[i]);
            if(!user_heap_release(&user_memory[i],user_memory_owner)){user_syscall_result=-0x2d1;return 0;}
            user_syscall_result=valid?0:USER_HEAP_FAULT;return 0;}
        user_syscall_result=-0x2d0;return 0;}
    if(call==18u){user_syscall_result=(int32_t)current_process->pid;return 0;}
    if(call==19u){uint32_t *info=(uint32_t *)pointer0,open_count=0,block_count=0,bytes=0,length=0;
        if(!user_write_aligned(info,24u,2u)){user_syscall_result=-0x2e0;return 0;}
        for(i=0;i<USER_FILE_HANDLES;++i)if(user_files[i].open)++open_count;
        for(i=0;i<USER_MEMORY_BLOCKS;++i)if(user_memory[i].pointer){++block_count;bytes+=user_memory[i].size;}
        while(user_working_directory[length])++length;
        info[0]=current_process->pid;info[1]=current_process->state;info[2]=open_count;
        info[3]=block_count;info[4]=bytes;info[5]=length;
        user_syscall_result=0;return 0;}
    if(call==20u){uint32_t *info=(uint32_t *)pointer0;const user_process_t *process;
        if(argument0>=USER_PROCESS_SLOTS||!user_write_aligned(info,16u,2u)){user_syscall_result=-0x2f0;return 0;}
        process=&process_table[argument0];info[0]=process->pid;info[1]=process->state;
        info[2]=(uint32_t)process->syscall_result;info[3]=(process==current_process)?1u:0u;
        user_syscall_result=0;return 0;}
    if(call==21u){uint32_t *result=(uint32_t *)pointer1,child_pid=0;int32_t child_result;
        if(!user_path((const char *)pointer0,path)||!user_write_aligned(result,8u,2u)){user_syscall_result=-0x300;return 0;}
        child_result=kernel_spawn_user(path,"",user_working_directory,&child_pid);
        if(child_pid==0u){user_syscall_result=child_result;return 0;}
        result[0]=child_pid;result[1]=(uint32_t)child_result;user_syscall_result=0;return 0;}
    if(call==22u){user_spawn_request_t *request=(user_spawn_request_t *)pointer0;char arguments[64];uint32_t child_pid=0;int32_t child_result;uint8_t j;
        if(!user_range(request,sizeof(*request))||!user_write_aligned(request,sizeof(*request),2u)||!user_path(request->path,path)||!user_string(request->arguments,64u)){user_syscall_result=-0x310;return 0;}
        for(j=0;j<63u&&request->arguments[j];++j){arguments[j]=request->arguments[j];}
        arguments[j]='\0';
        child_result=kernel_spawn_user(path,arguments,user_working_directory,&child_pid);
        if(!child_pid){user_syscall_result=child_result;return 0;}
        request->pid=child_pid;request->status=child_result;user_syscall_result=0;return 0;}
    if(call==23u){user_spawn_request_t *request=(user_spawn_request_t *)pointer0;char arguments[64];uint32_t job=0;int32_t result;uint8_t j;
        extern int32_t kernel_queue_user(const char *,const char *,const char *,uint32_t *);
        if(!user_range(request,sizeof(*request))||!user_write_aligned(request,sizeof(*request),2u)||!user_path(request->path,path)||!user_string(request->arguments,64u)){user_syscall_result=-0x320;return 0;}
        for(j=0;j<63u&&request->arguments[j];++j)arguments[j]=request->arguments[j];
        arguments[j]='\0';
        result=kernel_queue_user(path,arguments,user_working_directory,&job);
        if(result<0){user_syscall_result=result;return 0;}
        request->pid=job;request->status=0;user_syscall_result=0;return 0;}
    if(call==24u){uint32_t *result=(uint32_t *)pointer0;uint32_t state=0;int32_t status=0,query;
        extern int32_t kernel_query_user(uint32_t,int,uint32_t *,int32_t *);
        if(!user_write_aligned(result,8u,2u)||argument1>1u){user_syscall_result=-0x330;return 0;}
        query=kernel_query_user(argument0,(int)argument1,&state,&status);
        if(query<0){user_syscall_result=query;return 0;}
        result[0]=state;result[1]=(uint32_t)status;user_syscall_result=0;return 0;}
    if(call==25u){int32_t *status=(int32_t *)pointer0,wait_status=0,wait_result;
        extern int32_t kernel_wait_user(uint32_t,int32_t *);
        if(!user_write_aligned(status,4u,2u)){user_syscall_result=-0x340;return 0;}
        wait_result=kernel_wait_user(argument0,&wait_status);
        if(wait_result<0){user_syscall_result=wait_result;return 0;}
        *status=wait_status;user_syscall_result=0;return 0;}
    if(call==27u){uint32_t *context=(uint32_t *)pointer0;
        if(!user_write_aligned(context,12u,2u)||!current_process->saved_pc){user_syscall_result=-0x351;return 0;}
        context[0]=current_process->saved_usp;context[1]=current_process->saved_pc;
        context[2]=current_process->saved_sr;user_syscall_result=0;return 0;}
    if(call==31u){uint32_t *child_pid=(uint32_t *)pointer0;uint32_t pid=0;int32_t clone;
        if(!user_write_aligned(child_pid,4u,2u)){user_syscall_result=-0x360;return 0;}
        clone=kernel_user_clone(argument0,&pid);if(clone<0){user_syscall_result=clone;return 0;}
        *child_pid=pid;user_syscall_result=0;return 0;}
    if(call==32u){uint32_t *result=(uint32_t *)pointer0,state=0;int32_t status=0,query;
        if(!user_write_aligned(result,8u,2u)||argument1>1u){user_syscall_result=-0x370;return 0;}
        query=kernel_user_child_query(argument0,(int)argument1,&state,&status);
        if(query<0){user_syscall_result=query;return 0;}
        result[0]=state;result[1]=(uint32_t)status;user_syscall_result=0;return 0;}
    if(call==33u){int32_t *status=(int32_t *)pointer0;uint32_t state=0;int32_t child_status=0,query;
        if(!user_write_aligned(status,4u,2u)){user_syscall_result=-0x380;return 0;}
        query=kernel_user_child_query(argument0,0,&state,&child_status);
        if(query<0){user_syscall_result=query;return 0;}
        if(state!=USER_PROCESS_TERMINATED){user_syscall_result=-0x383;return 0;}
        query=kernel_user_child_query(argument0,1,&state,&child_status);
        if(query<0){user_syscall_result=query;return 0;}
        *status=child_status;user_syscall_result=0;return 0;}
    if(call==34u){user_spawn_request_t *request=(user_spawn_request_t *)pointer0;char arguments[64];uint32_t pid=0;int32_t spawn;uint8_t j;
        extern int32_t kernel_spawn_user_async(const char *,const char *,const char *,uint32_t *);
        if(!user_range(request,sizeof(*request))||!user_write_aligned(request,sizeof(*request),2u)||!user_path(request->path,path)||!user_string(request->arguments,64u)){user_syscall_result=-0x390;return 0;}
        for(j=0;j<63u&&request->arguments[j];++j)arguments[j]=request->arguments[j];
        arguments[j]='\0';
        spawn=kernel_spawn_user_async(path,arguments,user_working_directory,&pid);
        if(spawn<0){user_syscall_result=spawn;return 0;}
        request->pid=pid;request->status=0;user_syscall_result=0;return 0;}
    if(call==35u){char raw[64],normalized[64],result_path[64],segment[64];uint32_t path_size;uint8_t path_attributes,j=0,in=0,out=0;
        if(!user_string((const char *)pointer0,64u)){user_syscall_result=-0x3a0;return 0;}
        while(j<63u&&((const char *)pointer0)[j]){raw[j]=((const char *)pointer0)[j];++j;}raw[j]='\0';
        if(raw[0]=='+'){in=1;while(system_exec_path[out]){result_path[out]=system_exec_path[out];++out;}}
        while(raw[in]){uint8_t part=0,start;
            while(raw[in]&&raw[in]!=';'){if(part>=63u){user_syscall_result=-0x3a0;return 0;}segment[part++]=raw[in++];}
            segment[part]='\0';if(raw[in]==';')++in;if(!part){user_syscall_result=-0x3a1;return 0;}
            start=(part>=2u&&segment[1]==':')?2u:0u;
            if(!normalize_path(segment+start,normalized)||
               kfat_stat_path(normalized,&path_size,&path_attributes)!=FAT_FILE_FOUND||
               !(path_attributes&0x10u)){user_syscall_result=-0x3a1;return 0;}
            if(out){if(out>=63u){user_syscall_result=-0x3a0;return 0;}result_path[out++]=';';}
            for(j=0;normalized[j];++j){if(out>=63u){user_syscall_result=-0x3a0;return 0;}result_path[out++]=normalized[j];}
        }
        if(!out){user_syscall_result=-0x3a1;return 0;}result_path[out]='\0';
        for(j=0;j<64u;++j){system_exec_path[j]=result_path[j];if(!result_path[j])break;}
        user_syscall_result=0;return 0;}
    if(call==46u){char *destination=(char *)pointer0;uint8_t j;
        if(!user_write_range(destination,64u)){user_syscall_result=-0x3a2;return 0;}
        for(j=0;j<64u;++j){destination[j]=system_exec_path[j];if(!system_exec_path[j])break;}
        user_syscall_result=0;return 0;}
    if(call==36u){
        if(!user_range(pointer0,16u)){user_syscall_result=-0x3c0;return 0;}
        user_syscall_result=user_message_send(argument0,(const uint32_t *)pointer0);return 0;}
    if(call==37u||call==38u){
        if(!user_write_aligned(pointer0,sizeof(user_message_t),2u)){user_syscall_result=-0x3c0;return 0;}
        user_syscall_result=user_message_receive((user_message_t *)pointer0);return 0;}
    if(call==39u){user_syscall_result=user_service_register((const char *)pointer0);return 0;}
    if(call==40u){user_syscall_result=user_service_lookup((const char *)pointer0);return 0;}
    if(call==41u){user_syscall_result=user_service_unregister((const char *)pointer0);return 0;}
    if(call==42u){user_gfx_request_t *request=(user_gfx_request_t *)pointer0;
        int32_t x,y,width,height;
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GFX")){
            user_syscall_result=-0x3e1;return 0;}
        if(!user_range(request,sizeof(*request))){user_syscall_result=-0x3e0;return 0;}
        /* SYSTEM.GFX is the privileged broker, but argument0 preserves the
           kernel-authenticated PID of its original IPC client. Once a GUI
           foreground owner exists, background clients are read-only. */
        if(gui_keyboard_pid&&argument0!=gui_keyboard_pid&&request->opcode!=1u&&
           request->opcode!=9u&&request->opcode!=10u&&request->opcode!=11u){
            user_syscall_result=-0x3e5;return 0;}
        if(request->opcode==19u){const user_process_t *owner=0;
            const term_cell_t *cells=(const term_cell_t *)(uintptr_t)request->xy;
            for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            if(!owner||request->wh!=TERM_CELLS||request->color||
               (request->xy&1u)||!user_process_range(owner,cells,TERM_CELLS*sizeof(*cells))){
                user_syscall_result=-0x3e0;return 0;}
            if(kernel_user_pid_terminal(argument0)!=ksessions_current()){user_syscall_result=0;return 0;}
            /* Present into the application's own window surface, not the shared
               screen: the compositor then decides where and in which order it
               appears. */
            {uint8_t *surface=kernel_wm_app_surface(argument0);
             static uint32_t reported;
             if(surface){uint8_t *previous=kgfx_set_target(surface);
                 user_syscall_result=kgfx_terminal_present(cells);(void)kgfx_set_target(previous);
                 if(!reported){uint32_t k,nz=0;reported=1;
                     for(k=0;k<32000u;++k)if(surface[k])++nz;
                     kconsole_puts("APPSURF nonzero=");kconsole_put_u32(nz);
                     kconsole_puts(" row0nonzero=");
                     {uint32_t m=0;for(k=0;k<80u;++k)if(surface[k])++m;kconsole_put_u32(m);}
                     kconsole_puts("\n");}}
             else user_syscall_result=kgfx_terminal_present(cells);}
            return 0;
        }
        /* Only the active session may draw: a background app must not paint
           over the screen of the session the user is looking at. */
        if(kernel_user_pid_terminal(argument0)!=ksessions_current()){user_syscall_result=0;return 0;}
        /* A legacy graphics client may overwrite cells behind the renderer. */
        if(request->opcode!=1u&&request->opcode!=9u&&request->opcode!=10u&&request->opcode!=11u)
            kgfx_terminal_invalidate();
        x=(int16_t)(request->xy>>16);y=(int16_t)request->xy;
        width=(uint16_t)(request->wh>>16);height=(uint16_t)request->wh;
        if(request->opcode==2u)kgfx_clear((uint8_t)request->color);
        else if(request->opcode==3u)kgfx_fill_rect(x,y,width,height,(uint8_t)request->color);
        else if(request->opcode==4u)kgfx_frame_rect(x,y,width,height,(uint8_t)request->color);
        else if(request->opcode==5u)kgfx_pixel(x,y,(uint8_t)request->color);
        else if(request->opcode==7u)kgfx_line(x,y,(int16_t)(request->wh>>16),
                                              (int16_t)request->wh,(uint8_t)request->color);
        else if(request->opcode==8u){if(!kgfx_set_palette((uint8_t)x,(uint16_t)request->color)){
            user_syscall_result=-0x3e3;return 0;}}
        else if(request->opcode==9u){uint32_t handle=ksurface_create(argument0,
            (uint16_t)(request->xy>>16),(uint16_t)request->xy,(uint16_t)request->wh);
            user_syscall_result=handle?(int32_t)handle:-0x3f0;return 0;}
        else if(request->opcode==10u){user_syscall_result=
            ksurface_destroy(argument0,request->xy)?0:-0x3f1;return 0;}
        else if(request->opcode==11u){ksurface_info_t info;
            if(!ksurface_query(argument0,request->xy,&info))user_syscall_result=-0x3f1;
            else user_syscall_result=(int32_t)(((uint32_t)info.width<<16)|info.height);
            return 0;}
        else if(request->opcode==12u){
            kgfx_copy_rect(x,y,width,height,(int16_t)(request->color>>16),
                           (int16_t)request->color);}
        else if(request->opcode==13u){
            kgfx_session_view(kernel_user_pid_terminal(argument0)==ksessions_current());
            if(!gui_keyboard_persistent||argument0!=gui_keyboard_pid||
               (gui_desktop_pid&&gui_desktop_pid!=argument0)||!kgfx_enter_desktop()){
                user_syscall_result=-0x3e4;return 0;}
            gui_desktop_pid=argument0;}
        else if(request->opcode==18u){
            const user_process_t *owner=0;uint32_t n=request->wh;
            const gui_draw_t *list=(const gui_draw_t *)(uintptr_t)request->xy;
            for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            /* Any live GUI client may submit a draw list: with one owner only,
               a second application window could never be painted. Process and
               buffer validation below still guards the kernel. */
            if(!owner||!n||n>GUI_DRAW_MAX||
                (request->xy&1u)||!user_process_range(owner,list,n*sizeof(*list))){
                user_syscall_result=-0x3e0;return 0;}
            /* Caller remains blocked until the broker replies. Kernel code is
               not preempted by another user; no untrusted nested pointers. */
            {static uint32_t reported;uint8_t *surface=kernel_wm_app_surface(argument0);
             if(!reported){reported=1;kconsole_puts(surface?"APP WINDOW: surface ok\n":"APP WINDOW: no surface\n");}
             if(surface){uint8_t *previous=kgfx_set_target(surface);
                 kgfx_draw_batch(list,n);(void)kgfx_set_target(previous);}
             else kgfx_draw_batch(list,n);}
        }
        else if(request->opcode==15u)kgfx_emutos_background(x,y,width,height);
        else if(request->opcode==17u)kgfx_emutos_controls(x,y,width,height,request->color);
        else if(request->opcode==16u)kgfx_emutos_window(x,y,width,height);
        else if(request->opcode==14u){char glyph[2];
            glyph[0]=(char)request->wh;glyph[1]='\0';
            kgfx_text(x,y,glyph,(uint8_t)(request->color>>24),
                      (uint8_t)(request->color>>16),(int)(request->color&1u));}
        else{user_syscall_result=-0x3e2;return 0;}
        user_syscall_result=0;return 0;}
    if(call==43u){uint32_t *info=(uint32_t *)pointer0;const kgfx_info_t *gfx;
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GFX")){
            user_syscall_result=-0x3e1;return 0;}
        if(!user_write_aligned(info,24u,2u)){user_syscall_result=-0x3e0;return 0;}
        gfx=kgfx_get_info();info[0]=gfx->width;info[1]=gfx->height;
        info[2]=gfx->planes;info[3]=gfx->mode;info[4]=gfx->line_bytes;
        info[5]=gfx->capabilities;user_syscall_result=0;return 0;}
    if(call==44u){
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GFX")||
           !services||!services->console_clear){user_syscall_result=-0x3e1;return 0;}
        if(gui_keyboard_pid&&argument0!=gui_keyboard_pid){user_syscall_result=-0x3e5;return 0;}
        kgfx_restore_console();gui_desktop_pid=0;user_syscall_result=0;return 0;}
    if(call==45u){uint8_t found=0;
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")||
           !argument0||terminal_owner_pid||gui_keyboard_pid){user_syscall_result=-0x400;return 0;}
        for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
           process_table[i].state!=USER_PROCESS_FREE&&
           process_table[i].state!=USER_PROCESS_TERMINATED){found=1;break;}
        if(!found){user_syscall_result=-0x401;return 0;}
        {int32_t now=kernel_user_yield();
         if(now<0){user_syscall_result=-0x402;return 0;}
         gui_keyboard_pid=argument0;gui_keyboard_ready_tick=(uint32_t)now+5u;
         gui_keyboard_dropped=0;gui_keyboard_persistent=0;}
        user_syscall_result=0;return 0;}
    if(call==46u){
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")){
            user_syscall_result=-0x400;return 0;}
        if(gui_desktop_pid){kgfx_restore_console();gui_desktop_pid=0;}
        kgfx_cursor_hide();gui_keyboard_pid=0;gui_keyboard_ready_tick=0;gui_keyboard_dropped=0;
        gui_keyboard_persistent=0;
        user_syscall_result=0;return 0;}
    if(call==47u){uint8_t found=0;
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")||
           !argument0||terminal_owner_pid||gui_keyboard_pid){user_syscall_result=-0x400;return 0;}
        for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
           process_table[i].state!=USER_PROCESS_FREE&&
           process_table[i].state!=USER_PROCESS_TERMINATED){found=1;break;}
        if(!found){user_syscall_result=-0x401;return 0;}
        {int32_t now=kernel_user_yield();
         if(now<0){user_syscall_result=-0x402;return 0;}
         gui_keyboard_pid=argument0;gui_keyboard_ready_tick=(uint32_t)now+5u;
         gui_keyboard_dropped=0;gui_keyboard_persistent=1;
         {const kgfx_info_t *gfx=kgfx_get_info();gui_mouse_x=(int32_t)(gfx->width/2u);
          gui_mouse_y=(int32_t)(gfx->height/2u);}
         kernel_user_mouse_flush();}
        user_syscall_result=0;return 0;}
    if(call==48u){
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")||
           (argument0&&gui_keyboard_pid&&argument0!=gui_keyboard_pid)){
            user_syscall_result=-0x400;return 0;}
        if(gui_desktop_pid&&(!argument0||argument0==gui_desktop_pid)){
            kgfx_restore_console();gui_desktop_pid=0;}
        kgfx_cursor_hide();gui_keyboard_pid=0;gui_keyboard_ready_tick=0;gui_keyboard_dropped=0;
        gui_keyboard_persistent=0;user_syscall_result=0;return 0;}
    user_syscall_result=-0x2ff;
    return 0;
}
void kernel_exception_dispatch(uint32_t vector)
{
    kconsole_emergency();
    if(services&&services->console_clear&&services->console_puts){services->console_clear();services->console_puts("ATARI SYSTEM KERNEL PANIC\n\nFATAL CPU EXCEPTION: ");
        if(vector==2u)services->console_puts("BUS ERROR");else if(vector==3u)services->console_puts("ADDRESS ERROR");else if(vector==4u)services->console_puts("ILLEGAL INSTRUCTION");else if(vector==5u)services->console_puts("DIVIDE BY ZERO");else services->console_puts("UNKNOWN");services->console_puts("\n\nSYSTEM HALTED SAFELY\nPRESS RESET");}
}
