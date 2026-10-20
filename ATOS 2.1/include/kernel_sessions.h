#ifndef KERNEL_SESSIONS_H
#define KERNEL_SESSIONS_H
#include <stdint.h>
#include "kernel_shell.h"
#define SESSION_COUNT 3u
unsigned ksessions_current(void);
/* "Add Desk": give the newly created session its own desktop instance. */
void ksessions_autostart_desktop(void);
int  ksessions_take_autostart(void);
/* Test-only: inject a synthetic mouse state (absolute x,y + buttons). */
void ksessions_mouse_inject(int32_t,int32_t,uint32_t);
/* Current held button state (not consumed by the GUI wake path). */
uint32_t ksessions_mouse_buttons(void);
unsigned ksessions_mask(void);
int32_t ksessions_control(uint32_t op,uint32_t value);
int ksessions_key(uint32_t type,uint32_t scan);
void ksessions_init(const kernel_shell_service_t *shell,int enabled);
/* Hand the kernel the ROM's warm-reset entry (Ctrl+Alt+Del). */
void ksessions_set_reset(void (*fn)(void));
void ksessions_pump(void);
void ksessions_unpaint(void);
void ksessions_paint(void);
int ksessions_mouse_event(uint32_t *,int32_t *,int32_t *);
/* Ordered-edge transport: peek the oldest queued event (buttons + absolute
   position + tick) without consuming it, so the caller can confirm the real
   recipient has mailbox room before the event leaves the queue. */
int ksessions_mouse_peek(uint32_t *,int32_t *,int32_t *,uint32_t *);
void ksessions_mouse_consume(void);
void ksessions_mouse_flush(void);
int ksessions_mouse_x(void);
int ksessions_mouse_y(void);
/* Absolute position of the last button-down edge.  The compositor samples the
   mouse once per frame, so a press injected (or clicked) between two frames
   would otherwise be hit-tested at the wrong position: a scripted drag lost
   the title bar and a fast real click could miss its target. */
void ksessions_mouse_script(int on);
int ksessions_mouse_scripted(void);
int ksessions_mouse_press_x(void);
int ksessions_mouse_press_y(void);
int ksessions_mouse_press_pending(void);
void ksessions_mouse_press_consume(void);
#endif
