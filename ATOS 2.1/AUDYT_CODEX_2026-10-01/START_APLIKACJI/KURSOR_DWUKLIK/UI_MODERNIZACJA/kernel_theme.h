#ifndef KERNEL_THEME_H
#define KERNEL_THEME_H
#include <stdint.h>
/* System themes.  A theme owns the whole 8 bpp palette that describes the shell:
   entries 0..19 are the system/NC colours every application already draws with,
   and KGFX_THEME_RAMP entries AFTER the generated icon palette carry the UI ramp
   (top bar, accent, neutrals) so a theme can have gradients without stealing a
   single icon colour.

   Entries the shell must understand:
     0  ink      - application text / glyphs
     1  paper    - application bodies, desktop icon labels
     2  desktop  - the compositor's background fill (GUI_BG)
     16 frame shade, 17 inactive title, 18 active title, 19 accent */
#define KGFX_THEME_RAMP 16u
unsigned kgfx_theme_count(void);
unsigned kgfx_theme_current(void);
/* First RAMDAC entry of the UI ramp (right after the icon palette). */
unsigned kgfx_theme_ramp_base(void);
/* Program palette 0..19 and the ramp for theme i.  Returns 0 or -1. */
int kgfx_theme_apply(unsigned i);
/* Bit 0..7: palette entry for text drawn straight on the desktop background.
   Bit 8:    the desktop should draw the top bar as a gradient from ramp[0..5]. */
unsigned kgfx_theme_flags(void);
unsigned kgfx_theme_flags_of(unsigned i);
/* Packed preview colours of theme i: sys[2] | sys[18]<<12 | ramp[0]<<24
   (desktop background, active title, top bar). */
uint32_t kgfx_theme_preview(unsigned i);
#endif
