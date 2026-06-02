/*
 * mode_leds.c
 *
 * Three mode-indicator LEDs (active-high, push-pull outputs):
 *   Play     -> PA6
 *   Record   -> PC0
 *   Playback -> PC3
 *
 * Exactly one LED is lit at any time.
 */

#include "mode_leds.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Register macros                                                     */
/* ------------------------------------------------------------------ */
#define GPIOA_BASE   0x42020000UL
#define GPIOC_BASE   0x42020800UL

#define GPIO_MODER(b)  (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_OTYPER(b) (*(volatile uint32_t *)((b) + 0x04))
#define GPIO_BSRR(b)   (*(volatile uint32_t *)((b) + 0x18))

/* RCC AHB2ENR – GPIOA bit0, GPIOC bit2 already enabled by keypad_init,
 * but we set them again for safety (idempotent OR).                   */
#define RCC_AHB2ENR  (*(volatile uint32_t *)0x44020C8CUL)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static void led_pin_init(uint32_t base, uint32_t pin)
{
    GPIO_MODER(base)  &= ~(3UL << (pin * 2));
    GPIO_MODER(base)  |=  (1UL << (pin * 2));   /* 01 = output */
    GPIO_OTYPER(base) &= ~(1UL << pin);          /* push-pull   */
}

static void led_pin_on(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << pin);              /* set   */
}

static void led_pin_off(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << (pin + 16));       /* reset */
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static SystemMode current_mode = MODE_PLAY;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void ModeLEDs_Init(void)
{
    RCC_AHB2ENR |= (1U << 0)   /* GPIOA */
                 | (1U << 2);  /* GPIOC */

    led_pin_init(GPIOA_BASE, 6);   /* PA6 – Play     */
    led_pin_init(GPIOC_BASE, 0);   /* PC0 – Record   */
    led_pin_init(GPIOC_BASE, 3);   /* PC3 – Playback */

    /* Start in Play mode */
    ModeLEDs_Set(MODE_PLAY);
}

void ModeLEDs_Set(SystemMode mode)
{
    current_mode = mode;

    /* Turn all off first */
    led_pin_off(GPIOA_BASE, 6);
    led_pin_off(GPIOC_BASE, 0);
    led_pin_off(GPIOC_BASE, 3);

    /* Light the correct one */
    switch (mode) {
        case MODE_PLAY:
            led_pin_on(GPIOA_BASE, 6);
            break;
        case MODE_RECORD:
            led_pin_on(GPIOC_BASE, 0);
            break;
        case MODE_PLAYBACK:
            led_pin_on(GPIOC_BASE, 3);
            break;
    }
}

SystemMode ModeLEDs_GetMode(void)
{
    return current_mode;
}
