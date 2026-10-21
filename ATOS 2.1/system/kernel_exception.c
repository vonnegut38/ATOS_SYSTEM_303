#include "atos_display.h"
#include "kernel_game.h"
#include "kernel_video.h"
#include "kernel_console.h"
#include "kernel_sessions.h"
#include "kernel_theme.h"
#include "kernel_wm.h"
#include "system_report.h"
#include "kernel_block.h"
#include "kernel_partition.h"
#include "boot_protocol.h"
#include "app_abi_version.h"
#include "kernel_fat.h"
#include "kernel_language.h"
#include "kernel_memory.h"
#include "tos_async.h"
#include "kernel_graphics.h"
#include "icons_blob.h"
#include "kernel_surface.h"
#include "kernel_module.h"
#include "kernel_shell.h"
#include "gui_draw.h"
#include "tos_prg.h"
#include "kernel_audio.h"
#include "kernel_sound_nf.h"
#include "kernel_pcm.h"

/* Task 2: safe fault containment diagnostics.
   The exception vectors capture the six-byte SR/PC prefix: they never print and
   never touch the compositor or the graphics layer.  kernel_fault_report()
   is available to diagnostic callers; the snapshot is not a full bus frame. */
volatile uint32_t kernel_fault_vector;
volatile uint32_t kernel_fault_pc;
volatile uint32_t kernel_fault_sr;
volatile uint32_t kernel_fault_frame[6];
volatile uint32_t kernel_fault_a0,kernel_fault_d0;
volatile uint32_t kernel_fault_usp;   /* user stack pointer at the fault (AP) */
volatile uint32_t kernel_fault_w4,kernel_fault_w6,kernel_fault_w8;
volatile uint32_t kernel_fault_w10,kernel_fault_w12;   /* raw group-1 words */
/* Host-readable record of the LAST fault that was charged to a user process.
   The desktop kept dying with -258/-259 (bus/address error) and there was no
   way to tell which instruction or process was responsible from the host. */
uint32_t kdbg_uf_count,kdbg_uf_pid,kdbg_uf_vector,kdbg_uf_pc,kdbg_uf_a0,kdbg_uf_d0;
uint32_t kdbg_uf_sr,kdbg_uf_usp,kdbg_uf_image,kdbg_uf_savedpc,kdbg_uf_state;
uint32_t kdbg_uf_w4,kdbg_uf_w6,kdbg_uf_w8,kdbg_uf_w10,kdbg_uf_w12;
uint32_t kdbg_uf_stack[32];
void kernel_fault_report(void (*puts)(const char *),void (*put_u32)(uint32_t))
{
    uint8_t i;
    if(!kernel_fault_vector)return;
    puts("FAULT vec=");put_u32(kernel_fault_vector);
    puts(" pc=");put_u32(kernel_fault_pc);
    puts(" sr=");put_u32(kernel_fault_sr&0xffffu);
    puts(" usp=");put_u32(kernel_fault_usp);
    puts(" a0=");put_u32(kernel_fault_a0);
    puts(" d0=");put_u32(kernel_fault_d0);
    puts(" frame:");
    for(i=0;i<6u;++i){puts(" ");put_u32(kernel_fault_frame[i]);}
    puts("\n");
    kernel_fault_vector=0u;   /* report once per captured fault */
}
#define USER_FILE_HANDLES 4u
#define USER_MEMORY_BLOCKS 8u
#define USER_MEMORY_QUOTA (1024u*1024u)
/* Fast (TT/ALT) RAM is a separate budget: a request that PREFERS it may grow to
   USER_MEMORY_QUOTA_TT per process, but only when the block really lands in
   fast RAM (address >= 0x01000000).  A fallback into ST-RAM is still held to
   the 1 MB ST quota, so a plain 4 MB ST cannot be drained by one game. */
#define USER_MEMORY_QUOTA_TT (16u*1024u*1024u)
#define USER_FAST_RAM_BASE 0x01000000u
/* 4 slots sa zajete od bootu (INIT, GFX, GUI + pulpit), a WM miesci do 6
   okien aplikacji: 4+6=10, wiec 12 daje jeszcze zapas na procesy potomne. */
#define USER_PROCESS_SLOTS 12u
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
/* The cursor remembers the cluster the last read stopped in: without it every
   chunk re-walked the FAT chain from the file's first cluster, which made a
   sequential read quadratic in the number of chunks. */
typedef struct {char path[64];uint32_t size,offset;fat_cursor_t cursor;uint8_t open;} user_file_t;
typedef struct {void *pointer,*allocation;uint32_t size;} user_memory_t;
typedef struct {char name[13];uint8_t attributes,padding[2];uint32_t size,datetime;} user_dir_entry_t;
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
/* Desktop registration outcome (opcode 13): which guard refused it. */
uint32_t kdbg_desk_rejp,kdbg_desk_rejk,kdbg_desk_rejd,kdbg_desk_rejok;
/* Single-instance decisions: instances found, and cross-desktop switches made. */
uint32_t kdbg_app_seen,kdbg_app_switch;
uint32_t kdbg_desk_rejok_s[SESSION_COUNT];
/* Draw batches submitted (ok) and refused (rej) per session. */
uint32_t kdbg_batch_ok[SESSION_COUNT],kdbg_batch_rej[SESSION_COUNT];
uint32_t kdbg_items_ok[SESSION_COUNT];
static uint32_t gui_keyboard_pids[SESSION_COUNT];
#define gui_keyboard_pid gui_keyboard_pids[ksessions_current()]
static uint32_t gui_ready_ticks[SESSION_COUNT],gui_dropped[SESSION_COUNT];
static uint8_t gui_persistent[SESSION_COUNT];
#define gui_keyboard_ready_tick gui_ready_ticks[ksessions_current()]
#define gui_keyboard_dropped gui_dropped[ksessions_current()]
#define gui_keyboard_persistent gui_persistent[ksessions_current()]
/* Keyboard focus inside the session: the client that owns the last application
   window the user pressed.  0 means "no application window focused", so input
   falls back to the session's keyboard owner (the desktop or the shell).  The
   compositor updates this on click-to-focus (kernel_wm.c). */
static uint32_t gui_focus_pids[SESSION_COUNT];
#define gui_focus_pid gui_focus_pids[ksessions_current()]
uint32_t kernel_gui_focus_pid(void){return gui_focus_pid;}
void kernel_gui_focus_set(uint32_t pid){gui_focus_pid=pid;}
static int32_t gui_mouse_x,gui_mouse_y;
static char system_exec_path[64]="\\SYSTEM;\\SYSTEM\\COMMAND;\\SYSTEM\\APP;\\SYSTEM\\TEST";

int kernel_exec_path_get(uint32_t index,char destination[64])
{uint8_t in=0,out=0;if(!destination)return 0;
 while(index){while(system_exec_path[in]&&system_exec_path[in]!=';')++in;if(!system_exec_path[in])return 0;++in;--index;}
 while(system_exec_path[in]&&system_exec_path[in]!=';'&&out<63u)destination[out++]=system_exec_path[in++];
 destination[out]='\0';return out!=0;}
static uint32_t gui_desktop_pids[SESSION_COUNT];
#define gui_desktop_pid gui_desktop_pids[ksessions_current()]
/* Release only the owner's session; background exit must not touch the front. */
/* A foreground job may capture GUI input itself or hand it to a child.
 * Unrelated jobs cannot take its terminal, even while its GUI is released. */
static int user_gui_terminal_allowed(unsigned slot,uint32_t pid)
{
 unsigned depth,i;uint32_t owner;
 if(slot>=SESSION_COUNT)return 0;
 owner=terminal_owners[slot];if(!owner)return 1;
 for(depth=0;depth<USER_PROCESS_SLOTS&&pid;depth++){
  for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==pid&&
      process_table[i].state!=USER_PROCESS_FREE&&process_table[i].terminal==slot)break;
  if(i==USER_PROCESS_SLOTS)return 0;
  if(pid==owner)return 1;
  pid=process_table[i].parent_pid;
 }
 return 0;
}

uint32_t kdbg_rel_calls,kdbg_rel_clear,kdbg_rel_pid,kdbg_rel_slot,kdbg_rel_kill,kdbg_rel_end,kdbg_rel_svc;
uint32_t kdbg_end_pid,kdbg_end_term,kdbg_end_name,kdbg_end_pc,kdbg_end_image;int32_t kdbg_end_status;
static void user_gui_release_session(unsigned slot,uint32_t pid)
{
 ++kdbg_rel_calls;kdbg_rel_pid=pid;kdbg_rel_slot=slot;
 if(slot>=SESSION_COUNT)return;
 if(pid&&gui_focus_pids[slot]==pid)gui_focus_pids[slot]=0;
 if(pid&&gui_keyboard_pids[slot]!=pid&&gui_desktop_pids[slot]!=pid)return;
 ++kdbg_rel_clear;
 if(slot==ksessions_current()){
  if(gui_desktop_pids[slot])kgfx_restore_console();
  kgfx_cursor_hide();
 }
 gui_desktop_pids[slot]=0;gui_keyboard_pids[slot]=0;gui_focus_pids[slot]=0;
 gui_ready_ticks[slot]=0;gui_dropped[slot]=0;gui_persistent[slot]=0;
}
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
/* The mouse path now PEEKS the ordered session queue, so it consumes an event
   only after the real recipient has been resolved and has mailbox room. */
extern void kernel_user_mouse_flush(void);
extern void kernel_user_terminal_reset_modifiers(void);
extern int32_t kernel_user_yield(void);
extern uint32_t kernel_user_clock(void);
extern uint32_t kernel_user_wall_clock(void);
extern uint32_t kernel_user_hires_clock(void);
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

/* First game frame/palette seen, so a garbled image can be traced to a specific
   field (pitch/scale/dims) or to the palette instead of guessed. */
uint32_t kdbg_frame_n,kdbg_frame_w,kdbg_frame_h,kdbg_frame_pitch,kdbg_frame_scale,
         kdbg_frame_count,kdbg_frame_dst0,kdbg_frame_rect0;
uint32_t kdbg_pal_n,kdbg_pal_first,kdbg_pal_count,kdbg_pal0,kdbg_pal1;
uint32_t kdbg_frame_pixels;

/* Forward declarations for the bridge helpers below (the definitions live
   further down this file). */
unsigned kernel_user_pid_terminal(uint32_t pid);
uint32_t kernel_user_service_pid(const char *name);
unsigned ksessions_current(void);

/* ------------------------------------------------------------------------
   GFX service over the MESSAGE protocol.

   SYSTEM.GFX lives inside the kernel, so a client that addresses it with
   atos_send() (the REminiscence/Flashback port does: {op,xy,0,sub}) used to get
   "no such process" and saw its video-mode request fail.  These two helpers
   route that message to exactly the same game-mode code the direct GFX request
   (opcode 42) uses, and post the reply back with the service as the sender, so
   the client's synchronous call returns the real result.
   ------------------------------------------------------------------------ */
