#ifndef ASF_KERNEL_CONSOLE_H
#define ASF_KERNEL_CONSOLE_H
#include <stdint.h>
/* Runtime console. Early boot and panic retain the original ROM callbacks. */
void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                   void (*number)(uint32_t));
void kconsole_clear(void);
void kconsole_puts(const char *);
void kconsole_put_u32(uint32_t);
void kconsole_flush(void);
void kconsole_invalidate(void);
void kconsole_emergency(void);
void kconsole_select(unsigned,int);
void kconsole_puts_to(unsigned,const char *);
/* Cell grid of a session's console, for the compositor window. */
const void *kconsole_cells(unsigned session);
/* Set the console grid size (columns wrap the output); the screen is still
   80x25 physically, so cols <= 80 and rows <= 25. */
void kconsole_set_grid(unsigned cols,unsigned rows);
#endif
