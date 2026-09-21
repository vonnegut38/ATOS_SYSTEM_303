#ifndef TEXT_TERMINAL_H
#define TEXT_TERMINAL_H
#include <stdint.h>
#define TERM_COLS 80u
#define TERM_ROWS 25u
#define TERM_CELLS (TERM_COLS*TERM_ROWS)
#define TERM_INVERSE 1u
#define TERM_UNDERLINE 2u
#define TERM_NORTH 1u
#define TERM_EAST 2u
#define TERM_SOUTH 4u
#define TERM_WEST 8u
#define TERM_LINE(mask) (0x80u+(mask))
#define TERM_SHADE 0x90u
#define TERM_BLOCK 0x91u
/* Low byte: character. High byte: attributes. Native-endian 16-bit ABI. */
typedef uint16_t term_cell_t;
/* cols/rows are the runtime grid size. The buffer is sized for the maximum
   (80x25) so the same terminal can serve a smaller, wrapping window. */
typedef struct {term_cell_t cells[TERM_CELLS];uint16_t column,row;
                uint16_t cols,rows;uint8_t attr,pad;} text_terminal_t;
typedef struct {term_cell_t cells[TERM_CELLS];uint8_t valid;} term_display_t;
void term_clear(text_terminal_t *,unsigned attr);
void term_fill(text_terminal_t *,int x,int y,int w,int h,unsigned ch,unsigned attr);
void term_text(text_terminal_t *,int x,int y,const char *,unsigned attr,unsigned limit);
void term_frame(text_terminal_t *,int x,int y,int w,int h,unsigned attr);
void term_scroll(text_terminal_t *);
void term_write(text_terminal_t *,const char *);
/* Pure renderer: a separate display cache belongs to the visible display,
 * never to a global text cursor. A changed pair requires sixteen word stores. */
void term_font_prepare(uint8_t target[256][16],const uint8_t source[256][16]);
void term_display_invalidate(term_display_t *);
unsigned term_render_mono(term_display_t *,const term_cell_t *,volatile uint16_t *,
                          const uint8_t font[256][16]);
/* 8x8 variant: 80x25 cells occupy 640x200 pixels, so a whole text screen
   fits a window with a frame on a 640x400 display. */
unsigned term_render_mono8(term_display_t *,const term_cell_t *,volatile uint16_t *,
                           const uint8_t font[256][8]);
#endif
