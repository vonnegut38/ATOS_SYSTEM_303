#ifndef ASF_KERNEL_MODULE_H
#define ASF_KERNEL_MODULE_H
#include <stdint.h>
#include "boot_protocol.h"
#include "kernel_memory.h"
#include "kernel_fat.h"

#define KERNEL_MODULE_API_MAGIC 0x4b4d4150UL
#define KERNEL_SERVICE_CORE 0x434f5245UL
#define KERNEL_SERVICE_VFS 0x56465320UL
#define KERNEL_SERVICE_PROC 0x50524f43UL
#define KERNEL_SERVICE_TIMER 0x54494d52UL
#define KERNEL_SERVICE_EVENT 0x45564e54UL
#define KERNEL_SERVICE_INPUT 0x494e5054UL
#define KERNEL_SERVICE_SHELL 0x53484c4cUL
#define KERNEL_SERVICE_VIDEO 0x56494445UL /* "VIDE", optional VOFA.SYS */
/* TOS compatibility: the personality's working set lives in TOS.SYS, loaded on
   demand.  TOS_CFG is published by the kernel before loading it so the module
   learns whether its buffers belong in TT RAM; TOS is what the module returns. */
#define KERNEL_SERVICE_TOS 0x544f5320UL      /* "TOS " */
#define KERNEL_SERVICE_TOS_CFG 0x544f5343UL  /* "TOSC" */
typedef struct {
    uint8_t cpu_type,fpu_type,machine_type,address_bits;
    uint8_t has_mmu,has_instruction_cache,has_data_cache,clock_known;
    uint8_t clock_mhz,reserved[3];
} kernel_cpu_info_t;
typedef struct {
    uint32_t magic;
    uint16_t abi_version,size;
    void *(*alloc)(uint32_t,uint32_t,uint16_t,uint16_t,int);
    int (*free)(void *,uint16_t);
    int (*bios_acsi_read)(uint8_t,uint32_t,void *);
    int (*register_acsi_read_blocks)(int (*)(uint8_t,uint32_t,uint16_t,void *));
    int (*register_service)(uint32_t,uint16_t,const void *);
    const void *(*find_service)(uint32_t,uint16_t);
    int (*system_file_size)(const char name[11],uint32_t *size);
    int (*load_system_file)(const char name[11],void *destination,
                            uint32_t capacity,uint32_t *size);
    void (*console_clear)(void);
    void (*console_puts)(const char *text);
    void (*console_put_u32)(uint32_t value);
    void (*get_st_stats)(kmem_stats_t *stats);
    void (*get_tt_stats)(kmem_stats_t *stats);
    uint32_t (*boot_drive_letter)(void);
    int (*stat_path)(const char *path,uint32_t *size,uint8_t *attributes);
    int (*read_path)(const char *path,void *destination,
                     uint32_t capacity,uint32_t *size);
    int (*list_path)(const char *path,kfat_list_callback_t callback,void *context);
    int (*read_path_at)(const char *path,uint32_t offset,void *destination,
                        uint32_t capacity,uint32_t *size);
    void (*sync_instruction_cache)(void);
    int (*select_drive)(uint32_t letter);
    int32_t (*run_user)(void *entry,void *stack_top,
                        const void *image_base,uint32_t image_size,
                        const char *working_directory);
    void (*set_user_arguments)(const char *arguments);
    int32_t (*spawn_user_async)(const char *path,const char *arguments,
                                const char *working_directory,uint32_t *pid);
    int (*user_process_info)(uint32_t slot,uint32_t info[4]);
    int (*user_process_name)(uint32_t slot,char name[13]);
    int (*kill_user_process)(uint32_t pid);
    int (*user_process_resources)(uint32_t slot,uint32_t info[4]);
    int (*foreground_user_process)(uint32_t pid);
    int (*resume_user_process)(uint32_t pid);
    int (*suspend_user_process)(uint32_t pid);
    int (*take_user_completion)(uint32_t pid,int32_t *status);
    uint32_t (*user_process_cpu_ticks)(uint32_t slot);
    int (*set_user_watchdog)(uint32_t pid,uint32_t ticks);
    int (*get_cpu_info)(kernel_cpu_info_t *info);
    uint32_t (*user_process_stack_used)(uint32_t slot);
    int32_t (*ikbd_raw_pop)(void);
    uint32_t (*ikbd_raw_dropped)(void);
    int (*user_process_map)(uint32_t pid,uint32_t info[16]);
    int (*get_exec_path)(uint32_t index,char destination[64]);
    int32_t (*terminal_control)(uint32_t,uint32_t);
    int (*terminal_key)(uint32_t,uint32_t);
    /* Optional v3 tail, size >= 176; supervisor polling, no parsing. */
    void (*ikbd_capture)(void);
    /* Optional v4 tail: read-only floppy (FAT12) via the ROM reader. */
    int (*floppy_mount)(void);
    int (*floppy_dir)(uint32_t index,char name[13],uint32_t *size,uint8_t *attr);
    int (*floppy_read_file)(const char *name,void *buffer,uint32_t capacity,uint32_t *size);
    /* Optional v5 tail: run a TOS .PRG/.TOS through the TOS personality.  The
       shell/Commander use it so a TOS program is an ordinary command. */
    int32_t (*tos_run)(const char *path);
    /* Optional v6 tail: filesystem mutations for shell_fs.  All four return 0
       on success or a negative FAT_WRITE_* code. */
    int (*copy_path)(const char *source,const char *target);
    int (*mkdir_path)(const char *path);
    int (*delete_path)(const char *path);
    int (*move_path)(const char *source,const char *target);
} kernel_module_api_t;
enum module_state {MODULE_NOT_FOUND,MODULE_INVALID,MODULE_NO_MEMORY,MODULE_INIT_FAILED,MODULE_IO_FAILED,MODULE_READY};
enum module_state kernel_load_acsi(const boot_handoff_t *handoff);
enum module_state kernel_load_system_module(const char name[11],uint16_t owner,
                                            int prefer_tt);
