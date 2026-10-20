#ifndef TOS_ASYNC_H
#define TOS_ASYNC_H
#include <stdint.h>
/* One cooperative TOS domain; native ATOS processes remain independently scheduled. */
#define TOS_ASYNC_PID 0x7fff0001UL
int32_t kernel_tos_start_async(const char *path);
void kernel_tos_async_pump(void);
int kernel_tos_async_owns(uint32_t pid);
unsigned kernel_tos_async_session(void);
int kernel_tos_async_mouse(uint32_t buttons,int32_t x,int32_t y);
int kernel_tos_async_keyboard(void);
#endif
