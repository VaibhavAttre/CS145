/*
 * lcd.h
 *
 *  Created on: May 17, 2026
 *      Author: vaibh
 */

#ifndef LCD_H
#define LCD_H

#include <stdint.h>


//add functions up here maybe like void lcd_init(void);
void lcd_clear_top(void); //clear top row
void lcd_clear_bottom(void); //clear bottom row
void lcd_putc(char); //move cursor's right and print input char on cursor
void lcd_putuint64(uint64_t);
void lcd_set_cursor(uint8_t, uint8_t); //(0,0) top left, (1, 0) bottom left

#endif /* LCD_H_ */
