#ifndef KERNEL_SESSIONS_H
#define KERNEL_SESSIONS_H
#include <stdint.h>
#include "kernel_shell.h"
#define SESSION_COUNT 3u
unsigned ksessions_current(void);
unsigned ksessions_mask(void);
int32_t ksessions_control(uint32_t op,uint32_t value);
int ksessions_key(uint32_t type,uint32_t scan);
void ksessions_init(const kernel_shell_service_t *shell,int enabled);
void ksessions_pump(void);
void ksessions_unpaint(void);
void ksessions_paint(void);
int ksessions_mouse_event(uint32_t *,int32_t *,int32_t *);
void ksessions_mouse_flush(void);
int ksessions_mouse_x(void);
int ksessions_mouse_y(void);
#endif