static int32_t user_game_service(uint32_t pid,uint32_t xy,uint32_t sub)
{
    unsigned session=kernel_user_pid_terminal(pid);
    if(sub==ATOS_GAME_BEGIN){
        if(session!=ksessions_current())return -0x3e5;
        return kgame_begin(pid,session,(uint16_t)(xy>>16),(uint16_t)xy);
    }
    if(sub==ATOS_GAME_END)return kgame_end(pid);
    if(sub==ATOS_GAME_FRAME){
        static atos_game_frame_t frame;
        const uint8_t *src=(const uint8_t *)(uintptr_t)xy;
        const uint8_t *pixels;uint32_t k,bytes;
        if((xy&1u)||!user_process_range(current_process,src,ATOS_GAME_FRAME_HEADER))return -0x3e0;
        for(k=0;k<ATOS_GAME_FRAME_HEADER;++k)((uint8_t *)&frame)[k]=src[k];
        ++kdbg_frame_n;
        if(kdbg_frame_n==1u){
            kdbg_frame_w=frame.width;kdbg_frame_h=frame.height;
            kdbg_frame_pitch=frame.pitch;kdbg_frame_scale=frame.scale;
            kdbg_frame_count=frame.count;
            kdbg_frame_dst0=((uint32_t)(uint16_t)frame.dst_x<<16)|(uint16_t)frame.dst_y;
            kdbg_frame_pixels=frame.pixels;
        }
        if(frame.count>ATOS_GAME_RECTS||!frame.width||!frame.height||
           frame.pitch<frame.width||frame.scale<1u||frame.scale>2u)return -0x3e0;
        bytes=(uint32_t)frame.count*(uint32_t)sizeof(atos_game_rect_t);
        if(bytes){
            if(!user_process_range(current_process,src+ATOS_GAME_FRAME_HEADER,bytes))return -0x3e0;
            for(k=0;k<bytes;++k)((uint8_t *)frame.rect)[k]=src[ATOS_GAME_FRAME_HEADER+k];
            if(kdbg_frame_n==1u)kdbg_frame_rect0=((uint32_t)(uint16_t)frame.rect[0].x<<16)|
                                                  (uint32_t)(uint16_t)frame.rect[0].y;
        }
        pixels=(const uint8_t *)(uintptr_t)frame.pixels;
        if(!user_process_range(current_process,pixels,
           (uint32_t)frame.pitch*(uint32_t)(frame.height-1u)+frame.width))return -0x3e0;
        return kgame_frame(pid,&frame,pixels);
    }
    if(sub==ATOS_GAME_PALETTE){
        static atos_game_palette_t palette;
        const uint8_t *src=(const uint8_t *)(uintptr_t)xy;uint32_t k,bytes;
        if((xy&1u)||!user_process_range(current_process,src,ATOS_GAME_PALETTE_HEADER))return -0x3e0;
        for(k=0;k<ATOS_GAME_PALETTE_HEADER;++k)((uint8_t *)&palette)[k]=src[k];
        ++kdbg_pal_n;
        if(kdbg_pal_n==1u){
            kdbg_pal_first=palette.first;kdbg_pal_count=palette.count;
            kdbg_pal0=((uint32_t)palette.rgb[0]<<16)|((uint32_t)palette.rgb[1]<<8)|palette.rgb[2];
            kdbg_pal1=((uint32_t)palette.rgb[3]<<16)|((uint32_t)palette.rgb[4]<<8)|palette.rgb[5];
        }
        if(palette.first>255u||palette.count>256u-palette.first)return -0x3e0;
        bytes=(uint32_t)palette.count*3u;
        if(bytes){
            if(!user_process_range(current_process,src+ATOS_GAME_PALETTE_HEADER,bytes))return -0x3e0;
            for(k=0;k<bytes;++k)palette.rgb[k]=src[ATOS_GAME_PALETTE_HEADER+k];
        }
        return kgame_palette(pid,&palette);
    }
    return -0x3e2;
}

/* Deliver a message into pid's mailbox with an explicit sender (the kernel's
   own service), which is what a synchronous client call waits for. */
static int32_t user_message_post_from(uint32_t from,uint32_t pid,const uint32_t value[4])
{
    uint8_t i,j;user_process_t *target=0;user_message_t *message;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE&&
       process_table[i].state!=USER_PROCESS_TERMINATED){target=&process_table[i];break;}
    if(!target)return -0x3c1;
    if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
        message=target->wait_message_pointer;message->sender=from;
        for(j=0;j<4u;++j)message->value[j]=value[j];
        target->wait_message_pointer=0;target->saved_registers[0]=0;
        target->state=USER_PROCESS_READY;return 0;
    }
    if(target->mailbox_count>=USER_MAILBOX_DEPTH)return -0x3c2;
    message=&target->mailbox[target->mailbox_tail];message->sender=from;
    for(j=0;j<4u;++j)message->value[j]=value[j];
    target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
    ++target->mailbox_count;return 0;
}

static int32_t user_message_send(uint32_t pid,const uint32_t value[4])
{
    /* Bridge: a client addressing the kernel's GFX service with the message
       protocol ({op,xy,0,sub}) must reach the game-mode code. */
    {uint32_t gfx=kernel_user_service_pid("SYSTEM.GFX");
     if(gfx&&pid==gfx&&value[0]==ATOS_GAME_OPCODE){
         uint32_t reply[4];
         int32_t rc=user_game_service(current_process->pid,value[1],value[3]);
         reply[0]=0u;reply[1]=(uint32_t)rc;reply[2]=0u;reply[3]=0u;
         return user_message_post_from(gfx,current_process->pid,reply);}}
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
        /* Brak KMOU: pelna skrzynka moze byc zapchana WMOV/KKEY.  Odpowiedz
           uslugi (GFX/GUI) jest krytyczna - klient czeka na nia i nie odbierze
           niczego, dopoki jej nie dostanie.  Zastap NAJSTARSZY komunikat,
           ktory nie jest jadrowym KEXI (lifecycle).  Bez tego GFX.APP
           zapetlal sie na -0x3c2 i zamrazal rysowanie wszystkich okien. */
        index=target->mailbox_head;
        for(i=0;i<target->mailbox_count;++i){
            message=&target->mailbox[index];
            if(!(message->sender==0u&&message->value[0]==0x4b455849u)){
                message->sender=current_process->pid;
                for(j=0;j<4u;++j)message->value[j]=value[j];
                return 0;
            }
            index=(uint8_t)((index+1u)%USER_MAILBOX_DEPTH);
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

/* Tell an application that the window manager changed its window geometry
   (a committed move or resize).  This is the GEM/XaAES "WM_MOVED" pattern:
   the WM owns the frame, so the client has to learn the new coordinates before
   it can hit-test its own controls.  Sender stays 0 (kernel). */
static void user_notify(uint32_t pid,uint32_t kind,uint32_t a,uint32_t b,uint32_t c)
{
    uint8_t i;user_process_t *target=0;user_message_t *message;
    if(!pid)return;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
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
    message->sender=0;message->value[0]=kind;message->value[1]=a;message->value[2]=b;message->value[3]=c;
}
/* Kind 0x574d4f56 = "WMOV": value1 = (x<<16)|y, value2 = (w<<16)|h,
   value3 = the window HANDLE.  XaAES puts the handle in WM_SIZED/WM_MOVED for
   the same reason: without it a client that owns several windows has to guess
   which one moved, and after a resize the size no longer matches its cached
   value, so the message was dropped and the picture never grew. */
void kernel_user_notify_window(uint32_t pid,uint32_t xy,uint32_t wh,uint32_t handle)
{user_notify(pid,0x574d4f56u,xy,wh,handle);}
void kernel_user_notify_display(uint32_t pid)
{user_notify(pid,ATOS_DISPLAY_CHANGED,0u,0u,0u);}
/* The compositor dropped a window the client owns ("WCLS", value1 = handle):
   the desktop app stops treating its cached geometry as live and reopens the
   window on the next icon click. */
void kernel_user_notify_window_close(uint32_t pid,uint32_t handle)
{user_notify(pid,0x57434c53u,handle,0u,0u);}

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

/* Zombie slots (Claude, 16.10).  A finished process keeps its slot in the
   TERMINATED state until its parent collects the status (call 32 reap=1 /
   call 33).  The desktop launches applications with SYS_SPAWN_ASYNC and never
   collects them, so every QUIT/ESC left a zombie behind: with 6 slots (4
   services + 2 applications) the third launch failed - "PROCESS START FAILED",
   or on the desktop a silent fallback into the TOS personality ("TOS STARTED"
   and no window, "TOS RC -689").  Memory, windows and files are already
   released in kernel_user_end(); only the slot and its exit status remain.
   When no slot is free, give back the oldest zombie nobody is waiting for.  A
   later status query for it answers "no such child" (-0x371), which the
   desktop already treats as "not running". */
volatile uint32_t kdbg_zombie_reclaimed;
static void user_reclaim_zombie_if_full(void)
{
    uint8_t i,k;int victim=-1;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].state==USER_PROCESS_FREE)return;
    for(i=0;i<USER_PROCESS_SLOTS;++i){
        int waited=0;
        if(process_table[i].state!=USER_PROCESS_TERMINATED)continue;
        for(k=0;k<USER_PROCESS_SLOTS;++k)
            if(process_table[k].state==USER_PROCESS_WAITING&&
               process_table[k].wait_pid==process_table[i].pid)waited=1;
        if(waited)continue;
        if(victim<0||process_table[i].pid<process_table[(uint8_t)victim].pid)victim=(int)i;
    }
    if(victim<0)return;
    process_table[(uint8_t)victim].state=USER_PROCESS_FREE;
    process_table[(uint8_t)victim].pid=0;
    process_table[(uint8_t)victim].parent_pid=0;
    ++kdbg_zombie_reclaimed;
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
{unsigned i;
#if CONFIG_TOS
 if(kernel_tos_async_owns(pid))return kernel_tos_async_session();
#endif
 for(i=0;i<USER_PROCESS_SLOTS;i++) {
  if(process_table[i].pid==pid&&process_table[i].state!=USER_PROCESS_FREE)
   return process_table[i].terminal;
 }
 return 0;
}
unsigned kernel_user_last_terminal(void){return current_process->terminal;}
unsigned kernel_user_birth_terminal(void)
{return user_active?current_process->terminal:ksessions_current();}
void kernel_user_assign_terminal(uint32_t pid,unsigned terminal)
{unsigned i;if(terminal>=SESSION_COUNT)return;for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==pid)process_table[i].terminal=(uint8_t)terminal;}
int kernel_user_session_busy(unsigned terminal)
{unsigned i;for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].terminal==terminal&&process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED)return 1;return 0;}
/* Task 4: the TOS persona can own the desktop too (kernel_tos_use_surface()).
   Without this, kernel_user_desktop_visible() stayed false for a TOS program
   and ksessions_pump hid the desktop (desktop_hidden=1). */
volatile uint8_t kernel_tos_desktop_visible;
int kernel_user_desktop_visible(void)
{return kernel_tos_desktop_visible||(gui_desktop_pid&&kernel_user_pid_terminal(gui_desktop_pid)==ksessions_current());}
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

/* 8.3 program name of a running pid (dla paska: ikona/etykieta aplikacji). */
int kernel_user_pid_name(uint32_t pid,char name[13])
{
    uint8_t i;
    if(!pid||!name)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i){
        uint32_t info[4];
        if(!kernel_user_process_info(i,info)||info[0]!=pid)continue;
        return kernel_user_process_name(i,name);
    }
    return 0;
}

int kernel_user_kill(uint32_t pid)
{
    uint8_t i,j;user_process_t *process=0;
    if(!pid)return 0;
    for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==pid&&
       process_table[i].state!=USER_PROCESS_FREE){process=&process_table[i];break;}
    if(!process)return 0;
    if(process->state==USER_PROCESS_RUNNING||process->state==USER_PROCESS_WAITING)return -1;
    for(i=0;i<USER_MEMORY_BLOCKS;++i)if(process->memory[i].pointer)
        (void)user_heap_release(&process->memory[i],process->memory_owner);
    if(process->runtime_stack){(void)kmem_release(process->runtime_stack,process->runtime_stack_owner);process->runtime_stack=0;}
    if(process->runtime_image){(void)kmem_release(process->runtime_image,process->runtime_image_owner);process->runtime_image=0;}
    for(i=0;i<USER_FILE_HANDLES;++i)process->files[i].open=0;
    for(j=0;j<USER_PROCESS_SLOTS;++j)if(process_table[j].state!=USER_PROCESS_FREE&&
        process_table[j].parent_pid==pid)process_table[j].parent_pid=0;
    user_terminal_release(pid);
    if(kernel_user_service_pid("SYSTEM.GUI")==pid){
        for(i=0;i<SESSION_COUNT;i++)user_gui_release_session(i,0);
    }else{extern uint32_t kdbg_rel_kill; ++kdbg_rel_kill; user_gui_release_session(process->terminal,pid);}
    user_gui_notify_exit(pid,-3);user_service_remove_pid(pid);ksurface_release_owner(pid);
    /* A dying process must not leave a PSG channel sounding (normal exit, error
       or kill all arrive here).  Harmless when the pid was not the sound owner. */
    kaudio_release(pid);
    ksnd_release(pid);
    ksnd_owner_exit(pid);
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

uint32_t kdbg_kbpid,kdbg_term,kdbg_cur;
int kernel_user_gui_keyboard_active(void)
{
    extern unsigned kernel_user_pid_terminal(uint32_t);
    kdbg_kbpid=gui_keyboard_pid;kdbg_cur=ksessions_current();
    kdbg_term=gui_keyboard_pid?(uint32_t)kernel_user_pid_terminal(gui_keyboard_pid):0xFFFFFFFFu;
    return gui_keyboard_pid&&(unsigned)kdbg_term==ksessions_current();
}

