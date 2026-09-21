#ifndef ASF_KERNEL_PROC_H
#define ASF_KERNEL_PROC_H
#include <stdint.h>

#define KERNEL_PROC_MAGIC 0x50524f43UL
#define KERNEL_PROC_VERSION 5u
#define KERNEL_PROC_READY 0u
#define KERNEL_PROC_SLEEPING 1u
#define KERNEL_PROC_WAITING 2u
#define KERNEL_PROC_CAP_COOPERATIVE 1UL
#define KERNEL_PROC_CAP_68K_CONTEXT 2UL
#define KERNEL_PROC_CAP_CPU_EXTENSION 4UL
#define KERNEL_PROC_CAP_FULL_CONTEXT 8UL
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    uint32_t (*task_count)(void);
    int (*cooperate)(void);
    uint32_t (*dispatch_count)(uint32_t task);
    uint32_t capabilities;
    int (*sleep_ticks)(uint32_t task,uint32_t ticks);
    void (*timer_tick)(void);
    uint32_t (*sleep_remaining)(uint32_t task);
    int (*wait_event)(uint32_t task);
    int (*wake_task)(uint32_t task);
    uint32_t (*task_state)(uint32_t task);
    int (*context_self_test)(void);
} kernel_proc_service_t;

#endif
