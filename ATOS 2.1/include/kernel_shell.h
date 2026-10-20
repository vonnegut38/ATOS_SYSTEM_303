#ifndef ASF_KERNEL_SHELL_H
#define ASF_KERNEL_SHELL_H
#include <stdint.h>

#define KERNEL_SHELL_MAGIC 0x53484c4cUL
#define KERNEL_SHELL_VERSION 4u
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    void (*start)(void);
    int (*pump)(uint32_t task);
    uint32_t (*key_count)(void);
    void (*redraw_prompt)(void);
    int (*line_active)(void);
    int (*select_session)(uint32_t slot,uint32_t reset);
    /* v4: the GUI command line. submit() only queues the text; the kernel main
       loop runs it through run_pending() in the shell's own context, so a
       command that spawns or waits never re-enters the caller's syscall. */
    int (*submit)(const char *line);
    int (*run_pending)(void);
} kernel_shell_service_t;

#endif
