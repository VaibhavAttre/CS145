#include "keypad.h"

int main(void)
{
    keypad_init();

    while (1)
    {
        keypad_loop();
    }
}
