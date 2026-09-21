#ifndef ASF_KERNEL_INPUT_H
#define ASF_KERNEL_INPUT_H
#include <stdint.h>

#define KERNEL_INPUT_MAGIC 0x494e5054UL
#define KERNEL_INPUT_VERSION 1u
#define KERNEL_INPUT_KEY_DOWN 1u
#define KERNEL_INPUT_KEY_UP 2u
#define KERNEL_INPUT_MOUSE_PACKET 3u
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    int (*poll)(uint32_t target_task);
    uint32_t (*classify)(uint32_t raw_byte);
    uint32_t (*dropped_events)(void);
} kernel_input_service_t;

#endif
