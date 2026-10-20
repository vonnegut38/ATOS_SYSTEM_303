#ifndef KERNEL_THEME_H
#define KERNEL_THEME_H
#include <stdint.h>
/* System themes.  A theme owns the whole 8 bpp palette that describes the shell:
   entries 0..19 are the system/NC colours every application already draws with,
   and the LAST KGFX_THEME_RAMP entries carry the UI ramp (top bar gradient) so a
   theme can have a gradient without stealing a single icon colour.

   The palette map is fixed and the ranges must never overlap:
     0..19     system/NC colours (ink, paper, desktop, chrome, titles, accent)
     20..235   generated icon palette (ICON_PAL_BASE, ICON_PAL_COLORS)
     236..251  TOS window client palette (TOS_CLIENT_PALETTE_BASE, 16 colours)
     252..255  UI ramp (KGFX_THEME_RAMP)

   The TOS range used to sit exactly on the ramp, so starting ANY TOS program
   repainted the desktop top bar with the program's colours (and a mono program
   additionally pinned entries 0/15).  The ramp needs only the four entries the
   bar gradient already used, so it moved to the top of the palette and the TOS
   window got a range nothing else touches.

   Entries the shell must understand:
     0  ink      - application text / glyphs
     1  paper    - application bodies, desktop icon labels
     2  desktop  - the compositor's background fill (GUI_BG)
     16 frame shade, 17 inactive title, 18 active title, 19 accent */
#define KGFX_THEME_RAMP 4u
unsigned kgfx_theme_count(void);
unsigned kgfx_theme_current(void);
/* First RAMDAC entry of the UI ramp (the last entries of the palette). */
unsigned kgfx_theme_ramp_base(void);
/* Program palette 0..19 and the ramp for theme i.  Returns 0 or -1. */
int kgfx_theme_apply(unsigned i);
int kgfx_theme_set_desktop(unsigned rgb);   /* 0xFFFF = kolor motywu */
unsigned kgfx_theme_desktop(void);
int kgfx_theme_set_bar(unsigned rgb);        /* 0xFFFF = rampa motywu */
unsigned kgfx_theme_bar(void);
int kgfx_theme_set_wall(unsigned n);
unsigned kgfx_theme_wall(void);
/* Bit 0..7: palette entry for text drawn straight on the desktop background.
   Bit 8:    the desktop should draw the top bar as a gradient from ramp[0..3]. */
unsigned kgfx_theme_flags(void);
unsigned kgfx_theme_flags_of(unsigned i);
/* 1 when the top bar is a themed gradient: whatever draws text on that bar
   (the desktop menu, the window manager's clock) must paint glyphs only. */
unsigned kgfx_theme_ink_on_bar(void);
/* Packed preview colours of theme i: sys[2] | sys[18]<<12 | ramp[0]<<24
   (desktop background, active title, top bar). */
uint32_t kgfx_theme_preview(unsigned i);
#endif
