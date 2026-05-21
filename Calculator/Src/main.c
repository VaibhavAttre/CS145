#include "lcd.h"
#include "keypad.h"
#include "calculator.h"

int main(void)
{
	lcd_init();
	keypad_init();

    lcd_clear();

    /*
    lcd_set_cursor(0, 0);
    lcd_print("HELLO");

    lcd_set_cursor(1, 0);
    lcd_print("STM32H563ZI");*/

    while (1)
    {
    	char key = keypad_get_key();

        lcd_set_cursor(0, 0);
        lcd_putc(' ');

        lcd_set_cursor(0, 0);
        lcd_putc(key);
    }
}
