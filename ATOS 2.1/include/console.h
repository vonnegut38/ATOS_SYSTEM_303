#ifndef ASF_CONSOLE_H
#define ASF_CONSOLE_H
#include <stdint.h>
void console_clear(void);
void console_putc(char c);
volatile uint8_t *console_screen_ptr(void);
/* Like console_putc(), but writes only the glyph's non-blank scanlines.  Only
   valid where the destination cells are already blank (freshly cleared); the
   POST semigraphic frame uses it so a 68000 does not write 14 empty rows per
   border cell. */
void console_putc_thin(char c);
void console_puts_thin(const char *s);
void console_puts(const char *s);
void console_put_u32(uint32_t value);
void console_set_cursor(uint16_t col, uint16_t row);
void console_clear_text_row(uint16_t row);
void console_set_inverse(uint8_t enabled);
/* Snapshot the 80x25 character mirror and the cursor so a later stage (the
   kernel console) can continue the same log instead of clearing it. */
void console_text_copy(char *destination,uint16_t *column,uint16_t *row);
/* Retarget console output (Shifter framebuffer by default, or card VRAM). */
void console_set_screen(volatile uint8_t *screen);
void post_draw_logo(void);
void post_begin_boot(void);
void post_draw_progress(uint16_t percent,const char *label);
void post_boot_progress(uint16_t percent,const char *label);
#endif
