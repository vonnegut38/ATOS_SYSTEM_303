#ifndef ATOS_GAME_H
#define ATOS_GAME_H
/* Exclusive full-screen game mode (GFX opcode 38, ABI 1.2).
 *
 * A GUI client of the session on screen asks SYSTEM.GFX for the whole ET4000
 * screen (it should also ask SYSTEM.GUI for BEGIN_KEYS; when the desktop keeps
 * the keyboard, the kernel still routes every key/pointer event to the game
 * while it owns the screen):
 *
 *   gfx(38, (w<<16)|h, 0, ATOS_GAME_BEGIN)   -> (w<<16)|h of the active mode
 *   gfx(38, &frame,    0, ATOS_GAME_FRAME)   -> ATOS_GAME_FRAME_* status
 *   gfx(38, &palette,  0, ATOS_GAME_PALETTE) -> 0
 *   gfx(38, 0,         0, ATOS_GAME_END)     -> 0 (also automatic on exit)
 *
 * While the mode is active the compositor, the task bar and the mouse pointer
 * stay off the screen of that session; the game's 8 bpp frames are copied
 * (optionally doubled) straight into the presented frame and flushed to the
 * card.  Leaving the session (session switch) suspends the mode; the next
 * FRAME call reports ATOS_GAME_FRAME_REPAINT so the game resends everything.
 *
 * Every structure lives in the CLIENT's memory (image, stack or ALLOC block)
 * and is validated by the kernel; all fields are big-endian, 2-byte aligned.
 */
#include <stdint.h>

enum {
    ATOS_GAME_OPCODE  = 38u,
    ATOS_GAME_BEGIN   = 0u,
    ATOS_GAME_END     = 1u,
    ATOS_GAME_FRAME   = 2u,
    ATOS_GAME_PALETTE = 3u
};

/* FRAME results (>= 0); errors are negative. */
enum {
    ATOS_GAME_FRAME_OK      = 0,   /* drawn and flushed                      */
    ATOS_GAME_FRAME_REPAINT = 1,   /* drawn, but the screen was lost before:
                                      send a FULL frame next time            */
    ATOS_GAME_FRAME_HIDDEN  = 2    /* session not on screen: nothing drawn   */
};

#define ATOS_GAME_RECTS 128u

typedef struct {
    int16_t x,y;          /* source rectangle (pixels of the game frame)   */
    uint16_t w,h;
} atos_game_rect_t;

typedef struct {
    uint32_t pixels;      /* 8 bpp chunky source, one byte per pixel        */
    uint16_t width,height;/* source size                                    */
    uint16_t pitch;       /* bytes per source row                           */
    uint16_t scale;       /* 1 or 2 (pixel doubling in both directions)     */
    int16_t dst_x,dst_y;  /* screen position of source pixel (0,0)          */
    uint16_t count;       /* rectangles in rect[]; 0 = the whole source     */
    uint16_t flags;       /* reserved, 0                                    */
    atos_game_rect_t rect[ATOS_GAME_RECTS];
} atos_game_frame_t;

#define ATOS_GAME_FRAME_HEADER 20u

typedef struct {
    uint16_t first,count; /* palette range                                  */
    uint8_t rgb[256*3];   /* 8 bits per gun, rgb[0..2] = entry `first`     */
} atos_game_palette_t;

#define ATOS_GAME_PALETTE_HEADER 4u

/* Joystick port 1 (and port 0 while no mouse packet is pending) is delivered
   as virtual IKBD scan codes through the normal KKEY key stream (make/break),
   so every keyboard client sees it without a new message type. */
enum {
    ATOS_KEY_JOY_UP    = 0x78u,
    ATOS_KEY_JOY_DOWN  = 0x79u,
    ATOS_KEY_JOY_LEFT  = 0x7au,
    ATOS_KEY_JOY_RIGHT = 0x7bu,
    ATOS_KEY_JOY_FIRE  = 0x7cu,
    ATOS_KEY_JOY0_FIRE = 0x7du   /* fire on port 0 (the mouse port) */
};

#if defined(__STDC_VERSION__)&&__STDC_VERSION__>=201112L
_Static_assert(sizeof(atos_game_rect_t)==8,"game rect layout");
_Static_assert(sizeof(atos_game_frame_t)==ATOS_GAME_FRAME_HEADER+8u*ATOS_GAME_RECTS,"game frame layout");
_Static_assert(sizeof(atos_game_palette_t)==ATOS_GAME_PALETTE_HEADER+768u,"game palette layout");
#endif
#endif