/* Drivers live in \SYSTEM\DRV (graphics, and any future hardware family). */
enum module_state kernel_load_driver_module(const char name[11],uint16_t owner,
                                            int prefer_tt);
int kernel_acsi_read_blocks(uint8_t id,uint32_t sector,uint16_t count,void *buffer);
/* Write-through the ROM's device driver when the kernel has no native path
   (TF536 IDE). Returns 0 when no write backend is available. */
int kernel_acsi_write_blocks(uint8_t id,uint32_t sector,uint16_t count,const void *buffer);
int kernel_verify_module_image(const void *image,uint32_t size);
const void *kernel_find_service(uint32_t id,uint16_t minimum_version);
/* TOS compatibility module (TOS.SYS): loaded on demand when a TOS program runs,
   or eagerly after START when prefer_tt=1 (the SETTINGS option).  Returns 1 or a
   NEGATIVE code (-100..-105 module load, -110..-123 interface errors). */
int32_t kernel_tos_compat_ensure(int prefer_tt);
int kernel_tos_compat_loaded(void);
const void *kernel_tos_compat_service(void);
/* The private TOS screen owned by the module, or 0 when it was not adopted. */
uint8_t *kernel_tos_compat_screen(void);
/* Publish a kernel-owned interface without a module load window. */
int kernel_publish_service(uint32_t id,uint16_t version,const void *interface);
/* Diagnostics for the module scenario. */
extern uint32_t kdbg_tos_compat_loads;
extern uint32_t kdbg_tos_screen_fallbacks;
extern uint8_t *kdbg_tos_module_screen;
extern uint32_t kdbg_tos_adopted_module;
extern uint8_t kernel_tos_cfg_at_start;
extern uint8_t kernel_tos_cfg_prefer_tt;

#endif
