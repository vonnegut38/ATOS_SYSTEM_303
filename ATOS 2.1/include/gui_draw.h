#ifndef GUI_DRAW_H
#define GUI_DRAW_H
#include <stdint.h>
#define GUI_DRAW_MAX 80u
/* ABI 1.4: one synchronous frame, split into independently damaged batches.
   All counts/ranges are validated before any drawing. No nested pointers. */
#define GUI_FRAME_MAX 2048u
#define GUI_FRAME_GROUPS 32u
static inline int gui_frame_counts_valid(uint32_t total,uint32_t groups,
                                          const uint16_t *counts)
{
    uint32_t i,sum=0;
    if(!total||total>GUI_FRAME_MAX||!groups||groups>GUI_FRAME_GROUPS)return 0;
    for(i=0;i<groups;i++){
        if(!counts[i]||counts[i]>GUI_DRAW_MAX)return 0;
        sum+=counts[i];
    }
    return sum==total;
}
/* Inline text: the privileged renderer never follows nested user pointers. */
typedef struct {
    uint16_t kind;
    int16_t x,y;
    uint16_t w,h,color,background,reserved;
    char text[32];
} gui_draw_t;
/* GUI_BEVEL: a two-colour 3D border.  color = ink, background = paper,
   reserved = 0 raised / 1 sunken.  thickness is 2px (Win98-style frame). */
enum {GUI_BG=1,GUI_FILL,GUI_FRAME,GUI_TEXT,GUI_WINDOW,GUI_BEVEL,GUI_ICON,GUI_ICON_RGB,GUI_PATTERN,GUI_TILE_GRID};
/* GUI_TEXT: color = ink, background = paper, text = the string.  reserved bit 0
   draws the glyph pixels only, for text on a gradient bar whose colours the
   renderer cannot know (the themed top bar). */
/* Icon geometry: ONE shared contract for the renderer, the damage bounds, the
   stale-cell cleanup, the picker and every hit test.  The renderer draws the
   bitmap at (x,y), left of x for the centred label, and the label 16px below
   the icon.  The colour size must equal mk_icons_color.py SIZE / ICON_SIZE. */
#define GUI_ICON_COLOR_SIZE 48
#define GUI_ICON_COLOR_H    (GUI_ICON_COLOR_SIZE+16)
#define GUI_ICON_MONO_SIZE  32
#define GUI_ICON_MONO_H     (GUI_ICON_MONO_SIZE+16)
/* GUI_ICON_RGB: draw a GUI_ICON_COLOR_SIZE colour icon (system/ICONS) + label.
   color = the icon ID from icons_color.h, text = the label under the icon.
   Mono falls back to the GUI_ICON ink/paper path. */
/* GUI_ICON: draw a GUI_ICON_MONO_SIZE bitmap icon + label.  x,y = icon
   top-left, reserved = icon resource index (0 disk, 1 app, 2 folder, 3 trash),
   color = ink, background = paper, text = the label under the icon. */

/* Norton-Commander-style palette entries (programmed by kgfx_palette_defaults).
   Only meaningful in the 8 bpp card mode; mono keeps the black/white chrome. */
#define NC_BG       16u   /* dark blue: panel background, frame shade */
#define NC_BLUE     17u   /* blue: inactive title bar / bars */
#define NC_BRIGHT   18u   /* light blue: active title bar / selection */
#define NC_CYAN     19u   /* cyan: directory names / bright accents */
#endif
