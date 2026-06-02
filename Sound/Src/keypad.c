/*
 * keypad.c
 *
 * 4x4 matrix keypad driver – bare-metal, no HAL.
 *
 * Row pins (output, driven low during scan):
 *   ROW0 = PB7   ROW1 = PB6   ROW2 = PG14   ROW3 = PE13
 *
 * Column pins (input, pull-up; read 0 when pressed):
 *   COL0 = PE14  COL1 = PE11  COL2 = PE9   COL3 = PG12
 *
 * Also initialises the three mode LEDs (PA6, PC0, PC3) as outputs.
 */

#include "keypad.h"
#include "mode_leds.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Register-level GPIO macros (all STM32H5 GPIO ports)                */
/* ------------------------------------------------------------------ */
#define GPIOA_BASE  0x42020000UL
#define GPIOB_BASE  0x42020400UL
#define GPIOC_BASE  0x42020800UL
#define GPIOD_BASE  0x42020C00UL
#define GPIOE_BASE  0x42021000UL
#define GPIOF_BASE  0x42021400UL
#define GPIOG_BASE  0x42021800UL

#define GPIO_MODER(b)  (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_OTYPER(b) (*(volatile uint32_t *)((b) + 0x04))
#define GPIO_PUPDR(b)  (*(volatile uint32_t *)((b) + 0x0C))
#define GPIO_IDR(b)    (*(volatile uint32_t *)((b) + 0x10))
#define GPIO_BSRR(b)   (*(volatile uint32_t *)((b) + 0x18))

/* RCC – enable GPIO clocks (AHB2 ENR, STM32H563 RM Table 71) */
#define RCC_AHB2ENR  (*(volatile uint32_t *)0x44020C8CUL)
/* bit positions: GPIOAEN=0, GPIOBEN=1, GPIOCEN=2, GPIODEN=3,
 *               GPIOEEN=4, GPIOFEN=5, GPIOGEN=6                     */

/* ------------------------------------------------------------------ */
/* GPIO helpers                                                        */
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

/* ------------------------------------------------------------------ */
/* Short software delay                                                */
/* ------------------------------------------------------------------ */

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
/* Keypad row / column tables (must match binary: see .rodata dump)    */
/* ------------------------------------------------------------------ */

typedef struct { uint32_t base; uint32_t pin; } PinDef;

static const PinDef ROW[4] = {
    { GPIOB_BASE, 7  },   /* PB7  */
    { GPIOB_BASE, 6  },   /* PB6  */
    { GPIOG_BASE, 14 },   /* PG14 */
    { GPIOE_BASE, 13 },   /* PE13 */
};

static const PinDef COL[4] = {
    { GPIOE_BASE, 14 },   /* PE14 */
    { GPIOE_BASE, 11 },   /* PE11 */
    { GPIOE_BASE, 9  },   /* PE9  */
    { GPIOG_BASE, 12 },   /* PG12 */
};

/* Physical keypad character map [row][col] */
static const char KEYS[4][4] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' },
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void keypad_init(void)
{
    /* Enable clocks for GPIOB, GPIOE, GPIOG (rows/cols)
     * and GPIOA, GPIOC (mode LEDs)                                    */
    RCC_AHB2ENR |= (1U << 0)   /* GPIOA */
                 | (1U << 1)   /* GPIOB */
                 | (1U << 2)   /* GPIOC */
                 | (1U << 4)   /* GPIOE */
                 | (1U << 6);  /* GPIOG */

    /* Row pins: push-pull output, driven high (idle) */
    for (int r = 0; r < 4; r++) {
        pin_output_pushpull(ROW[r].base, ROW[r].pin);
        pin_high(ROW[r].base, ROW[r].pin);
    }

    /* Column pins: input with pull-up */
    for (int c = 0; c < 4; c++) {
        pin_input_pullup(COL[c].base, COL[c].pin);
    }

    /* Mode LEDs */
    ModeLEDs_Init();
}

char keypad_get_current_key(void)
{
    /* First drive ALL rows HIGH to release any previously asserted row */
    for (int i = 0; i < 4; i++) {
        pin_high(ROW[i].base, ROW[i].pin);
    }

    for (int r = 0; r < 4; r++) {
        /* Drive this row LOW */
        pin_low(ROW[r].base, ROW[r].pin);
        keypad_delay_short();

        for (int c = 0; c < 4; c++) {
            if (pin_read(COL[c].base, COL[c].pin) == 0) {
                /* Key at [r][c] is pressed – release row and return */
                pin_high(ROW[r].base, ROW[r].pin);
                return KEYS[r][c];
            }
        }

        /* Release this row before trying the next */
        pin_high(ROW[r].base, ROW[r].pin);
    }

    return 0;   /* no key pressed */
}
