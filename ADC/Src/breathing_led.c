#include "breathing_led.h"
#include "pwm_driver.h"


#define MAX_DELAY 5U
#define MIN_DELAY 1U

#define ADC_MAX 4095U

static uint32_t duty = 0;
static uint32_t tick = 0;
static uint32_t delay = MAX_DELAY;
static int direction = 1;   /* +1 = going brighter, -1 = going dimmer */

void BREATHING_LED_Init(void)
{
    duty = 0;
    tick = 0;
    delay = MAX_DELAY;
    direction = 1;
    PWM_SetDuty(0);
}

void BREATHING_LED_Update(uint32_t adc_value)
{
    uint32_t arr = PWM_GetARR();


    uint32_t range = MAX_DELAY - MIN_DELAY;
    delay = MAX_DELAY - ((adc_value * range) / ADC_MAX);
    if (delay < MIN_DELAY) delay = MIN_DELAY;


    tick++;
    if (tick < delay) return;
    tick = 0;

    if (direction == 1)
    {
        duty++;
        if (duty >= arr)
        {
            duty = arr;
            direction = -1;
        }
    }
    else
    {
        if (duty == 0)
        {
            direction = 1;
        }
        else
        {
            duty--;
        }
    }

    PWM_SetDuty(duty);
}
