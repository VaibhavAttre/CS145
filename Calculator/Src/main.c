#include "keypad.h"

int main(void)
{
    keypad_init();   /* 3 blinks on boot = init OK */

    while (1)
    {
        char key = keypad_get_key();

        if (key != 0)
            keypad_indicate_key(key);

        keypad_delay_short();
    }
}
