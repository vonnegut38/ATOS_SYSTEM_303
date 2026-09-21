#ifndef ASF_KERNEL_SHELL_H
#define ASF_KERNEL_SHELL_H
#include <stdint.h>

#define KERNEL_SHELL_MAGIC 0x53484c4cUL
#define KERNEL_SHELL_VERSION 3u
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    void (*start)(void);
    int (*pump)(uint32_t task);
    uint32_t (*key_count)(void);
    void (*redraw_prompt)(void);
    int (*line_active)(void);
    int (*select_session)(uint32_t slot,uint32_t reset);
} kernel_shell_service_t;

#endif
