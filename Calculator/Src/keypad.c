#include "keypad.h"
#include <stdint.h>
#include "board_config.h"

static const char KEY[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

/* ------------------------------------------------------------------ */
/* Delays @ 250 MHz: 1 NOP ≈ 4 ns                                      */
/* ------------------------------------------------------------------ */
static void delay(volatile uint32_t n) { while (n--) __asm__("nop"); }

/* These are NOT divided — full values needed at 250 MHz */
void keypad_delay_short(void)       { delay(5000000/10);  }   /* ~20 ms settle/debounce */
static void blink_half(void)        { delay(25000000/10); }   /* ~100 ms on or off      */
static void blink_gap(void)         { delay(75000000/10); }   /* ~300 ms gap after seq  */

/* ------------------------------------------------------------------ */
/* GPIO helpers                                                         */
/* ------------------------------------------------------------------ */
static void pin_output_pushpull(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base)  &= ~(3UL << (pin * 2));
    GPIO_MODER(base)  |=  (1UL << (pin * 2));  /* 01 = output */
    GPIO_OTYPER(base) &= ~(1UL << pin);         /* 0  = push-pull */
}

static void pin_input_pullup(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base) &= ~(3UL << (pin * 2));    /* 00 = input */
    GPIO_PUPDR(base) &= ~(3UL << (pin * 2));
    GPIO_PUPDR(base) |=  (1UL << (pin * 2));    /* 01 = pull-up */
}

static void pin_high(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << pin);             /* set pin high */
}

static void pin_low(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << (pin + 16));      /* set pin low */
}

static int pin_read(uint32_t base, uint32_t pin)
{
    return (GPIO_IDR(base) >> pin) & 1;
}

/* ------------------------------------------------------------------ */
/* LED                                                                  */
/* ------------------------------------------------------------------ */
static void led_on(void)  { pin_high(LED_BASE, LED_PIN); }
static void led_off(void) { pin_low (LED_BASE, LED_PIN); }

static void led_blink(int n)
{
    for (int i = 0; i < n; i++) {
        led_on();  blink_half();
        led_off(); blink_half();
    }
    blink_gap();
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void keypad_init(void)
{
    /* Enable clocks: B=1, E=4, F=5, G=6 */
    RCC_AHB2ENR |= (1UL << 1) | (1UL << 4) | (1UL << 5) | (1UL << 6);
    (void)RCC_AHB2ENR;
    delay(500);   /* let clocks settle */

    /* LED init */
    pin_output_pushpull(LED_BASE, LED_PIN);
    led_off();

    /* ---------- Keypad init ----------
       Set row pins as push-pull outputs, drive HIGH (idle).
       Set col pins as inputs with pull-up.
       Do this ONCE here, not repeatedly inside the scan loop. */
    for (int r = 0; r < 4; r++) {
        pin_output_pushpull(ROW[r].base, ROW[r].pin);
        pin_high(ROW[r].base, ROW[r].pin);
    }
    for (int c = 0; c < 4; c++) {
        pin_input_pullup(COL[c].base, COL[c].pin);
    }

    /* 3 blinks = init OK */
    led_blink(3);
}

/*
 * Scans the keypad once.
 * Returns the pressed key char, or 0 if nothing pressed.
 * Blocks until the key is released before returning.
 *
 * Key fix: col pins are configured ONCE in init and stay as
 * pull-up inputs. The scan loop only drives rows — it does NOT
 * reconfigure col pins on every iteration.
 */
char keypad_get_key(void)
{
    for (int r = 0; r < 4; r++)
    {
        /* Drive all rows HIGH, then pull this row LOW */
        for (int i = 0; i < 4; i++) pin_high(ROW[i].base, ROW[i].pin);
        pin_low(ROW[r].base, ROW[r].pin);

        /* Wait for the signal to settle before reading */
        keypad_delay_short();

        /* Read all 4 columns */
        for (int c = 0; c < 4; c++)
        {
            if (pin_read(COL[c].base, COL[c].pin) == 0)
            {
                /* Debounce: wait another settle period and confirm */
                keypad_delay_short();
                if (pin_read(COL[c].base, COL[c].pin) == 0)
                {
                    /* Valid press — wait for release */
                    while (pin_read(COL[c].base, COL[c].pin) == 0) { }
                    keypad_delay_short();  /* debounce after release */

                    /* Return all rows HIGH before returning */
                    for (int i = 0; i < 4; i++) pin_high(ROW[i].base, ROW[i].pin);
                    return KEY[r][c];
                }
            }
        }
    }

    /* Nothing pressed — leave all rows HIGH */
    for (int i = 0; i < 4; i++) pin_high(ROW[i].base, ROW[i].pin);
    return 0;
}

char keypad_indicate_key(char key)
{
    int n = 0;
    if (key >= '1' && key <= '9') n = key - '0';
    //else if (key == '#')          n = 10;
    else if (key == '*')          n = 11;

    return (char)n;
    //if (n > 0) led_blink(n);
}
