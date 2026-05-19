#include "lcd.h"

int main(void)
{
    lcd_init();

    lcd_clear();

    lcd_set_cursor(0, 0);
    lcd_print("HELLO");

    lcd_set_cursor(1, 0);
    lcd_print("STM32H563ZI");

    while (1)
    {
    }
}
