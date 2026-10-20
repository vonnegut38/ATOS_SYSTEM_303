#ifndef ATOS_DISPLAY_H
#define ATOS_DISPLAY_H
/* SYSTEM.GFX broker opcodes, distinct from trap #0 syscall numbers.
 * 36 = PNG icon upload, 37 = themes, 38 = game, 39 = guard, 40 = mode.
 * SET: xy=width:height, wh=depth | PREVIEW; color=0.
 * GUARD: xy=0 confirm, 1 cancel, 2 query (1 pending, 0 inactive).
 * Only the preview owner may confirm/cancel. A late confirmation fails.
 */
#define ATOS_GFX_DISPLAY_GUARD 39u
#define ATOS_GFX_DISPLAY_SET 40u
#define ATOS_DISPLAY_PREVIEW 0x100u
#define ATOS_DISPLAY_CHANGED 0x56434847u /* VCHG, kernel sender 0 */
#endif
