#include <stdint.h>

/* =========================
   Register Addresses
   ========================= */
#define RCC_AHB2ENR     ((volatile uint32_t*) 0x44020C8C)

#define GPIOF_MODER     ((volatile uint32_t*) 0x42021400)
#define GPIOF_PUPDR     ((volatile uint32_t*) 0x4202140C)
#define GPIOF_IDR       ((volatile uint32_t*) 0x42021410)
#define GPIOF_BSRR      ((volatile uint32_t*) 0x42021418)

/* =========================
   Pin / Bit Definitions
   ========================= */
#define LED_PIN         0   // PF0 — output
#define BUTTON_PIN      1   // PF1 — input, wired to GND

#define GPIOF_EN        (1U << 5)

/* BSRR: lower 16 bits = SET, upper 16 bits = RESET */
#define LED_SET         (1U << LED_PIN)
#define LED_RESET       (1U << (LED_PIN + 16))

/* =========================
   Simple busy-wait delay
   ========================= */
static void delay(volatile uint32_t count)
{
    while (count--);
}

int main(void)
{
    /* 1. Enable GPIOF clock */
    *RCC_AHB2ENR |= GPIOF_EN;

    /* 2. Configure PF0 as output (MODER bits [1:0] = 01) */
    *GPIOF_MODER &= ~(0x3U << (LED_PIN * 2));   // clear
    *GPIOF_MODER |=  (0x1U << (LED_PIN * 2));   // set to output

    /* 3. Configure PF1 as input (MODER bits [3:2] = 00) */
    *GPIOF_MODER &= ~(0x3U << (BUTTON_PIN * 2)); // clear = input mode

    /* 4. Enable pull-up on PF1 (PUPDR bits [3:2] = 01)
          Pin floats HIGH at rest, goes LOW when button pressed to GND */
    *GPIOF_PUPDR &= ~(0x3U << (BUTTON_PIN * 2));
    *GPIOF_PUPDR |=  (0x1U << (BUTTON_PIN * 2));

    /* 5. LED off to start */
    *GPIOF_BSRR = LED_RESET;

    uint32_t led_state   = 0;   // 0 = off, 1 = on
    uint32_t prev_button = 1;   // last stable button state (1 = released)

    while (1)
    {
        uint32_t current_button = (*GPIOF_IDR >> BUTTON_PIN) & 1U;

        /* Detect falling edge: was released (1), now pressed (0) */
        if (prev_button == 1 && current_button == 0)
        {
            delay(80000);   // ~debounce: wait ~5 ms at 16 MHz HSI

            /* Re-read after debounce to confirm it's a real press */
            if (((*GPIOF_IDR >> BUTTON_PIN) & 1U) == 0)
            {
                led_state ^= 1;   // toggle

                if (led_state)
                    *GPIOF_BSRR = LED_SET;
                else
                    *GPIOF_BSRR = LED_RESET;
            }
        }

        prev_button = current_button;
    }
}
