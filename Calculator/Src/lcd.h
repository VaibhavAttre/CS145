#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear(void);
void lcd_home(void);

void lcd_set_cursor(uint8_t row, uint8_t col);

void lcd_putc(char c);
void lcd_print(const char *str);
void lcd_putuint64(uint64_t n);

void lcd_clear_top(void);
void lcd_clear_bottom(void);

#endif
