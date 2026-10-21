#ifndef KERNEL_WM_H
#define KERNEL_WM_H
#include <stdint.h>
/* Kernel glue between the compositor core (wm_core/wm_blit) and the live
   desktop framebuffer. */
void kernel_wm_init(void);
int  kernel_wm_self_test(void);
/* Opt-in (CONFIG_WM): bring the compositor up on the live framebuffer without
   the self-test's pixel writes.  Idempotent; 0 when no framebuffer is ready. */
int  kernel_wm_enable(void);
void kernel_wm_frame(void);
/* Composite at most once per tick: damage only accumulates between frames. */
void kernel_wm_frame_tick(uint32_t now);
int  kernel_wm_enabled(void);
/* Render the active session's console grid and compose it as a window. */
void kernel_wm_console_present(void);
/* True when the compositor owns the console as a visible window: the shell
   then has priority for the keyboard (it is the focused window). */
int  kernel_wm_console_visible(void);
/* Keyboard focus: shell window (1) or desktop application (0). */
int  kernel_wm_console_focused(void);
void kernel_wm_console_hide(void);
int  kernel_wm_desktop_session(void);
void kernel_wm_set_desktop_session(int);
void kernel_wm_console_show(void);
/* Route the keyboard to the shell window (launcher "Open terminal"). */
void kernel_wm_console_focus(void);
/* Compositor counters: composites, damaged pixels and console cells rendered. */
void kernel_wm_stats(uint32_t *frames,uint32_t *pixels,uint32_t *cells);
/* Requests-per-frame: total serviced, the last composed frame, and the peak. */
void kernel_wm_request_stats(uint32_t *total,uint32_t *last,uint32_t *max);
/* Give the application its own canvas surface and register a window for it. */
uint8_t *kernel_wm_app_surface(uint32_t pid,uint16_t handle);
/* Release the surface (closes the window and frees the backing slot). */
void kernel_wm_release_surface(uint32_t pid,uint16_t handle);
/* Compose the WM immediately (no desktop-session gate, no damage gate). */
void kernel_wm_compose_now(void);
/* Adopt a new geometry for an already-registered window. */
void kernel_wm_resize_window(uint32_t pid,uint16_t handle,int16_t x,int16_t y,uint16_t w,uint16_t h);
/* GFX opcode 20: give this process a real decorated window at (x,y,w,h). */
void kernel_wm_set_window(uint32_t pid,uint16_t handle,int16_t x,int16_t y,uint16_t w,uint16_t h);
/* GFX opcode 21: window title shown in the top-bar task list. */
void kernel_wm_set_title(uint32_t pid,uint16_t handle,const char *title);
int kernel_wm_dragging(void);
void kernel_wm_set_content(uint32_t pid,uint16_t handle,uint16_t content_h);
uint16_t kernel_wm_get_scroll(uint32_t pid,uint16_t handle);
/* Keyboard task switch: bring the next window to the front. */
void kernel_wm_cycle(void);
/* Force a full-desktop repaint (used once when the desktop takes over). */
void kernel_wm_damage_all(void);
/* Adopt a new video mode: surface geometry, shell window cap, full repaint. */
int kernel_wm_prepare_mode(uint16_t width,uint16_t height,uint8_t bpp);
void kernel_wm_reconfigure(void);
/* Keep the desktop pull-down list above application windows (GFX opcode 31).
   on=1 declares the rectangle, on=0 clears it. */
void kernel_wm_overlay(int16_t x,int16_t y,uint16_t w,uint16_t h,int on);
/* Repaint only the top-bar strip (task icon + session badge). */
void kernel_wm_refresh_bar(void);
/* An application repainted part of its own surface: damage that region so the
   compositor re-reads it.  Without this an app window only refreshed when some
   unrelated damage happened to recompose the desktop. */
int kernel_wm_client_origin(uint8_t *surface,int32_t *x,int32_t *y);
void kernel_wm_app_dirty(uint8_t *surface,int32_t l,int32_t t,int32_t r,int32_t b);
/* Client pid owning the topmost window under a screen point (0 = the desktop
   canvas/task bar/console).  Drives per-event input routing (focus manager). */
uint32_t kernel_wm_pid_at(int32_t x,int32_t y);
/* Non-client controls own their mouse gesture; never click through to an app. */
int kernel_wm_chrome_at(int32_t x,int32_t y);
/* Dolny pasek (i jego otwarte menu kontekstowe) jest overlayem WM: gest
   myszy w tym obszarze nalezy do WM i nie moze trafic do aplikacji pod spodem.
   Przy otwartym menu zwraca 1 dla KAZDEGO punktu (menu jest modalne). */
int kernel_wm_dock_at(int32_t x,int32_t y);
/* Release every compositor window owned by a process that is exiting. */
void kernel_wm_close_pid(uint32_t pid);
/* Close just one window of a client.  A TOS program registers its window under
   the pid of the application that launched it, so kernel_wm_close_pid() would
   take that application's own dialogs down with it. */
void kernel_wm_close_window(uint32_t pid,uint16_t handle);
/* Un-minimise, raise and focus the window owned by pid (launcher wake-up). */
int kernel_wm_activate_pid(uint32_t pid);
/* 0 = no window, 1 = visible, 2 = minimised (launcher button state). */
int kernel_wm_window_state(uint32_t pid);
/* Ctrl+Alt+Enter, called by the input hotkey filter. */
int kernel_wm_key(uint32_t type,uint32_t scan,uint32_t modifiers);

#endif
