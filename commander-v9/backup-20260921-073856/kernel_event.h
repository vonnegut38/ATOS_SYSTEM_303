#ifndef ASF_KERNEL_EVENT_H
#define ASF_KERNEL_EVENT_H
#include <stdint.h>

#define KERNEL_EVENT_MAGIC 0x45564e54UL
#define KERNEL_EVENT_VERSION 1u
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    int (*receive)(uint32_t task,uint32_t *type,uint32_t *value);
    int (*send)(uint32_t task,uint32_t type,uint32_t value);
    int (*pending)(uint32_t task);
} kernel_event_service_t;

#endif