uint32_t kdbg_wake_calls,kdbg_wake_keys,kdbg_route_gui,kdbg_route_shell;
/* Mouse events delivered per receiving session. */
uint32_t kdbg_mouse_routed[SESSION_COUNT];
int kernel_user_gui_keyboard_wake(void)
{
    uint8_t i;int32_t now;uint32_t type,scan,modifiers,character;
    /* Click capture: the window that received the press owns the whole click
       (motion and release), so a drag that ends over another window cannot
       strand the previous recipient's button state.  Function-local so the host
       tests that compile this slice stay self-contained. */
    static uint32_t mouse_capture_pid;
    static uint8_t mouse_wm_capture;
    static unsigned mouse_wm_session;
    /* Pasek dolny (WM) i jego menu kontekstowe: gest myszy w tym obszarze
       nalezy do WM, nie do aplikacji pod spodem. */
    static uint8_t dock_wm_capture;
    static unsigned dock_wm_session;
    ++kdbg_wake_calls;
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
    if(gui_keyboard_persistent){
        uint32_t ev_buttons,ev_tick;int32_t ev_x,ev_y;
        /* PEEK the oldest ordered interaction event: the real recipient and its
           mailbox room are resolved BEFORE it leaves the queue, so a full client
           mailbox keeps the DOWN/UP pair queued instead of dropping (or
           collapsing) the release. */
        if(ksessions_mouse_peek(&ev_buttons,&ev_x,&ev_y,&ev_tick)){
            /* Presentation follows the LIVE pointer; the interaction event keeps
               the x/y it was recorded with, so a hit test is never re-evaluated
               at a cursor position the pointer has already left. */
            gui_mouse_x=ksessions_mouse_x();gui_mouse_y=ksessions_mouse_y();
            {const kgfx_info_t *gfx=kgfx_get_info();
             if(gui_mouse_x<0)gui_mouse_x=0;
             else if((uint32_t)gui_mouse_x>=gfx->width)gui_mouse_x=(int32_t)(gfx->width-1u);
             if(gui_mouse_y<0)gui_mouse_y=0;
             else if((uint32_t)gui_mouse_y>=gfx->height)gui_mouse_y=(int32_t)(gfx->height-1u);}
            kgfx_cursor_move(gui_mouse_x,gui_mouse_y);
            /* Per-event routing with click capture.  With no button held the
               topmost window under the EVENT position owns it; a press latches
               its target until the release, so press and release always reach
               the same client. */
            /* A capture latched while ANOTHER desktop was in front belongs to a
               process of that session.  Keeping it would send this desktop's
               clicks to the previous session's owner, which is exactly why the
               second desktop looked dead to the mouse. */
            if(mouse_capture_pid&&
               kernel_user_pid_terminal(mouse_capture_pid)!=ksessions_current())
                mouse_capture_pid=0;
            /* A frame/scrollbar press must not also activate the app beneath
               it (SysInfo's QUIT used to sit under the resize gadget). Keep
               the entire gesture, including release outside the frame, with
               the WM. Its independent recorded press remains untouched. */
            if(mouse_wm_capture&&mouse_wm_session!=ksessions_current())
                mouse_wm_capture=0;
            if(kgame_owns_screen()){mouse_wm_capture=0;dock_wm_capture=0;}
            else {
                if(dock_wm_capture&&dock_wm_session!=ksessions_current())
                    dock_wm_capture=0;
                if(!mouse_capture_pid&&!dock_wm_capture&&kernel_wm_dock_at(ev_x,ev_y)){
                    dock_wm_capture=1;
                    dock_wm_session=ksessions_current();
                }
                if(dock_wm_capture){
                    if(!(ev_buttons&3u))dock_wm_capture=0;
                    ksessions_mouse_consume();
                    return 1;
                }
                if(!mouse_capture_pid&&!mouse_wm_capture&&(ev_buttons&1u)&&
                   kernel_wm_chrome_at(ev_x,ev_y)){
                    mouse_wm_capture=1;
                    mouse_wm_session=ksessions_current();
                }
                if(mouse_wm_capture){
                    if(!(ev_buttons&1u))mouse_wm_capture=0;
                    ksessions_mouse_consume();
                    return 1;
                }
            }
            {uint32_t want;
             /* Exclusive game mode: every pointer event belongs to the game,
                nothing underneath may be clicked blind. */
             if(kgame_owns_screen())want=kgame_owner();
             else if(ev_buttons&1u){
                 if(!mouse_capture_pid)
                     mouse_capture_pid=kernel_wm_pid_at(ev_x,ev_y);
                 if(!mouse_capture_pid)mouse_capture_pid=(uint32_t)pid;
                 want=mouse_capture_pid;
             }else want=mouse_capture_pid?mouse_capture_pid:kernel_wm_pid_at(ev_x,ev_y);
             if(!want)want=(uint32_t)pid;
#if CONFIG_TOS
             if(kernel_tos_async_owns(want)){
                 if(!kernel_tos_async_mouse(ev_buttons,ev_x,ev_y))return 0;
                 ksessions_mouse_consume();
                 if(!(ev_buttons&1u))mouse_capture_pid=0;
                 return 1;
             }
#endif
            {unsigned d=kernel_user_pid_terminal(want);
             if(d<SESSION_COUNT)++kdbg_mouse_routed[d];}
             if(want!=(uint32_t)pid){uint8_t k;int found=0;
                 for(k=0;k<USER_PROCESS_SLOTS;++k)if(process_table[k].pid==want&&
                     process_table[k].state!=USER_PROCESS_FREE&&
                     process_table[k].state!=USER_PROCESS_TERMINATED){target=&process_table[k];found=1;break;}
                 if(!found)mouse_capture_pid=0;}}
            /* Backpressure against the REAL recipient: when its mailbox is full
               the event stays queued; the release is never lost silently. */
            if(!(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer)&&
               target->mailbox_count>=USER_MAILBOX_INPUT_DEPTH)return 0;
            /* During a WM drag there is nothing for the client to do with
               pointer motion, so the KMOU is skipped (the release still commits
               the move).  Only while the button is actually held: a stale drag
               flag must not swallow every later click. */
            if(kernel_wm_dragging()&&(ev_buttons&1u)){ksessions_mouse_consume();return 1;}
            if(target->state==USER_PROCESS_MESSAGE&&target->wait_message_pointer){
                message=target->wait_message_pointer;target->wait_message_pointer=0;
                target->saved_registers[0]=0;target->state=USER_PROCESS_READY;
            }else{
                /* Coalesce a KMOU already at the tail: only the latest cursor
                   position matters.  Appending one entry per host mouse packet
                   let a moving pointer fill the client half of the mailbox,
                   after which keyboard events were dropped as backpressure (the
                   desktop then ignored F-keys). */
                uint8_t last=(uint8_t)((target->mailbox_tail+USER_MAILBOX_DEPTH-1u)%USER_MAILBOX_DEPTH);
                user_message_t *prev=target->mailbox_count?&target->mailbox[last]:0;
                /* A matching button mask alone is NOT proof of motion: the
                   tail may be the unconsumed DOWN. Replacing its coordinates
                   turns a press on one control into a click on another. Only
                   merge when the preceding queued mouse event proves that the
                   tail is motion, preserving both edges and their timestamps. */
                user_message_t *before=target->mailbox_count>=2u?
                    &target->mailbox[(last+USER_MAILBOX_DEPTH-1u)%USER_MAILBOX_DEPTH]:0;
                if(prev&&before&&prev->sender==0u&&before->sender==0u&&
                   prev->value[0]==0x4b4d4f55u&&before->value[0]==0x4b4d4f55u&&
                   ((before->value[2]>>24)&7u)==((prev->value[2]>>24)&7u)&&
                   ((prev->value[2]>>24)&7u)==((ev_buttons&31u)&7u))message=prev;
                else{
                    message=&target->mailbox[target->mailbox_tail];
                    target->mailbox_tail=(uint8_t)((target->mailbox_tail+1u)%USER_MAILBOX_DEPTH);
                    ++target->mailbox_count;
                }
            }
            message->sender=0;message->value[0]=0x4b4d4f55u; /* KMOU */
            /* value[1] = the EVENT position, value[3] = the EVENT timestamp so
               the client times a double click from when it happened, not from
               when it happened to be drained.  No receiver read value[3]. */
            message->value[1]=((uint32_t)(uint16_t)ev_x<<16)|(uint16_t)ev_y;
            message->value[2]=((ev_buttons&31u)<<24);
            message->value[3]=ev_tick;
            gui_keyboard_dropped=0;
            ksessions_mouse_consume();
            if(!(ev_buttons&1u))mouse_capture_pid=0;
            return 1;
        }
    }
    /* The focused application window takes the keyboard; otherwise the session
       owner (desktop/shell) keeps it.  Resolved before the backpressure check so
       a busy owner cannot swallow a key that belongs to the focused app. */
    {uint32_t want=kgame_owns_screen()?(uint32_t)kgame_owner():(uint32_t)kernel_gui_focus_pid();
#if CONFIG_TOS
     if(kernel_tos_async_owns(want))return kernel_tos_async_keyboard();
#endif
     if(want&&want!=(uint32_t)pid){uint8_t k;
         for(k=0;k<USER_PROCESS_SLOTS;++k)if(process_table[k].pid==want&&
             process_table[k].state!=USER_PROCESS_FREE&&
             process_table[k].state!=USER_PROCESS_TERMINATED){target=&process_table[k];break;}}}
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
    ++kdbg_wake_keys;
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
    /* Only the current session may consume keyboard input.  Without this gate
       an inactive session's terminal (e.g. the shell) steals every key from
       the desktop/TOS session, so the TOS program and the session hotkey never
       see a key at all. */
    if(process->terminal!=ksessions_current())return 0;
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

/* ONE LIVE COPY PER SYSTEM, not per desktop.  Each desktop keeps its own launch
   list, so the same application could be started from two desktops and the second
   copy then fought the first over process slots, window slots and the screen.
   The kernel is the only layer that sees every session, so the lookup lives here:
   it matches the 8.3 program name and reports the owning terminal session. */
static uint32_t user_find_running_app(const char *path,unsigned *session)
{
    char want[13];const char *base=path;uint8_t i;unsigned n;
    if(session)*session=0;
    if(!path)return 0;
    for(i=0;path[i];++i)if(path[i]=='\\'||path[i]=='/')base=path+i+1;
    for(n=0;n<12u&&base[n];++n)want[n]=base[n];
    want[n]='\0';
    if(!want[0])return 0;
    /* Use the same accessors the task manager uses: they already skip free slots
       and own the layout of the process table. */
    for(i=0;i<USER_PROCESS_SLOTS;++i){
        char name[13];uint32_t info[4];unsigned k;int same=1;
        if(!kernel_user_process_name(i,name))continue;
        for(k=0;k<8u;++k){
            char x=name[k],y=want[k];
            if(x>='a'&&x<='z')x=(char)(x-32);
            if(y>='a'&&y<='z')y=(char)(y-32);
            if(x!=y){same=0;break;}
            if(!x)break;
        }
        if(!same)continue;
        if(!kernel_user_process_info(i,info)||!info[0])continue;
        if(session)*session=kernel_user_pid_terminal(info[0]);
        return info[0];
    }
    return 0;
}

/* Publiczne wejscie dla WM (pasek): "czy ta aplikacja juz gdzies chodzi?".
   Dzieki temu klik w skrot nie tworzy drugiej kopii, tylko przywraca okno. */
uint32_t kernel_user_find_running_app(const char *path,unsigned *session)
{
    return user_find_running_app(path,session);
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

/* The TOS personality reads the pending command line here, so a program
   launched from the shell ("RUN FOO.PRG ARGS") gets the same arguments the
   native spawn path would.  Consumed (cleared) by kernel_user_begin() for
   native processes; the shell resets it after a TOS run. */
const char *kernel_user_get_arguments(void){return pending_arguments;}

int kernel_user_begin(const void *image,uint32_t size,const char *working_directory,
                      void *stack_top)
{uint8_t i,slot;uint32_t stack_size;
 stack_size=(image&&size>=32u)?user_be32((const uint8_t *)image+24u):0u;
 user_reclaim_zombie_if_full();
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
{uint8_t i;{uint32_t v=0;uint8_t z;
 kdbg_end_pid=current_process->pid;kdbg_end_term=current_process->terminal;
 kdbg_end_status=user_syscall_result;
 kdbg_end_pc=current_process->saved_pc;kdbg_end_image=(uint32_t)(uintptr_t)current_process->image;
 for(z=0;z<4u&&current_process->program_name[z];++z)v=(v<<8)|(uint8_t)current_process->program_name[z];
 kdbg_end_name=v;}
 for(i=0;i<USER_MEMORY_BLOCKS;++i)if(user_memory[i].pointer){
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
 if(kernel_user_service_pid("SYSTEM.GUI")==current_process->pid){
     for(i=0;i<SESSION_COUNT;i++)user_gui_release_session(i,0);
 }else{extern uint32_t kdbg_rel_end; ++kdbg_rel_end; user_gui_release_session(current_process->terminal,current_process->pid);}
 user_gui_notify_exit(current_process->pid,user_syscall_result);
/* A game that exits (or faults) while owning the screen gives it back. */
  kgame_release_pid(current_process->pid);
/* Drop the exiting client's compositor windows before its surfaces go. */
  kernel_wm_close_pid(current_process->pid);
   user_service_remove_pid(current_process->pid);ksurface_release_owner(current_process->pid);
   kaudio_release(current_process->pid);ksnd_release(current_process->pid);ksnd_owner_exit(current_process->pid);
 current_process->state=USER_PROCESS_TERMINATED;user_image=0;user_image_size=0;user_arguments[0]='\0';}

static void user_wake_sleepers(uint32_t now)
{uint8_t i;for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].state==USER_PROCESS_SLEEPING&&
    (int32_t)(now-process_table[i].wake_tick)>=0){process_table[i].state=USER_PROCESS_READY;
        process_table[i].saved_registers[0]=now;}}

/* CPU weighting per console (docs/terminal-plan.md).  The session on screen
 * runs a long slice; a background session gets a one-tick slice so it still
 * makes progress (~5-6% of the CPU) without stealing the foreground.  With
 * A=16 and B=1 a single background process gets 1/(16+1)=5.9%, two get 5.6%
 * each.  The weight is derived from ksessions_current(), so switching sessions
 * re-weights the tasks with no extra API. */
#define USER_QUANTUM_ON_SCREEN   16u
#define USER_QUANTUM_BACKGROUND   1u
static uint8_t user_task_quantum(const user_process_t *p)
{
    return (uint8_t)(p->terminal==(uint8_t)ksessions_current()
                     ? USER_QUANTUM_ON_SCREEN : USER_QUANTUM_BACKGROUND);
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
    user_reclaim_zombie_if_full();
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
    user_reclaim_zombie_if_full();
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

/* Pick the next READY process for the preemptive dispatch.  Timer-C preempts
 * the running user every tick and kernel_dispatch_ready_user() selects what to
 * run next; the old code advanced next_process_slot unconditionally, so the
 * quantum was ignored for CPU-bound tasks (a plain round robin).  A process
 * now keeps the CPU while it still owns quantum time, which gives the session
 * on screen USER_QUANTUM_ON_SCREEN ticks for every USER_QUANTUM_BACKGROUND of
 * a background session. */
static user_process_t *user_pick_ready(void)
{
    uint8_t i,slot;user_process_t *p=current_process;
    if(p&&p->state==USER_PROCESS_READY&&p->quantum_left){--p->quantum_left;return p;}
    if(p&&p->state==USER_PROCESS_READY)p->quantum_left=user_task_quantum(p);
    for(i=0;i<USER_PROCESS_SLOTS;++i){slot=(uint8_t)((next_process_slot+i)%USER_PROCESS_SLOTS);
        p=&process_table[slot];
        if(p->state==USER_PROCESS_READY){
            p->quantum_left=(uint8_t)(user_task_quantum(p)-1u);
            next_process_slot=(uint8_t)((slot+1u)%USER_PROCESS_SLOTS);
            return p;}}
    return 0;
}
uint32_t kernel_user_take_ready(uint32_t *registers,uint32_t *pc,uint16_t *sr)
{
    uint8_t j;user_process_t *p;
    if(!registers||!pc||!sr)return 0;
    /* No user IRQ can wake sleepers when all users are blocked. */
    user_wake_sleepers(kernel_user_clock());
    p=user_pick_ready();
    if(!p)return 0;
    current_process=p;current_process->state=USER_PROCESS_RUNNING;
    for(j=0;j<15u;++j)registers[j]=current_process->saved_registers[j];
    *pc=current_process->saved_pc;*sr=current_process->saved_sr;
    return current_process->saved_usp;
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
    uint8_t i;int32_t now;
    {extern uint32_t kdbg_sleep_calls,kdbg_sleep_zero;++kdbg_sleep_calls;
     if(ticks>3000u||!registers||!pc||!sr){++kdbg_sleep_zero;return 0;}}
    if(!user_heaps_valid_current()){
        user_heap_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    if(!user_stack_valid(current_process,usp)){
        user_stack_fail_current();return user_switch_frame(registers,0,pc,sr,0,0);
    }
    now=kernel_user_yield();if(now<0)return 0;
    for(i=0;i<15u;++i)current_process->saved_registers[i]=registers[i];
    current_process->saved_registers[0]=(uint32_t)now+ticks;
    current_process->saved_usp=usp;current_process->saved_pc=*pc;current_process->saved_sr=*sr;
    current_process->wake_tick=(uint32_t)now+ticks;current_process->registers_valid=1u;
    /* sleep(0) is a cooperative yield: route input without waiting 20 ms. */
    current_process->state=ticks?USER_PROCESS_SLEEPING:USER_PROCESS_READY;
    /* Always hand control back to the supervisor for one pass.  It must route
       pending input, sample the mouse and composite a frame.  Returning 0 made
       pause_tick() a no-op for the last runnable process (WMDEMO's idle loop
       starved the main loop and froze input/compositing), while switching
       straight to a ready peer let two clients trading pause_tick() ping-pong
       below the scheduler: the compositor saw one frame and a scripted drag
       collapsed to a single sampled position. */
    {extern uint32_t kdbg_sleep_park;++kdbg_sleep_park;}
    return (uint32_t)-1;
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
{int32_t status;uint32_t i,sp;
 if(!user_active)return 0;
 kdbg_uf_count++;kdbg_uf_pid=current_process->pid;kdbg_uf_vector=vector;
 kdbg_uf_pc=kernel_fault_pc;kdbg_uf_a0=kernel_fault_a0;kdbg_uf_d0=kernel_fault_d0;
 kdbg_uf_sr=kernel_fault_sr;kdbg_uf_usp=kernel_fault_usp;
 kdbg_uf_image=(uint32_t)(uintptr_t)current_process->image;
 kdbg_uf_savedpc=current_process->saved_pc;
 kdbg_uf_state=(uint32_t)current_process->state;
 kdbg_uf_w4=kernel_fault_w4;kdbg_uf_w6=kernel_fault_w6;kdbg_uf_w8=kernel_fault_w8;
 kdbg_uf_w10=kernel_fault_w10;kdbg_uf_w12=kernel_fault_w12;
 for(i=0;i<32u;++i)kdbg_uf_stack[i]=0u;
 sp=kernel_fault_usp;
 if(current_process->runtime_stack&&
    sp>=(uint32_t)(uintptr_t)current_process->runtime_stack&&
    sp+128u<=(uint32_t)(uintptr_t)current_process->runtime_stack+
                       current_process->runtime_stack_size){
     const uint8_t *p=(const uint8_t *)(uintptr_t)sp;
     for(i=0;i<32u;++i){
         kdbg_uf_stack[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
                          ((uint32_t)p[2]<<8)|(uint32_t)p[3];
         p+=4;}
 }
 status=-(int32_t)(0x100u+vector);
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
                             uint32_t datetime,void *opaque)
{
    user_dir_context_t *context=(user_dir_context_t *)opaque;uint8_t i;
    if(context->current++!=context->wanted)return 1;
    for(i=0;i<12u&&name[i];++i)context->entry->name[i]=name[i];
    context->entry->name[i]='\0';
    context->entry->attributes=(uint8_t)attributes;
    context->entry->padding[0]=context->entry->padding[1]=0;
    context->entry->size=size;context->entry->datetime=datetime;context->found=1;return 0;
}

/* A page of directory entries is collected in one FAT traversal. */
typedef struct {uint32_t skip,seen,count,capacity;user_dir_entry_t *entries;} user_dir_page_t;
static int user_dir_page_callback(const char *name,uint32_t size,uint32_t attr,uint32_t datetime,void *opaque)
{
    user_dir_page_t *page=opaque;user_dir_entry_t *e;unsigned i;
    if(page->seen++<page->skip)return 1;
    e=&page->entries[page->count++];
    for(i=0;i<12u&&name[i];i++)e->name[i]=name[i];
    e->name[i]=0;e->attributes=(uint8_t)attr;e->padding[0]=e->padding[1]=0;e->size=size;e->datetime=datetime;
    return page->count<page->capacity;
}

int32_t kernel_user_syscall_result(void){return user_syscall_result;}

/* Session output mirror.  A .APP writes diagnostics with ATOS_WRITE, which
   goes to its session console; a session started without one (for example from
   a kernel-side launch) would lose those messages entirely.  Keeping the last
   few kilobytes here makes them readable through the debugger, so "it exited
   silently" stops being a dead end. */
uint8_t kdbg_session_out[4096];
uint32_t kdbg_session_out_len;
static void kdbg_session_out_append(const char *text)
{
    while(text&&*text){
        kdbg_session_out[kdbg_session_out_len&4095u]=(uint8_t)*text++;
        ++kdbg_session_out_len;
    }
}


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

/* Diagnostics: refusal code of the ATOS_GAME_OPCODE dispatch (before kgame_begin). */
int32_t kdbg_game_disp_rc;
/* Completed GUI submissions, draw/compose clocks, worst total clocks,
   commands, groups, >25 ms frames, >33 ms frames. Passive telemetry. */
uint32_t kdbg_gui_frame_stats[8];

/* --- ROZMIAR IKON + SIATKA PULPITU ------------------------------------------
   Wartosci trzyma JADRO, zeby SETTINGS mogl je zmienic, a pulpit zauwazyl zmiane
   pollingiem (ta sama droga co kolor tla/paska/tapety).  Zapis w
   \SYSTEM\ICONSZ.CFG, wiec nastepny start rozwija od razu wlasciwa sekcje
   ICONS.DAT, bez drugiego dekodowania. */
/* Diagnostyka opcodow 48/49/50 (memdump): ile razy klient pytal o rozmiar i
   jaka wartosc dostal.  Pozwala odroznic "pulpit nie pyta" od "pulpit pyta,
   ale dostaje stare". */
uint32_t kdbg_icon_q_calls,kdbg_icon_q_last,kdbg_icon_set_calls;
static void icons_cfg_save(void)
{
    char text[8];unsigned k=0;unsigned px=kgfx_icons_size();
    if(px>=10u){text[k++]=(char)('0'+px/10u);text[k++]=(char)('0'+px%10u);}
    else text[k++]=(char)('0'+px);
    text[k++]='\n';
    text[k++]=(char)('0'+kgfx_icon_grid());
    text[k++]='\n';
    (void)kfat_save_path("\\SYSTEM\\ICONSZ.CFG",text,k);
}
/* Przeladuj ikony w innym rozmiarze: plik czyta JADRO (tylko ono ma FAT),
   potem zwalnia poprzednie dane i rozwija wybrana sekcje. */
static int32_t icons_size_set(unsigned px)
{
    static const char icons_path[]="\\SYSTEM\\ICONS.DAT";
    uint32_t blob_size=0,got=0;uint8_t attributes=0;uint8_t *blob;uint32_t ver;
    if(kfat_stat_path(icons_path,&blob_size,&attributes)!=FAT_FILE_FOUND||
       (attributes&0x10u)||blob_size<sizeof(icons_blob_header_t)||blob_size>0x100000u)
        return -0x3e0;
    blob=(uint8_t *)kmem_alloc(blob_size,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!blob)return -0x3e1;
    if(kfat_load_path(icons_path,blob,blob_size,&got)!=FAT_FILE_FOUND||got!=blob_size){
        (void)kmem_free(blob,KMEM_OWNER_KERNEL);return -0x3e0;}
    ver=((uint32_t)blob[4]<<24)|((uint32_t)blob[5]<<16)|((uint32_t)blob[6]<<8)|(uint32_t)blob[7];
    kgfx_icons_unload();
    kgfx_icons_request((uint16_t)px);
    if(!kgfx_icons_load(blob,blob_size)){
        (void)kmem_free(blob,KMEM_OWNER_KERNEL);return -0x3e0;}
    /* Spakowane wersje maja wlasny bufor; v1 rysuje WPROST z pliku. */
    if(ver>=2u)(void)kmem_free(blob,KMEM_OWNER_KERNEL);
    kgfx_icon_palette();
    {extern void kernel_wm_damage_all(void);extern void kernel_wm_desktop_repaint(void);
     kernel_wm_damage_all();kernel_wm_desktop_repaint();}
    return 0;
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
    /* File operations act on the calling console's own mounted volume. */
    (void)kfat_select_session(current_process?current_process->terminal:0u);
    if(call==0u){user_syscall_result=(int32_t)argument0;kernel_user_end();return 1;}
    if(call==1u){
        if(!services||!services->console_puts||!user_string((const char *)pointer0,4096u)){user_syscall_result=-0x201;return 0;}
        kdbg_session_out_append((const char *)pointer0);
        user_console_begin_output();kconsole_puts_to(current_process->terminal,(const char *)pointer0);return 0;}
    if(call==2u){if(!user_path((const char *)pointer0,path)){user_syscall_result=-0x210;return 0;}
        if(kfat_stat_path(path,&size,&attributes)!=FAT_FILE_FOUND||(attributes&0x10u)){user_syscall_result=-0x211;return 0;}
        for(i=0;i<USER_FILE_HANDLES;++i)if(!user_files[i].open){uint8_t j;for(j=0;j<64u;++j)user_files[i].path[j]=path[j];user_files[i].size=size;user_files[i].offset=0;user_files[i].cursor.valid=0;user_files[i].cursor.offset=0;user_files[i].open=1;user_syscall_result=i;return 0;}
        user_syscall_result=-0x212;return 0;}
    if(call==3u){if(argument0>=USER_FILE_HANDLES||!user_files[argument0].open){user_syscall_result=-0x220;return 0;}
        if(!user_write_range(pointer0,argument1)){user_syscall_result=-0x221;return 0;}
        if(kfat_read_cursor(user_files[argument0].path,user_files[argument0].offset,pointer0,argument1,&got,&user_files[argument0].cursor)!=FAT_FILE_FOUND){user_syscall_result=-0x222;return 0;}
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
        character[0]=(char)value;character[1]='\0';kdbg_session_out_append(character);user_console_begin_output();
        kconsole_puts_to(current_process->terminal,character);user_syscall_result=1;return 0;}
    if(call==9u){user_syscall_result=kernel_user_session_getchar();return 0;}
    if(call==10u){user_syscall_result=kernel_user_yield();return 0;}
    if(call==11u){user_syscall_result=kernel_user_sleep(argument0);return 0;}
    if(call==59u){user_syscall_result=kernel_language_save(argument0);return 0;}
    if(call==74u){uint32_t *st=(uint32_t *)pointer1;unsigned session=0;uint32_t pid;
        /* Opcode 74 (ATOS_CALL_APP_ACTIVATE).  This used to sit on 66, which is
           the FROZEN ATOS_CALL_ABI_VERSION: applications that ask for the ABI
           version got this service instead, saw a bogus value and refused to
           run (first observed with the Flashback port). */
        /* Activate-or-report.  The desktop asks BEFORE spawning: when the
           application already runs somewhere, no second copy is created; when it
           runs on another desktop the kernel brings that desktop forward, because
           only the kernel may switch sessions. */
        if(!user_path((const char *)pointer0,path)||
           (st&&!user_write_aligned(st,8u,2u))){user_syscall_result=-0x240;return 0;}
        pid=user_find_running_app(path,&session);
        if(!pid){user_syscall_result=0;return 0;}                 /* not running: caller spawns */
        ++kdbg_app_seen;                                          /* diagnostics */
        if(st){st[0]=pid;st[1]=session;}
        if(session!=ksessions_current()){
            (void)ksessions_control(session+1u,0);                /* show its desktop */
            ++kdbg_app_switch;
            user_syscall_result=2;return 0;}
        user_syscall_result=1;return 0;}                          /* here: caller shows it */
    if(call==26u){uint32_t *m=(uint32_t *)pointer0;
        /* Work counters for the boot-time measurement harness: full traversals
           of the kernel block list and set/way probes in the block cache. */
        extern uint32_t kmem_list_walks;extern uint32_t kblock_search_count(void);
        if(!m||!user_write_aligned(m,8u,2u)){user_syscall_result=-0x240;return 0;}
        m[0]=kmem_list_walks;m[1]=kblock_search_count();
        user_syscall_result=0;return 0;}
    if(call==56u){
        if(argument0!=sizeof(system_report_t)||!user_write_aligned(pointer0,sizeof(system_report_t),2u)){user_syscall_result=-0x280;return 0;}
        user_syscall_result=kernel_system_report((system_report_t *)pointer0);return 0;}
    if(call==57u){uint32_t start=0,limit=0,j;
        for(j=0;j<kpart_count();j++)if(kpart_is_boot((uint8_t)j)){start=kpart_start((uint8_t)j);limit=kpart_size((uint8_t)j);break;}
        if(!argument1||argument1>8||argument0>=limit||argument1>limit-argument0||argument0>0xffffffffUL-start||
           !user_write_range(pointer0,argument1*512u)){user_syscall_result=-0x281;return 0;}
        user_syscall_result=kblock_read_uncached(start+argument0,(uint16_t)argument1,pointer0)?(int32_t)(argument1*512u):-0x282;return 0;}
    if(call==58u){user_syscall_result=(int32_t)kernel_user_clock();return 0;}
    if(call==67u){user_syscall_result=(int32_t)kernel_user_hires_clock();return 0;}
    /* ---- audio (ABI 1.3).  PSG only; the first call claims the single owner,
       and a second process is refused instead of sharing the channels. ---- */
    if(call==75u){user_syscall_result=(int32_t)kaudio_caps();return 0;}
    if(call==76u){
        unsigned ch=(unsigned)(argument0&0xffu),vol=(unsigned)((argument0>>8)&0x0fu);
        uint16_t period=(uint16_t)((argument0>>16)&0xfffu);
        user_syscall_result=kaudio_claim(current_process->pid,current_process->terminal);
        if(user_syscall_result>=0)
            user_syscall_result=kaudio_tone(current_process->pid,ch,period,(uint8_t)vol);
        return 0;}
    if(call==77u){
        unsigned vol=(unsigned)((argument0>>8)&0x0fu);
        uint16_t period=(uint16_t)((argument0>>16)&0x1fu);
        user_syscall_result=kaudio_claim(current_process->pid,current_process->terminal);
        if(user_syscall_result>=0)
            user_syscall_result=kaudio_noise(current_process->pid,period,(uint8_t)vol);
        return 0;}
    if(call==78u){
        user_syscall_result=kaudio_claim(current_process->pid,current_process->terminal);
        if(user_syscall_result>=0)
            user_syscall_result=kaudio_stop(current_process->pid,(uint8_t)(argument0&0x07u));
        return 0;}
    if(call==79u){uint32_t count=argument0;
        if(count==0u||count>KAUDIO_QUEUE_EVENTS){user_syscall_result=-0x3f1;return 0;}
        if(!user_range(pointer0,count*(uint32_t)sizeof(kaudio_event_t))){user_syscall_result=-0x3c0;return 0;}
        user_syscall_result=kaudio_claim(current_process->pid,current_process->terminal);
        if(user_syscall_result>=0)
            user_syscall_result=kaudio_queue(current_process->pid,
                                             (const kaudio_event_t *)pointer0,count);
        return 0;}
    if(call==80u){uint32_t *state=(uint32_t *)pointer0;
        if(!user_write_range(pointer0,16u)){user_syscall_result=-0x3c0;return 0;}
        state[0]=kaudio_owner();state[1]=kaudio_dbg_queued();
        state[2]=kaudio_dbg_events();state[3]=kaudio_dbg_rejected();
        user_syscall_result=0;return 0;}
    if(call==82u){user_syscall_result=kpcm_control(current_process->pid,current_process->terminal,argument0);return 0;}
    if(call==83u){
        if(!argument0||argument0>KPCM_MAX_SUBMIT||!user_range(pointer0,argument0)){
            user_syscall_result=-0x3f1;return 0;}
        user_syscall_result=kpcm_submit(current_process->pid,(const uint8_t *)pointer0,argument0);return 0;}
    if(call==84u){
        if(!user_write_aligned(pointer0,32u,2u)){user_syscall_result=-0x3c0;return 0;}
        kpcm_state((uint32_t *)pointer0);user_syscall_result=0;return 0;}
    if(call==85u){user_syscall_result=kpcm_clip_create(current_process->pid,argument0);return 0;}
    /* ATOS Sound 1.0: transport ASND przez usluge jadra (aplikacja NIE wola NF).
       Wlasciciela audio sprawdzamy ta sama regula co ton/kolejka/PCM. */
    if(call==86u){
        int32_t owned;
        /* Walidacja PRZED przejeciem zasobu: bledne zadanie nie moze zajac
           urzadzenia (audyt r3, punkt 2). */
        if(!argument0||argument0>4128u||!user_range(pointer0,argument0)||
           !argument1||argument1<256u||argument1>4128u||!user_write_range(pointer1,argument1)){
            user_syscall_result=-0x401;return 0;}
        /* OSOBNA domena wlasciciela niz PSG: Arkanoid i PCM nie blokuja sie. */
        owned=ksnd_claim(current_process->pid);
        if(owned){user_syscall_result=owned;return 0;}
        user_syscall_result=ksnd_exchange(current_process->pid,pointer0,argument0,pointer1,argument1);
        return 0;}
    if(call==88u){
        /* Stan i naprawa woluminu (FAT[1] "clean shutdown").
           d1=0 -> enum fat_health (0..3), d1=1 -> naprawa (0 lub ujemny kod).
           Naprawa jest tylko na wyraźne zadanie uzytkownika: sprawdza obie
           kopie FAT i dopiero potem ustawia bity 14+15. */
        if(argument0==0u){user_syscall_result=(uint32_t)kfat_health();return 0;}
        if(argument0==1u){user_syscall_result=(uint32_t)(int32_t)kfat_volume_repair();return 0;}
        user_syscall_result=(uint32_t)(-FAT_WRITE_INVALID);return 0;}
    if(call==87u){
        uint32_t info[4];uint8_t k;
        if(!user_write_aligned(pointer0,16u,2u)){user_syscall_result=-0x3c0;return 0;}
        ksnd_info(info);
        for(k=0;k<4u;++k)((uint32_t *)pointer0)[k]=info[k];
        user_syscall_result=0;return 0;}
    /* Process manager (TASKMGR): enumerate by slot and control by pid. */
    if(call==68u){char name[13];uint8_t k;
        if(!user_write_range(pointer0,13u)){user_syscall_result=-0x3d0;return 0;}
        if(!kernel_user_process_name(argument0,name)){user_syscall_result=-0x3d1;return 0;}
        for(k=0;k<13u;++k)((char *)pointer0)[k]=name[k];
        user_syscall_result=0;return 0;}
    if(call==69u){uint32_t info[4];uint8_t k;
        if(!user_write_aligned(pointer0,16u,2u)){user_syscall_result=-0x3d2;return 0;}
        if(!kernel_user_process_resources(argument0,info)){user_syscall_result=-0x3d3;return 0;}
        for(k=0;k<4u;++k)((uint32_t *)pointer0)[k]=info[k];
        user_syscall_result=0;return 0;}
    if(call==70u){user_syscall_result=(int32_t)kernel_user_process_cpu_ticks(argument0);return 0;}
    if(call==71u){user_syscall_result=kernel_user_kill(argument0);return 0;}
    if(call==72u){user_syscall_result=kernel_user_suspend(argument0);return 0;}
    if(call==73u){user_syscall_result=kernel_user_resume(argument0);return 0;}
    if(call==12u){if(!user_write_aligned(pointer0,24u,2u)){user_syscall_result=-0x280;return 0;}
        user_syscall_result=kernel_user_system_info((uint32_t *)pointer0);return 0;}
    if(call==61u){const kernel_shell_service_t *sh=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);
        if(!sh||!sh->submit||!sh->run_pending||argument0<2u||!user_string((const char *)pointer0,64u)||
           !user_write_range(pointer1,argument0)){user_syscall_result=-0x3b0;return 0;}
        kconsole_capture_begin((char *)pointer1,argument0);
        sh->submit((const char *)pointer0);
        user_syscall_result=0;return 0;}
    if(call==62u){user_syscall_result=kconsole_capture_state();return 0;}
    if(call==63u){char *out=(char *)pointer0;uint32_t cap=argument0,len=0u,i;char name[13];uint32_t fsize;uint8_t attr;
        if(!out||cap<16u||!user_write_range(out,cap)||!services||!services->floppy_dir){user_syscall_result=-0x3c0;return 0;}
        if(services->floppy_mount)(void)services->floppy_mount();
        for(i=0u;i<64u;++i){
            uint32_t n;char num[12];uint32_t k=0u,v;
            if(!services->floppy_dir(i,name,&fsize,&attr))break;
            n=0u;while(name[n])++n;
            if(len+n+14u>cap)break;
            for(n=0u;name[n];++n)out[len++]=name[n];
            while(len<cap&&(len%12u)!=0u)out[len++]=' ';
            v=fsize;if(!v)num[k++]='0';while(v){num[k++]=(char)('0'+v%10u);v/=10u;}
            while(k){out[len++]=num[--k];}
            out[len++]=(char)(attr&0x10u?'/':'\n');
            if(!(attr&0x10u)&&len<cap)out[len++]='\n';
        }
        if(len<cap)out[len]=0;
        user_syscall_result=(int32_t)len;return 0;}
    if(call==64u){char name[13];uint32_t fsize=0u,i;
        if(!user_string((const char *)pointer0,13u)||!user_write_range(pointer1,argument0)||!services||!services->floppy_read_file){user_syscall_result=-0x3c1;return 0;}
        for(i=0u;i<12u&&((const char *)pointer0)[i];++i)name[i]=((const char *)pointer0)[i];
        name[i]=0;
        if(services->floppy_mount)(void)services->floppy_mount();
        if(!services->floppy_read_file(name,pointer1,argument0,&fsize)){user_syscall_result=-1;return 0;}
        user_syscall_result=(int32_t)fsize;return 0;}
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
    if(call==60u){
        if(!user_path((const char *)pointer0,path)||(argument0&&!user_range(pointer1,argument0))){
            user_syscall_result=-FAT_WRITE_INVALID;return 0;}
        user_syscall_result=kfat_save_path(path,pointer1,argument0);return 0;
    }
    if(call==65u){
        /* ABI-compatible blocking call for existing clients; d1=1 queues a
           cooperative domain so the desktop can keep serving other windows. */
        if(!user_path((const char *)pointer0,path)){user_syscall_result=-0x2b0;return 0;}
#if CONFIG_TOS
        if(argument0==1u){user_syscall_result=kernel_tos_start_async(path);return 0;}
#endif
        user_syscall_result=kernel_tos_run_program(path);return 0;
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
    if(call==16u){void *allocation;uint32_t private_bytes=0,fast_bytes=0;
        uint32_t limit=argument1?USER_MEMORY_QUOTA_TT:USER_MEMORY_QUOTA;
        if(!argument0||argument0>limit||argument1>1u){user_syscall_result=-0x2c0;return 0;}
        for(i=0;i<USER_MEMORY_BLOCKS;++i){
            if((uint32_t)(uintptr_t)user_memory[i].pointer>=USER_FAST_RAM_BASE)fast_bytes+=user_memory[i].size;
            else private_bytes+=user_memory[i].size;}
        if(argument1&&argument0>USER_MEMORY_QUOTA){
            /* Large request: only the fast-RAM budget can satisfy it. */
            if(fast_bytes>USER_MEMORY_QUOTA_TT||argument0>USER_MEMORY_QUOTA_TT-fast_bytes){user_syscall_result=-0x2c3;return 0;}
        }else if(argument1&&fast_bytes<=USER_MEMORY_QUOTA_TT&&argument0<=USER_MEMORY_QUOTA_TT-fast_bytes){
            /* Small fast-RAM request within budget; an ST fallback is re-checked below. */
        }else if(private_bytes>USER_MEMORY_QUOTA||argument0>USER_MEMORY_QUOTA-private_bytes){user_syscall_result=-0x2c3;return 0;}
        for(i=0;i<USER_MEMORY_BLOCKS&&user_memory[i].pointer;++i){}
        if(i==USER_MEMORY_BLOCKS){user_syscall_result=-0x2c1;return 0;}
        allocation=kmem_alloc_user(argument0+2u*USER_HEAP_GUARD_BYTES,16u,user_memory_owner,KMEM_READ|KMEM_WRITE,argument1!=0u);
        if(!allocation){user_syscall_result=-0x2c2;return 0;}
        if((uint32_t)(uintptr_t)allocation<USER_FAST_RAM_BASE&&
           (private_bytes>USER_MEMORY_QUOTA||argument0>USER_MEMORY_QUOTA-private_bytes)){
            /* The fast pool was full or absent and the block fell back into
               ST-RAM: that is the 1 MB budget again. */
            (void)kmem_release(allocation,user_memory_owner);
            user_syscall_result=-0x2c3;return 0;}
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
    if(call==81u){
        const gui_draw_t *list=(const gui_draw_t *)pointer0;
        const uint16_t *groups=(const uint16_t *)pointer1;
        uint8_t *surface,*previous;uint32_t g,offset=0;
        unsigned session=current_process->terminal;
        uint32_t started,drawn,ended;
        /* Validate the entire frame before changing a surface. In particular,
           no caller-supplied pid/target pointer can redirect a draw. */
        if(!argument0||argument0>GUI_FRAME_MAX||!argument1||argument1>GUI_FRAME_GROUPS||
           ((uintptr_t)list&1u)||((uintptr_t)groups&1u)||
           !user_range(list,argument0*sizeof(*list))||
           !user_range(groups,argument1*sizeof(*groups))||
           !gui_frame_counts_valid(argument0,argument1,groups)){
            user_syscall_result=-0x3e0;return 0;}
        if(session!=ksessions_current()) {user_syscall_result=1;return 0;}
        if(!kernel_wm_enabled()||!kgfx_desktop_active()||kgame_active()){
            user_syscall_result=-0x3e5;return 0;}
        surface=kernel_wm_app_surface(current_process->pid,0u);
        if(!surface){user_syscall_result=-0x3f0;return 0;}
        /* One trap, no broker scheduling and no partially presented batches. */
        started=kernel_user_hires_clock();
        kgfx_overlay_hide();kgfx_cursor_hide();
        previous=kgfx_set_target(surface);
        for(g=0;g<argument1;g++){
            int32_t l,t,r,b;
            kgfx_draw_batch(list+offset,groups[g]);
            if(kgfx_batch_bounds(&l,&t,&r,&b))kernel_wm_app_dirty(surface,l,t,r,b);
            offset+=groups[g];
        }
        (void)kgfx_set_target(previous);
        drawn=kernel_user_hires_clock();
        kernel_wm_compose_now();
        ended=kernel_user_hires_clock();
        ++kdbg_gui_frame_stats[0];
        kdbg_gui_frame_stats[1]+=drawn-started;
        kdbg_gui_frame_stats[2]+=ended-drawn;
        if(ended-started>kdbg_gui_frame_stats[3])kdbg_gui_frame_stats[3]=ended-started;
        kdbg_gui_frame_stats[4]+=argument0;kdbg_gui_frame_stats[5]+=argument1;
        if(ended-started>307u)++kdbg_gui_frame_stats[6];
        if(ended-started>410u)++kdbg_gui_frame_stats[7];
        return 0;
    }
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
        unsigned client_session=kernel_user_pid_terminal(argument0);
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GFX")){
            user_syscall_result=-0x3e1;return 0;}
        if(!user_range(request,sizeof(*request))){user_syscall_result=-0x3e0;return 0;}
        /* This is a real serviced graphics request: the requests-per-frame
           counter is sampled by the compositor once per composed frame. */
        kgfx_note_request();
        {extern uint32_t kdbg_last_opcode,kdbg_last_color;   /* diagnostyka: co broker dostal */
         kdbg_last_opcode=request->opcode;kdbg_last_color=request->color;}
        /* SYSTEM.GFX is the privileged broker, but argument0 preserves the
           kernel-authenticated PID of its original IPC client. Once a GUI
           owner exists, authorization is checked in the client's session. */
        /* Draw batches (18) and window geometry (20) are allowed for ANY live
           client of the active session, not just the GUI keyboard owner: the
           desktop owner is one process, and a second application must be able
           to paint its own window.  The active-session check below still keeps
           background sessions off the visible screen. */
        /* Opcodes a second application window legitimately needs: drawing (18),
           its own geometry/title (20/21), the cycle hotkey (22), the surface
           queries (1/9/10/11) and its own document height (29).  The video-mode
           QUERY (30 with a=0) is read-only and also allowed, so a windowed client
           can size itself for the active resolution; changing the mode stays with
           the session owner, as does every global opcode. */
        {int allowed=(request->opcode==1u||request->opcode==9u||request->opcode==10u||
                      request->opcode==11u||request->opcode==18u||request->opcode==20u||
                      request->opcode==21u||request->opcode==22u||request->opcode==29u||
                      (request->opcode==32u&&request->color==argument0)||
                      (request->opcode==30u&&request->xy==0u)||
                      request->opcode==ATOS_GFX_DISPLAY_SET||request->opcode==35u||request->opcode==36u||request->opcode==37u||request->opcode==39u||   /* depth/icon/theme */
                      request->opcode==45u||request->opcode==46u||request->opcode==47u||
                      /* 48/49/50 = rozmiar ikon i siatka pulpitu. MUSZA byc tu:
                         gdy fokus ma SETTINGS, to DESKTOP nie jest wlascicielem
                         klawiatury i jego polling (op 49) byl odrzucany - zmiana
                         rozmiaru nie dochodzila do pulpitu. */
                      request->opcode==48u||request->opcode==49u||request->opcode==50u||
                      request->opcode==ATOS_GAME_OPCODE);   /* game mode: checked by its own rules below */
         if(gui_keyboard_pids[client_session]&&argument0!=gui_keyboard_pids[client_session]&&!allowed){
            user_syscall_result=-0x3e5;return 0;}}
        /* 38: exclusive full-screen game mode (include/atos_game.h).  Handled
           before the active-session gate: FRAME itself reports a hidden
           session so the game knows to repaint when it comes back. */
        if(request->opcode==ATOS_GAME_OPCODE){kdbg_game_disp_rc=1;
            const user_process_t *owner=0;unsigned sub=(unsigned)request->color;
            for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            if(!owner){kdbg_game_disp_rc=-0x3e0;user_syscall_result=-0x3e0;return 0;}
            if(sub==ATOS_GAME_BEGIN){
                /* Only a client of the session on screen may take it.  The
                   desktop may keep the keyboard ownership; while the game owns
                   the screen, input routing hands every key and pointer event
                   to the game instead (kernel_user_gui_keyboard_wake). */
                if(client_session!=ksessions_current()){kdbg_game_disp_rc=-0x3e5;user_syscall_result=-0x3e5;return 0;}
                user_syscall_result=kgame_begin(argument0,client_session,
                                                (uint16_t)(request->xy>>16),(uint16_t)request->xy);
                return 0;
            }
            if(sub==ATOS_GAME_END){user_syscall_result=kgame_end(argument0);return 0;}
            if(sub==ATOS_GAME_FRAME){
                static atos_game_frame_t frame;   /* kernel copy: no TOCTOU on the rects */
                const uint8_t *src=(const uint8_t *)(uintptr_t)request->xy;
                const uint8_t *pixels;uint32_t k,bytes;
                if((request->xy&1u)||!user_process_range(owner,src,ATOS_GAME_FRAME_HEADER)){
                    user_syscall_result=-0x3e0;return 0;}
                for(k=0;k<ATOS_GAME_FRAME_HEADER;++k)((uint8_t *)&frame)[k]=src[k];
                if(frame.count>ATOS_GAME_RECTS||!frame.width||!frame.height||
                   frame.pitch<frame.width||frame.scale<1u||frame.scale>2u){
                    user_syscall_result=-0x3e0;return 0;}
                bytes=(uint32_t)frame.count*(uint32_t)sizeof(atos_game_rect_t);
                if(bytes){
                    if(!user_process_range(owner,src+ATOS_GAME_FRAME_HEADER,bytes)){user_syscall_result=-0x3e0;return 0;}
                    for(k=0;k<bytes;++k)((uint8_t *)frame.rect)[k]=src[ATOS_GAME_FRAME_HEADER+k];
                }
                pixels=(const uint8_t *)(uintptr_t)frame.pixels;
                if(!user_process_range(owner,pixels,(uint32_t)frame.pitch*(uint32_t)(frame.height-1u)+frame.width)){
                    user_syscall_result=-0x3e0;return 0;}
                user_syscall_result=kgame_frame(argument0,&frame,pixels);
                return 0;
            }
            if(sub==ATOS_GAME_PALETTE){
                static atos_game_palette_t palette;
                const uint8_t *src=(const uint8_t *)(uintptr_t)request->xy;uint32_t k,bytes;
                if((request->xy&1u)||!user_process_range(owner,src,ATOS_GAME_PALETTE_HEADER)){
                    user_syscall_result=-0x3e0;return 0;}
                for(k=0;k<ATOS_GAME_PALETTE_HEADER;++k)((uint8_t *)&palette)[k]=src[k];
                if(palette.first>255u||palette.count>256u-palette.first){user_syscall_result=-0x3e0;return 0;}
                bytes=(uint32_t)palette.count*3u;
                if(bytes){
                    if(!user_process_range(owner,src+ATOS_GAME_PALETTE_HEADER,bytes)){user_syscall_result=-0x3e0;return 0;}
                    for(k=0;k<bytes;++k)palette.rgb[k]=src[ATOS_GAME_PALETTE_HEADER+k];
                }
                user_syscall_result=kgame_palette(argument0,&palette);
                return 0;
            }
            user_syscall_result=-0x3e2;return 0;
        }
        if(request->opcode==19u){const user_process_t *owner=0;
            const term_cell_t *cells=(const term_cell_t *)(uintptr_t)request->xy;
            for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            if(!owner||request->wh!=TERM_CELLS||request->color||
               (request->xy&1u)||!user_process_range(owner,cells,TERM_CELLS*sizeof(*cells))){
                user_syscall_result=-0x3e0;return 0;}
            user_syscall_result=kgfx_terminal_submit(client_session,cells);
            return 0;
        }
        if(request->opcode==13u){
            if(!gui_persistent[client_session]||argument0!=gui_keyboard_pids[client_session]||
               (gui_desktop_pids[client_session]&&gui_desktop_pids[client_session]!=argument0)){
                /* Which registration condition refused the desktop. */
                if(!gui_persistent[client_session])++kdbg_desk_rejp;
                else if(argument0!=gui_keyboard_pids[client_session])++kdbg_desk_rejk;
                else ++kdbg_desk_rejd;
                user_syscall_result=-0x3e4;return 0;}
            ++kdbg_desk_rejok;if(client_session<SESSION_COUNT)++kdbg_desk_rejok_s[client_session];
            if(client_session==ksessions_current()){
                kgfx_session_view(1);
                if(!kgfx_enter_desktop()){user_syscall_result=-0x3e4;return 0;}
                {extern void kgfx_session_forget_cells(unsigned);
                 kgfx_session_forget_cells(client_session);}
                /* The desktop owns the whole screen: drop the shell window so
                   the composited desktop canvas is what the user sees. */
                kernel_wm_console_hide();
            }
            gui_desktop_pids[client_session]=argument0;user_syscall_result=0;return 0;
        }
        /* Only the active session may draw: a background app must not paint
           over the screen of the session the user is looking at. */
        if(kernel_user_pid_terminal(argument0)!=ksessions_current()){user_syscall_result=0;return 0;}
        /* A legacy graphics client may overwrite cells behind the renderer.
           Batch (18) and window-geometry (20) calls do not touch the console
           surface, so invalidating its cell cache would force a full-grid
           re-render (and a full-window repaint) on every application draw. */
        if(request->opcode!=1u&&request->opcode!=9u&&request->opcode!=10u&&
           request->opcode!=11u&&request->opcode!=18u&&request->opcode!=20u&&
           request->opcode!=21u&&request->opcode!=22u&&request->opcode!=29u&&
           request->opcode!=36u)
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
        /* 37: system themes.  xy selects the theme, color the sub-operation:
           0 = query (count<<16)|current, 1 = apply, 2 = current flags,
           3 = UI ramp base, 4 = preview colours of theme xy.  The palette is
           privileged state shared by every window, so it lives in the kernel. */
        else if(request->opcode==37u){
            unsigned sub=(unsigned)request->color;
            if(sub==0u)user_syscall_result=(int32_t)((kgfx_theme_count()<<16)|kgfx_theme_current());
            else if(sub==1u)user_syscall_result=(int32_t)kgfx_theme_apply((unsigned)request->xy);
            else if(sub==2u)user_syscall_result=(int32_t)kgfx_theme_flags();
            else if(sub==3u)user_syscall_result=(int32_t)kgfx_theme_ramp_base();
            else if(sub==4u)user_syscall_result=(int32_t)kgfx_theme_preview((unsigned)request->xy);
            /* 5 = ustaw kolor TLA pulpitu (wpis 2 palety, 0xFFFF = kolor motywu),
               6 = odczytaj obecny kolor tla.  Reszta motywu (chrom okien, pasek)
               zostaje pod kontrola motywu. */
            else if(sub==5u)user_syscall_result=(int32_t)kgfx_theme_set_desktop((unsigned)(request->xy&0xffffu));
            else if(sub==6u)user_syscall_result=(int32_t)kgfx_theme_desktop();
            /* 7 = kolor PASKA (rampa 252..255, 0xFFFF = rampa motywu),
               8 = odczytaj kolor paska. */
            else if(sub==7u)user_syscall_result=(int32_t)kgfx_theme_set_bar((unsigned)(request->xy&0xffffu));
            else if(sub==8u)user_syscall_result=(int32_t)kgfx_theme_bar();
            /* 9 = tapeta (wzor 0..3 proceduralne, 4 = wlasny kafelek 16x16),
               10 = odczyt tapety.  Mask 7 (nie 3), bo `4 & 3` dawalo 0 i CUSTOM
               zapisany przez edytor kafelka wracal jako SOLID. */
            else if(sub==9u)user_syscall_result=(int32_t)kgfx_theme_set_wall((unsigned)(request->xy&7u));
            else if(sub==10u)user_syscall_result=(int32_t)kgfx_theme_wall();
            else user_syscall_result=-0x3e2;
            return 0;   /* without this the chain's trailing reset zeroes the reply */
        }
        else if(request->opcode==18u){
            /* Draw batches per session: tells a quiet desktop from a rejected one. */

            const user_process_t *owner=0;uint32_t n=request->wh;
            const gui_draw_t *list=(const gui_draw_t *)(uintptr_t)request->xy;
            for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            /* Any live GUI client may submit a draw list: with one owner only,
               a second application window could never be painted. Process and
               buffer validation below still guards the kernel. */
            if(!owner||!n||n>GUI_DRAW_MAX||
                (request->xy&1u)||!user_process_range(owner,list,n*sizeof(*list))){
                if(client_session<SESSION_COUNT)++kdbg_batch_rej[client_session];
                user_syscall_result=-0x3e0;return 0;}
            if(client_session<SESSION_COUNT){++kdbg_batch_ok[client_session];kdbg_items_ok[client_session]+=n;}
            /* Caller remains blocked until the broker replies. Kernel code is
               not preempted by another user; no untrusted nested pointers. */
            {uint8_t *surface=kernel_wm_app_surface(argument0,(uint16_t)request->color);
             if(surface){uint8_t *previous=kgfx_set_target(surface);
                 int32_t dl=0,dt=0,dr=0,db=0;
                 kgfx_draw_batch(list,n);(void)kgfx_set_target(previous);
                 /* The compositor only re-reads a surface whose window was
                    damaged; report the exact region the app repainted. */
                 if(kgfx_batch_bounds(&dl,&dt,&dr,&db))
                     kernel_wm_app_dirty(surface,dl,dt,dr,db);}
             else kgfx_draw_batch(list,n);}
            /* The first validated desktop canvas is ready. Compose it offscreen
               before releasing the boot frame; enrolment alone is too early
               (resources and the first draw can still take several frames). */
            if(kconsole_boot_active()&&argument0==gui_desktop_pids[client_session]&&
               client_session==ksessions_current()&&request->color==0u){
                kernel_wm_compose_now();
                kconsole_boot_screen(0);
            }
        }
        else if(request->opcode==36u){
            /* Register a dynamic per-app icon from PNG data: xy -> PNG bytes,
               wh -> byte size, color -> slot.  The kernel decodes + quantizes. */
            const user_process_t *owner=0;uint8_t *data;uint32_t size=request->wh;
            for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            data=(uint8_t *)(uintptr_t)request->xy;
            if(!owner||(request->xy&1u)||!size||size>16384u||
               !user_process_range(owner,data,size)){
                user_syscall_result=-0x3e0;return 0;}
            user_syscall_result=kgfx_icon_from_png((uint8_t)request->color,data,size);
        }
        else if(request->opcode==15u)kgfx_emutos_background(x,y,width,height);
        else if(request->opcode==17u)kgfx_emutos_controls(x,y,width,height,request->color);
        else if(request->opcode==16u)kgfx_emutos_window(x,y,width,height);
        else if(request->opcode==14u){char glyph[2];
            glyph[0]=(char)request->wh;glyph[1]='\0';
            kgfx_text(x,y,glyph,(uint8_t)(request->color>>24),
                      (uint8_t)(request->color>>16),(int)(request->color&1u));}
        else if(request->opcode==20u)kernel_wm_set_window(argument0,(uint16_t)request->color,
                                                          (int16_t)x,(int16_t)y,
                                                          (uint16_t)width,(uint16_t)height);
        else if(request->opcode==21u){
            const char *title=(const char *)(uintptr_t)request->xy;char buffer[24];uint8_t k;int ok=0;
            const user_process_t *owner=0;
            /* The syscall runs in SYSTEM.GFX, so user_string() would validate
               against the broker, not the app that owns the title.  Find the
               real client and validate each byte inside its image/stack. */
            for(i=0;i<USER_PROCESS_SLOTS;++i)if(process_table[i].pid==argument0&&
                process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED){owner=&process_table[i];break;}
            if(owner)for(k=0;k<23u;++k){
                if(!user_process_range(owner,&title[k],1u))break;
                buffer[k]=title[k];
                if(!title[k]){ok=1;break;}}
            if(ok)kernel_wm_set_title(argument0,(uint16_t)request->color,buffer);
            else user_syscall_result=-0x3e0;
        }
        else if(request->opcode==22u)kernel_wm_cycle();
        /* 24: launcher "Open terminal" - bring the shell console window back on
           the desktop (and focus it for the keyboard). */
        else if(request->opcode==24u){
            /* Jawne "OPEN TERMINAL" z pulpitu: ekran ma byc czysty, bez sladow
               bootowania (zgloszenie uzytkownika).  Czyscimy TYLKO tutaj -
               kernel_wm_console_show() jest wolane takze cyklicznie przez
               jadro i czyszczenie tam kasowaloby prace terminala w tle. */
            kconsole_clear();
            /* Czysty ekran musi od razu pokazac znak zachety, inaczej nie
               widac, gdzie pisac (shell rysuje go dopiero po ENTER). */
            {const kernel_shell_service_t *sh=(const kernel_shell_service_t *)kernel_find_service(KERNEL_SERVICE_SHELL,KERNEL_SHELL_VERSION);
             if(sh&&sh->redraw_prompt)sh->redraw_prompt();}
            kernel_wm_console_show();kernel_wm_console_focus();}
        /* 25: scripted-test hook - inject a synthetic mouse state. */
        else if(request->opcode==25u){extern void ksessions_mouse_inject(int32_t,int32_t,uint32_t);
            ksessions_mouse_inject((int16_t)(request->xy>>16),(int16_t)request->xy,(uint32_t)request->color);}
        /* 26: scripted-test hook - return the mouse to the real IKBD stream. */
        else if(request->opcode==26u){extern void ksessions_mouse_script(int);
            ksessions_mouse_script(0);}
        /* 28: test hook - run a fast synthetic drag from the kernel side. */
        else if(request->opcode==28u){extern uint32_t kdbg_autodrag;kdbg_autodrag=200u;}
        /* 29: client declares its document height (enables the scrollbar). */
        else if(request->opcode==29u)kernel_wm_set_content(argument0,(uint16_t)request->color,
                                                           (uint16_t)request->xy);
        /* 38: offset przewiniecia okna klienta (src_y).  Potrzebne, gdy klient
           rysuje cala liste w powierzchni, a przewija nia WM: bez tego kliknie
           w wiersz trafialby o src_y za nisko. */
        /* 45/46/47: EDYTOR KAFELKA.  UWAGA: numery 42/43/44 sa ZAREZERWOWANE
           przez protokol GFX (ATOS_CALL_GFX / GFX_INFO / GFX_RESTORE), wiec
           broker odrzucal je jako "nieznany opcode" (rc=-994) - dlatego edytor
           "nie dal sie rysowac" mimo poprawnego kodu.
           45: ustaw/zniszcz/przelacz piksel: xy=(x<<8)|y, color 0/1/2.
           46: narysuj siatke 16x16: xy=(x<<16)|y, wh=(w<<16)|h.
           47: dane kafelka: color 0=odczyt 32 B pod xy, 1=zapis. */
        else if(request->opcode==45u){
            int px=(int)((request->xy>>8)&0xffu),py=(int)(request->xy&0xffu);
            if((request->color&4u)){kgfx_tile_set_fg((unsigned)(request->xy&0xfffu));return 0;}
            /* 8 = odczyt bajtu kafelka (xy = indeks 0..31), 16 = zapis bajtu
               (xy = (indeks<<8)|wartosc).  Bez wskaznikow - to omija problem
               z przekazywaniem bufora przez brokera (zapis TILE dawal A5A5). */
            if((request->color&8u)){user_syscall_result=(int32_t)kgfx_tile_bytes()[(unsigned)request->xy&31u];return 0;}
            if((request->color&16u)){
                extern void kgfx_tile_put_byte(unsigned,unsigned);
                kgfx_tile_put_byte(((unsigned)request->xy>>8)&31u,(unsigned)request->xy&0xffu);return 0;}
            if((request->color&3u)==2u)kgfx_tile_set(px,py,!kgfx_tile_get(px,py)); /* 2 = przelacz */
            else kgfx_tile_set(px,py,(int)(request->color&1u));
        }
        else if(request->opcode==46u){
            /* 43 z bitem 0x8000 w color = PRZELACZ PIKSEL (x<<8|y), bez bitu =
               rysuj siatke.  Opcode 42 jest odrzucany przez brokera jako
               nieznany (rc=-994), a 43 dziala - dlatego tryb edycji jedzie
               tym samym, sprawdzonym opcodem. */
            if(request->color&0x8000u){
                int px=(int)((request->xy>>8)&0xffu),py=(int)(request->xy&0xffu);
                if((request->color&3u)==2u)kgfx_tile_set(px,py,!kgfx_tile_get(px,py));
                else kgfx_tile_set(px,py,(int)(request->color&1u));
            }else
                kgfx_tile_grid((int16_t)(request->xy>>16),(int16_t)request->xy,
                               (int16_t)(request->wh>>16)/16,(int16_t)request->wh/16);
        }
        else if(request->opcode==47u){
            uint8_t *buf=(uint8_t *)(uintptr_t)request->xy;
            if(!buf){user_syscall_result=-0x3e0;return 0;}
            if(request->color&1u)kgfx_tile_from_bytes(buf);
            else{const uint8_t *src=kgfx_tile_bytes();unsigned k;for(k=0;k<32u;++k)buf[k]=src[k];}
        }
        /* 41: wypelnienie prostokata WZOREM (tapeta pulpitu) - jedna operacja
           na wiersz zamiast setek prostokatow w liscie rysowania.
           a=wzor, b=(x<<16)|y, c=(w<<16)|h. */
        else if(request->opcode==41u)
            kgfx_pattern_fill((int16_t)(request->xy>>16),(int16_t)request->xy,
                              (int16_t)(request->wh>>16),(int16_t)request->wh,
                              (unsigned)request->color);
        else if(request->opcode==38u)
            user_syscall_result=(int32_t)kernel_wm_get_scroll(argument0,(uint16_t)request->color);
        /* 48/49/50: ROZMIAR IKON I SIATKA PULPITU.
           48 = ustaw rozmiar (xy = 32/48/64; przeladowuje ICONS.DAT),
           49 = odczytaj (zwraca (px<<8)|tryb_siatki) - JEDEN call na tick,
           50 = ustaw tryb siatki (xy = 0 COMPACT / 1 NORMAL / 2 WIDE).
           Wybor zapisujemy w \SYSTEM\ICONSZ.CFG (rozmiar + siatka), wiec nastepny
           start rozwija wlasciwa sekcje od razu. */
        else if(request->opcode==48u){
            unsigned px=(unsigned)(request->xy&0xffffu);
            int32_t rc;
            if(px!=32u&&px!=48u&&px!=64u){user_syscall_result=-0x3e0;return 0;}
            ++kdbg_icon_set_calls;
            if(px==(unsigned)kgfx_icons_size()){user_syscall_result=0;return 0;}
            rc=icons_size_set(px);
            if(rc<0){user_syscall_result=rc;return 0;}
            icons_cfg_save();
            user_syscall_result=0;return 0;
        }
        else if(request->opcode==49u){
            user_syscall_result=(int32_t)(((uint32_t)kgfx_icons_size()<<8)|(uint32_t)kgfx_icon_grid());
            ++kdbg_icon_q_calls;kdbg_icon_q_last=(uint32_t)user_syscall_result;
            return 0;   /* bez tego lancuch zeruje odpowiedz (jak przy op 37) */
        }
        else if(request->opcode==50u){
            unsigned mode=(unsigned)(request->xy&0xffu);
            if(mode>2u){user_syscall_result=-0x3e0;return 0;}
            kgfx_icon_grid_set((uint8_t)mode);
            icons_cfg_save();
            user_syscall_result=0;return 0;
        }
        /* 30: video mode.  a=0 queries -> (width<<16)|height; otherwise it asks
           for a mode.  On the ET4000 path the adapter and the whole compositor
           are reprogrammed live (background repaint included); the ST Shifter
           has exactly one mono mode, so there any other size is refused. */
        else if(request->opcode==30u){
            const kgfx_info_t *g=kgfx_get_info();
            uint16_t rw=(uint16_t)(request->xy>>16),rh=(uint16_t)request->xy;
            if(!rw&&!rh){user_syscall_result=(int32_t)(((uint32_t)g->width<<16)|(uint16_t)g->height);return 0;}
            if(rw==g->width&&rh==g->height){user_syscall_result=0;return 0;}
            {int32_t r=kgfx_video_reconfigure(rw,rh);
             user_syscall_result=r<0?r:0;return 0;}
        }
        /* 34: switch to an 8 bits-per-pixel (256 colours, chain-4) mode.  The
           geometry comes from xy (width<<16)|height, defaulting to 800x600 so
           existing callers that passed 0 keep working.  Kept apart from opcode
           30 because both depths share the same geometry. */
        else if(request->opcode==34u){
            uint16_t rw=(uint16_t)(request->xy>>16),rh=(uint16_t)request->xy;
            if(!rw)rw=800u;
            if(!rh)rh=600u;
            {int32_t r=kgfx_video_reconfigure_format(rw,rh,8u);
             user_syscall_result=r<0?r:0;return 0;}
        }
        /* 36 remains PNG icons; 40 is the explicit display-mode contract. */
        else if(request->opcode==ATOS_GFX_DISPLAY_SET){
            uint16_t rw=(uint16_t)(request->xy>>16),rh=(uint16_t)request->xy;
            uint8_t bpp=(uint8_t)request->wh;
            const kgfx_info_t *g=kgfx_get_info();
            uint16_t ow=g->width,oh=g->height;uint8_t ob=kvideo_mode_bpp();
            int32_t r;
            if(kvideo_guard_active()||kgame_owns_screen()){
                user_syscall_result=-0x3e5;return 0;}
            r=kgfx_video_reconfigure_format(rw,rh,bpp);
            if(r>=0&&(request->wh&ATOS_DISPLAY_PREVIEW))
                /* Zegar z PRZERWANIA Timer-C, nie z pollingu: straznik musi
                   wygasnac po 4 s TAKZE wtedy, gdy tryb-smieci zwalnia petle. */
                kvideo_guard_watch(ow,oh,ob,kernel_user_wall_clock(),argument0);
            user_syscall_result=r<0?r:0;return 0;
        }
        /* 39: confirm / cancel / query a preview owned by this client. */
        else if(request->opcode==ATOS_GFX_DISPLAY_GUARD){
            user_syscall_result=kvideo_guard_control(argument0,request->xy,kernel_user_wall_clock());
            return 0;
        }
        /* 35: query the ACTIVE depth.  Bylo (g->mode==8)?8:1, czyli tryby 4 i 16
           raportowaly 1 (mono) - dlatego "glebia gineła" po drodze.  Teraz
           pytamy warstwe wideo, ktora zna format KARTY (1/4/8/16). */
        else if(request->opcode==35u){
            const kgfx_info_t *g=kgfx_get_info();
            uint8_t cb=kvideo_mode_bpp();
            user_syscall_result=cb?(int32_t)cb:((g->mode==8u)?8:1);
            return 0;
        }
        /* 31: menu pop-up overlay.  Declares the desktop pull-down rectangle so
           the kernel keeps it above application windows (they may sit at y=16,
           which is exactly where the list starts). */
        else if(request->opcode==31u)
            kernel_wm_overlay(x,y,width,height,(int)(request->color&1u));
        /* 32: wake the window owned by the pid in 'color' (un-minimise, raise,
           focus).  The desktop launcher uses it so clicking a running app's
           button restores it instead of starting a second copy. */
        else if(request->opcode==32u){
            if(kernel_wm_activate_pid(request->color)){user_syscall_result=0;return 0;}
            user_syscall_result=-0x3e5;return 0;}
        /* 33: window state of a pid: 0 = none, 1 = visible, 2 = minimised.
           Drives the launcher button colours (white/black/grey). */
        else if(request->opcode==33u){
            user_syscall_result=(int32_t)kernel_wm_window_state(request->color);return 0;}
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
        {unsigned slot=kernel_user_pid_terminal(argument0);
         if(gui_keyboard_pids[slot]&&argument0!=gui_keyboard_pids[slot]){
             user_syscall_result=-0x3e5;return 0;}
         if(slot==ksessions_current())kgfx_restore_console();
         gui_desktop_pids[slot]=0;}
        user_syscall_result=0;return 0;}
    if(call==APP_ABI_SYSCALL_VERSION){user_syscall_result=(int32_t)APP_ABI_VERSION;return 0;}
    if(call==45u||call==47u){unsigned slot;
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")||!argument0){
            user_syscall_result=-0x400;return 0;}
        for(i=0;i<USER_PROCESS_SLOTS;i++)if(process_table[i].pid==argument0&&
           process_table[i].state!=USER_PROCESS_FREE&&process_table[i].state!=USER_PROCESS_TERMINATED)break;
        if(i==USER_PROCESS_SLOTS){user_syscall_result=-0x401;return 0;}
        slot=process_table[i].terminal;
        if(!user_gui_terminal_allowed(slot,argument0)||gui_keyboard_pids[slot]){
            user_syscall_result=-0x400;return 0;}
        {int32_t now=kernel_user_yield();
         if(now<0){user_syscall_result=-0x402;return 0;}
         gui_keyboard_pids[slot]=argument0;gui_ready_ticks[slot]=(uint32_t)now+5u;
         gui_dropped[slot]=0;gui_persistent[slot]=call==47u;
         if(slot==ksessions_current()&&call==47u)kernel_user_mouse_flush();}
        user_syscall_result=0;return 0;}
    if(call==46u||call==48u){
        if(current_process->pid!=kernel_user_service_pid("SYSTEM.GUI")){
            user_syscall_result=-0x400;return 0;}
        if(argument0){for(i=0;i<SESSION_COUNT;i++)user_gui_release_session(i,argument0);}
        else user_gui_release_session(ksessions_current(),0);
        user_syscall_result=0;return 0;}
    user_syscall_result=-0x2ff;
    return 0;
}
void kernel_exception_dispatch(uint32_t vector)
{
    kconsole_emergency();
    if(services&&services->console_clear&&services->console_puts){services->console_clear();services->console_puts("ATARI SYSTEM KERNEL PANIC\n\nFATAL CPU EXCEPTION: ");
        if(vector==2u)services->console_puts("BUS ERROR");else if(vector==3u)services->console_puts("ADDRESS ERROR");else if(vector==4u)services->console_puts("ILLEGAL INSTRUCTION");else if(vector==5u)services->console_puts("DIVIDE BY ZERO");else services->console_puts("UNKNOWN");services->console_puts("\n");if(services->console_put_u32)kernel_fault_report(services->console_puts,services->console_put_u32);services->console_puts("\nSYSTEM HALTED SAFELY\nPRESS RESET");}
}
