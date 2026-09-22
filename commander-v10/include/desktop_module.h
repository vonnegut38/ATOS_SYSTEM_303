#ifndef ASF_DESKTOP_MODULE_H
#define ASF_DESKTOP_MODULE_H
#include <stdint.h>
#include "video_driver.h"
/* Desktop appearance/applet modules (ATOS v10, docs/DESKTOP-ARCHITECTURE.md).
 * The desktop loads modules at startup from a directory and drives them through
 * this small ABI.  A module NEVER touches hardware - it draws into a
 * video_surface_t and receives ticks/events.  Host-tested in
 * tests/test_desktop_module.c. */
typedef struct desktop_module {
    const char *name;
    uint16_t    version;
    int  (*init)(void *config);
    void (*draw)(video_surface_t *surface,int x,int y,int w,int h);
    void (*tick)(uint32_t now_ticks);
    int  (*event)(int type,int a,int b);
    void (*shutdown)(void);
} desktop_module_t;

void desktop_module_register(desktop_module_t *module);
desktop_module_t *desktop_module_find(const char *name);
int  desktop_module_count(void);
int  desktop_module_init_all(void *config);     /* returns how many initialised */
void desktop_module_tick_all(uint32_t now_ticks);
void desktop_module_draw_all(video_surface_t *surface,int x,int y,int w,int h);
void desktop_module_reset(void);
const char *desktop_module_name(int index);
#endif
