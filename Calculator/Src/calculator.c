//main logic
#include <stdint.h>

#include "lcd.h"
#include "keypad.h"
#include "calculator.h"

typedef enum {
	INITIAL_STATE,
	START_STATE,
	A_STATE,
	B_STATE,
	DISPLAY_STATE
} state_t;

static uint32_t A, B;
static uint64_t product;
static state_t state;
static uint8_t count;

void calc_init(){
	state = INITIAL_STATE;
	A = 0;
	B = 0;
	product = 0;
	count = 0;
}

void calc_update(char input){
	if(input == '#'){
		state = INITIAL_STATE;
	}

	switch (state) {
		case INITIAL_STATE:
			lcd_clear_top();
			lcd_clear_bottom();
			state = START_STATE;
		case START_STATE:
			lcd_clear_top();
			A = 0;
			B = 0;
			product = 0;
			count = 0;
			lcd_set_cursor(0,0);
			state = A_STATE;
		case A_STATE:
			if(input >= '0' && input <= '9'){
				A = A*10 + (input - '0');
				lcd_putc(input);
				count++;
				if( count == 8){
					lcd_clear_top();
					count = 0;
					lcd_set_cursor(0,0);
					state = B_STATE;
				}
			}
			else if(input == '*'){
				lcd_clear_top();
				count = 0;
				lcd_set_cursor(0,0);
				state = B_STATE;
			}
			break;
		case B_STATE:
			if(input >= '0' && input <= '9'){
				B = B*10 + (input-'0');
				lcd_putc(input);
				count++;
				if(count == 8){
					state = DISPLAY_STATE;
				}
				else break;
			}
			else if(input == '*'){
					state = DISPLAY_STATE;
				}
			else break;
		case DISPLAY_STATE:
			product = (uint64_t)A*B;
			lcd_clear_bottom();
			lcd_set_cursor(1,0);
			lcd_putuint64(product);
			state = START_STATE;
			break;
		default:
			break;
	}
}
