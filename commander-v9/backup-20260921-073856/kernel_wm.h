#ifndef KERNEL_WM_H
#define KERNEL_WM_H
#include <stdint.h>
/* Kernel glue between the compositor core (wm_core/wm_blit) and the live
   desktop framebuffer. */
void kernel_wm_init(void);
int  kernel_wm_self_test(void);
void kernel_wm_frame(void);
/* Composite at most once per tick: damage only accumulates between frames. */
void kernel_wm_frame_tick(uint32_t now);
int  kernel_wm_enabled(void);
/* Render the active session's console grid and compose it as a window. */
void kernel_wm_console_present(void);
/* True when the compositor owns the console as a visible window: the shell
   then has priority for the keyboard (it is the focused window). */
int  kernel_wm_console_visible(void);
void kernel_wm_console_hide(void);
int  kernel_wm_desktop_session(void);
void kernel_wm_set_desktop_session(int);
void kernel_wm_console_show(void);
/* Compositor counters: composites, damaged pixels and console cells rendered. */
void kernel_wm_stats(uint32_t *frames,uint32_t *pixels,uint32_t *cells);
/* Give the application its own canvas surface and register a window for it. */
uint8_t *kernel_wm_app_surface(uint32_t pid);
/* Force a full-desktop repaint (used once when the desktop takes over). */
void kernel_wm_damage_all(void);
#endif
