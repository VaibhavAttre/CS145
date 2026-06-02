#include "lcd.h"
#include "keypad.h"
#include "calculator.h"

int main(void)
{

    lcd_init();
    keypad_init();
    lcd_clear();
    calc_init();

    while (1)
    {
        char key = keypad_get_key();
        if (key != 0)
        {
            calc_update(key);
        }
    }
}
