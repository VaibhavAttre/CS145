#include "keypad.h"
#include <stdint.h>
#include "board_config.h"

static const char KEY[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

static void delay(volatile uint32_t n)
{
    while (n--) {
        __asm__("nop");
    }
}


void keypad_delay_short(void)
{
    delay(5000);
}

/* ------------------------------------------------------------------ */
/* GPIO helpers                                                        */
/* ------------------------------------------------------------------ */

static void pin_output_pushpull(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base)  &= ~(3UL << (pin * 2));
    GPIO_MODER(base)  |=  (1UL << (pin * 2));  /* 01 = output */
    GPIO_OTYPER(base) &= ~(1UL << pin);        /* 0 = push-pull */
}

static void pin_input_pullup(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base) &= ~(3UL << (pin * 2));   /* 00 = input */

    GPIO_PUPDR(base) &= ~(3UL << (pin * 2));
    GPIO_PUPDR(base) |=  (1UL << (pin * 2));   /* 01 = pull-up */
}

static void pin_high(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << pin);
}

static void pin_low(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << (pin + 16));
}

static int pin_read(uint32_t base, uint32_t pin)
{
    return (GPIO_IDR(base) >> pin) & 1;
}


static void led_on(void)
{
    pin_high(LED_BASE, LED_PIN);
}

static void led_off(void)
{
    pin_low(LED_BASE, LED_PIN);
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

void keypad_init(void)
{
    /*
     * Enable GPIO clocks.
     * Your keypad uses GPIOB, GPIOE, GPIOF, GPIOG based on your old code.
     */
    RCC_AHB2ENR |= (1UL << 1) | (1UL << 4) | (1UL << 5) | (1UL << 6);

    (void)RCC_AHB2ENR;
    delay(500);

    /* Optional LED init */
    pin_output_pushpull(LED_BASE, LED_PIN);
    led_off();

    /*
     * Rows are outputs.
     * Idle state = HIGH.
     */
    for (int r = 0; r < 4; r++) {
        pin_output_pushpull(ROW[r].base, ROW[r].pin);
        pin_high(ROW[r].base, ROW[r].pin);
    }

    for (int c = 0; c < 4; c++) {
        pin_input_pullup(COL[c].base, COL[c].pin);
    }
}


char keypad_get_current_key(void)
{
    for (int r = 0; r < 4; r++)
    {

        for (int i = 0; i < 4; i++) {
            pin_high(ROW[i].base, ROW[i].pin);
        }

        pin_low(ROW[r].base, ROW[r].pin);

        keypad_delay_short();


        for (int c = 0; c < 4; c++)
        {
            if (pin_read(COL[c].base, COL[c].pin) == 0)
            {

                for (int i = 0; i < 4; i++) {
                    pin_high(ROW[i].base, ROW[i].pin);
                }

                return KEY[r][c];
            }
        }
    }


    for (int i = 0; i < 4; i++) {
        pin_high(ROW[i].base, ROW[i].pin);
    }

    return 0;
}

char keypad_get_key(void)
{
    char key = 0;

    while (key == 0) {
        key = keypad_get_current_key();
    }

    /*
     * Wait until released.
     */
    while (keypad_get_current_key() != 0) {
    }

    keypad_delay_short();

    return key;
}
