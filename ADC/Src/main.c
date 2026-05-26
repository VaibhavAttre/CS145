#include "breathing_led.h"
#include "pwm_driver.h"

#define FAKE_ADC 4095U

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        for (volatile uint32_t j = 0; j < 4000; j++)
        {
            __asm("nop");
        }
    }
}

int main(void)
{

    PWM_Init(1000, 16);
    BREATHING_LED_Init();

    while (1)
    {

    	//READ POTENTOIMETER/ADC VALUE

        BREATHING_LED_Update(FAKE_ADC);


        delay_ms(1);
    }

    return 0;
}

