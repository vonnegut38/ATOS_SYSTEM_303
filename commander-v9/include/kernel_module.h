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
} kernel_module_api_t;
enum module_state {MODULE_NOT_FOUND,MODULE_INVALID,MODULE_NO_MEMORY,MODULE_INIT_FAILED,MODULE_IO_FAILED,MODULE_READY};
enum module_state kernel_load_acsi(const boot_handoff_t *handoff);
enum module_state kernel_load_system_module(const char name[11],uint16_t owner,
                                            int prefer_tt);
int kernel_acsi_read_sector(uint8_t id,uint32_t sector,void *buffer);
int kernel_acsi_read_blocks(uint8_t id,uint32_t sector,uint16_t count,void *buffer);
int kernel_verify_module_image(const void *image,uint32_t size);
const void *kernel_find_service(uint32_t id,uint16_t minimum_version);

#endif
