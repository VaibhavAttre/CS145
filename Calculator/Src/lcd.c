//lcd display behavior
#include <stdint.h>
#include "board_config.h"
#include "lcd.h"

void lcd_clear_top(void); //clear top row
void lcd_clear_bottom(void); //clear bottom row
void lcd_putc(char); //move cursor's right and print input char on cursor
void lcd_putuint64(uint64_t n){

	char buf[17];
	int i = 16;

	buf[i] = '\0';

	if(n == 0){
		lcd_putc('0');
		return;
	}

	while(n>0){
		buf[--i] = (char)(('0') + (n % 10));
		n /= 10;
	}

	while(buf[i] != '\0'){
		lcd_putc(buf[i++]);
	}

}
void lcd_set_cursor(uint8_t, uint8_t); //(0,0) top left, (1, 0) bottom left
