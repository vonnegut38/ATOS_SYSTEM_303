#ifndef ASF_CONSOLE_H
#define ASF_CONSOLE_H
#include <stdint.h>
void console_clear(void);
void console_putc(char c);
void console_puts(const char *s);
void console_put_u32(uint32_t value);
void console_set_cursor(uint16_t col, uint16_t row);
void console_clear_text_row(uint16_t row);
void console_set_inverse(uint8_t enabled);
#endif
