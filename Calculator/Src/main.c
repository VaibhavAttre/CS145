#include "keypad.h"
#include "lcd.h"
#include "calculator.h"

int main(void)
{
	lcd_init();
    keypad_init();   /* 3 blinks on boot = init OK */
    calc_init();

    while (1)
    {
        char key = keypad_get_key();

        if (key != 0)
            calc_update(key);

        keypad_delay_short();
    }
}
