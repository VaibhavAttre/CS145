#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdint.h>

/*
 * pwm_driver.h
 *
 * Uses TIM2 Channel 1 on PA0 (AF1)
 * PA0 = Arduino connector A0 / CN8 pin 1 on Nucleo boards
 *
 * If using an EXTERNAL LED:
 *   - Connect LED anode -> 330Ω resistor -> PA0
 *   - Connect LED cathode -> GND
 */

/**
 * Initialize TIM2 CH1 PWM output on PA5.
 *
 * @param arr       Auto-reload register value (sets PWM period).
 *                  Period = (arr + 1) / (SystemCoreClock / prescaler)
 * @param prescaler Clock prescaler (divides 16 MHz system clock).
 *                  e.g. prescaler=16 -> timer ticks at 1 MHz
 */
void PWM_Init(uint32_t arr, uint32_t prescaler);

/**
 * Set the PWM duty cycle.
 *
 * @param duty  Value from 0 to arr (inclusive).
 *              0   = always OFF  (0% duty cycle)
 *              arr = always ON   (100% duty cycle)
 *              Intermediate values produce PWM.
 */
void PWM_SetDuty(uint32_t duty);

/**
 * Get the current ARR (max counter value).
 * Useful so breathing_led can compute percentages.
 */
uint32_t PWM_GetARR(void);

#endif
