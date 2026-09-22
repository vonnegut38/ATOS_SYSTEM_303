#ifndef ASF_KERNEL_TIMER_H
#define ASF_KERNEL_TIMER_H
#include <stdint.h>

#define KERNEL_TIMER_MAGIC 0x54494d52UL
#define KERNEL_TIMER_VERSION 1u
#define KERNEL_TIMER_CAP_POLLING 1UL
#define KERNEL_TIMER_CAP_DEFERRED_SWITCH 2UL
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    uint32_t (*poll)(void);
    uint32_t (*ticks)(void);
    int (*switch_pending)(void);
    void (*acknowledge_switch)(void);
    uint32_t frequency_hz;
    uint32_t capabilities;
} kernel_timer_service_t;

#endif
