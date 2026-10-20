/* Glue between the C runtime (libc.c) and the C++ side of the port. */
#ifndef ATOS_PORT_H
#define ATOS_PORT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int atos_main(void);                 /* main_atos.cpp */
int atos_loading_active(void);
void atos_loading(const char *stage, unsigned progress); /* startup stages, no waits */
void atos_port_shutdown(void);       /* gives the screen/keyboard back; idempotent */
void atos_log_to(const char *path);  /* console log copy, saved at exit */
void atos_log_flush(void);
void atos_path(char *dst,size_t cap,const char *src);
uint32_t atos_heap_size(void);
uint32_t atos_heap_peak(void);
#ifdef __cplusplus
}
#endif
#endif
