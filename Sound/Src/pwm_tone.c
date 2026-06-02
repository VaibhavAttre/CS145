/*
 * pwm_tone.c
 *
 * Bare-metal PWM tone output using TIM2 channel 1 on PA0 (AF1).
 *
 * Timer clock: 64 MHz (PCLK1 after default PLL setup on NUCLEO-H563ZI).
 * Prescaler  : 64 – 1 = 63  =>  1 MHz tick.
 * ARR        : (1 000 000 / freq) – 1
 * CCR1       : (ARR + 1) / 2   => 50 % duty cycle
 */

#include "pwm_tone.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Register addresses (STM32H563 Reference Manual)                    */
/* ------------------------------------------------------------------ */

/* RCC */
#define RCC_AHB2ENR   (*(volatile uint32_t *)0x44020C8CUL)   /* GPIOA bit 0 */
#define RCC_APB1LENR  (*(volatile uint32_t *)0x44020C9CUL)   /* TIM2  bit 0 */

/* GPIOA */
#define GPIOA_BASE    0x42020000UL
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRL    (*(volatile uint32_t *)(GPIOA_BASE + 0x20))

/* TIM2 (APB1, base 0x40000000) */
#define TIM2_BASE     0x40000000UL
#define TIM2_CR1      (*(volatile uint32_t *)(TIM2_BASE + 0x00))
#define TIM2_EGR      (*(volatile uint32_t *)(TIM2_BASE + 0x14))
#define TIM2_CCMR1    (*(volatile uint32_t *)(TIM2_BASE + 0x18))
#define TIM2_CCER     (*(volatile uint32_t *)(TIM2_BASE + 0x20))
#define TIM2_PSC      (*(volatile uint32_t *)(TIM2_BASE + 0x28))
#define TIM2_ARR      (*(volatile uint32_t *)(TIM2_BASE + 0x2C))
#define TIM2_CCR1     (*(volatile uint32_t *)(TIM2_BASE + 0x34))

#define PWM_TICK_HZ   1000000UL

static uint32_t current_arr = 0;

void PWM_Tone_Init(void)
{
    /* Enable GPIOA clock */
    RCC_AHB2ENR |= (1U << 0);

    /* Enable TIM2 clock */
    RCC_APB1LENR |= (1U << 0);

    /*
     * PA0 alternate function mode
     * PA0 = TIM2_CH1 using AF1
     */
    GPIOA_MODER &= ~(3U << (0 * 2));
    GPIOA_MODER |=  (2U << (0 * 2));       /* 10 = alternate function */

    GPIOA_AFRL &= ~(0xFU << (0 * 4));
    GPIOA_AFRL |=  (0x1U << (0 * 4));      /* AF1 = TIM2_CH1 */

    /* Prescaler: 64 MHz / 64 = 1 MHz tick */
    TIM2_PSC = (PWM_TIMER_CLOCK_HZ / PWM_TICK_HZ) - 1U;

    /* Default ARR (will be overwritten by PWM_Tone_Play) */
    TIM2_ARR  = 1000U - 1U;
    TIM2_CCR1 = 0U;

    /*
     * PWM mode 1 on channel 1.
     * OC1M bits [6:4] = 110 means PWM mode 1.
     * OC1PE bit 3 enables preload for CCR1.
     */
    TIM2_CCMR1 &= ~(0x7U << 4);
    TIM2_CCMR1 |=  (0x6U << 4);
    TIM2_CCMR1 |=  (1U   << 3);

    /* Enable channel 1 output */
    TIM2_CCER |= (1U << 0);

    /* ARPE: auto-reload preload enable */
    TIM2_CR1 |= (1U << 7);

    /* Generate update event so PSC/ARR load */
    TIM2_EGR |= (1U << 0);

    /* Enable timer */
    TIM2_CR1 |= (1U << 0);
}

void PWM_Tone_Play(uint32_t frequency_hz)
{
    if (frequency_hz == 0U) {
        PWM_Tone_Stop();
        return;
    }

    uint32_t arr = (PWM_TICK_HZ / frequency_hz) - 1U;
    current_arr = arr;

    TIM2_ARR  = arr;
    TIM2_CCR1 = (arr + 1U) / 2U;           /* 50 % duty cycle */

    /* Force update so ARR/CCR apply cleanly */
    TIM2_EGR |= (1U << 0);
}

void PWM_Tone_Stop(void)
{
    TIM2_CCR1 = 0U;
}
