//lcd display behavior
#include <stdint.h>
#include "board_config.h"
#include "lcd.h"

void lcd_clear_top(void); //clear top row
void lcd_clear_bottom(void); //clear bottom row
void lcd_putc(char); //move cursor's right and print input char on cursor
void lcd_putuint64(uint64_t){
	//take number 1919
	//when modding my 10 the lowest digit is taken
	//copy number digit by digit into buffer array
	//extend by one using a null terminator to automatically stop insertion

	char buf[17];
	int i
}
void lcd_set_cursor(uint8_t, uint8_t); //(0,0) top left, (1, 0) bottom left
