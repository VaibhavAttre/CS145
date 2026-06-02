#include <stdint.h>
#include "keypad.h"
#include "pwm_tone.h"
#include "tones.h"

int main(void)
{
    keypad_init();
    PWM_Tone_Init();

    char last_key = 0;

    while (1)
    {
        char key = keypad_get_current_key();

        uint32_t freq = Tone_Frequency_From_Key(key);

        if (freq != 0)
        {

            if (key != last_key)
            {
                PWM_Tone_Play(freq);
                last_key = key;
            }
        }
        else
        {

            PWM_Tone_Stop();
            last_key = 0;
        }
    }
}
