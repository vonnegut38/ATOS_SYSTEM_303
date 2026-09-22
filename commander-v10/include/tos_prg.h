#ifndef ASF_TOS_PRG_H
#define ASF_TOS_PRG_H
#include <stdint.h>

/* TOS personality (T1): run a 0x601A .PRG under the ASF kernel with the TOS
 * trap vectors (GEMDOS/BIOS/XBIOS/AES-VDI) routed to our handlers. */
int32_t kernel_tos_run_program(const char *path);
void kernel_tos_set_console(void (*puts)(const char *));
void kernel_tos_set_cpu(uint8_t cpu_type);
void kernel_tos_set_input(int32_t (*pop)(void));
void kernel_tos_set_console_clear(void (*clear)(void));
void kernel_tos_set_video(void (*enter)(void),void (*leave)(void),void (*clear)(uint8_t),
                          void (*fill)(int,int,int,int,uint8_t),
                          void (*text)(int,int,const char *,uint8_t,uint8_t),
                          void (*blit_mono)(int,int,int,int,const uint8_t *,int,uint8_t,uint8_t));

#endif
