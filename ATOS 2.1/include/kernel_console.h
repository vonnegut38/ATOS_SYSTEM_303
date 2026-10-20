#ifndef ASF_KERNEL_CONSOLE_H
#define ASF_KERNEL_CONSOLE_H
#include <stdint.h>
/* Runtime console. Early boot and panic retain the original ROM callbacks. */
void kconsole_init(int mono,void (*clear)(void),void (*puts)(const char *),
                   void (*number)(uint32_t));
void kconsole_clear(void);
unsigned kconsole_cursor_row(void);
void kconsole_puts(const char *);
void kconsole_put_u32(uint32_t);
void kconsole_flush(void);
/* Pin the output to one row: further kconsole_puts() calls overwrite it in
   place instead of scrolling (row < 0 restores normal appending). */
void kconsole_status_row(int row);
/* Same, but pinning the row the log cursor is on right now. */
void kconsole_status_here(void);
/* Quiet boot: verbose kernel lines are captured but not drawn; the system
   table uses the notice_* calls, so the screen never scrolls. */
void kconsole_quiet(int on);
/* Retain the graphical boot screen until the desktop owns presentation.
   Explicit notices and exception output remain visible. */
void kconsole_boot_screen(int on);
int kconsole_boot_active(void);
void kconsole_notice_puts(const char *s);
void kconsole_notice_u32(uint32_t v);
void kconsole_invalidate(void);
void kconsole_emergency(void);
void kconsole_select(unsigned,int);
void kconsole_puts_to(unsigned,const char *);
/* Cell grid of a session's console, for the compositor window. */
const void *kconsole_cells(unsigned session);
/* Set the console grid size (columns wrap the output); the screen is still
   80x25 physically, so cols <= 80 and rows <= 25. */
void kconsole_set_grid(unsigned cols,unsigned rows);
/* Adopt a character mirror (80x25, row-major) and cursor from the ROM console. */
void kconsole_seed(const char *cells,unsigned column,unsigned row);
/* Command-line output capture: while active, every kconsole_puts() also appends
   to the caller's buffer (used by the GUI command line in commander.c). */
void kconsole_capture_begin(char *buffer,uint32_t capacity);
void kconsole_capture_end(void);
int kconsole_capture_state(void);   /* byte count, or -1 while still capturing */
/* Diagnostics: report the state kconsole_flush() sees (256-colour boot).
   The graphics/WM state is passed in so this file stays free of those deps. */
typedef struct {uint32_t mode,planes,desktop_active,wm_enabled,wm_session,
                framebuffer,scanout;} kconsole_debug_t;
void kconsole_debug_state(void (*outs)(const char *),void (*outn)(uint32_t),
                          const kconsole_debug_t *d);
#endif
