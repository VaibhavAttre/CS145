#include "pwm_driver.h"
#include <stdint.h>

#define RCC_BASE    0x44020C00UL
#define GPIOA_BASE  0x42020000UL
#define TIM2_BASE   0x40000000UL

#define RCC_AHB2ENR  (*((volatile uint32_t *)(RCC_BASE + 0x8C)))  // GPIOAEN
#define RCC_APB1LENR (*((volatile uint32_t *)(RCC_BASE + 0x9C)))  // TIM2EN

/* GPIOA registers (offsets same as F4) */
#define GPIOA_MODER (*((volatile uint32_t *)(GPIOA_BASE + 0x00)))
#define GPIOA_AFRL  (*((volatile uint32_t *)(GPIOA_BASE + 0x20)))

/* TIM2 registers (offsets same as F4) */
#define TIM2_CR1   (*((volatile uint32_t *)(TIM2_BASE + 0x00)))
#define TIM2_EGR   (*((volatile uint32_t *)(TIM2_BASE + 0x14)))
#define TIM2_CCMR1 (*((volatile uint32_t *)(TIM2_BASE + 0x18)))
#define TIM2_CCER  (*((volatile uint32_t *)(TIM2_BASE + 0x20)))
#define TIM2_PSC   (*((volatile uint32_t *)(TIM2_BASE + 0x28)))
#define TIM2_ARR   (*((volatile uint32_t *)(TIM2_BASE + 0x2C)))
#define TIM2_CCR1  (*((volatile uint32_t *)(TIM2_BASE + 0x34)))

static uint32_t arr = 0;

void PWM_Init(uint32_t arrv, uint32_t presc) {

	arr = arrv;

	RCC_AHB2ENR |= (1U << 0); //GPIOA
	RCC_APB1LENR |= (1U << 0); //TIM2EN

	GPIOA_MODER &= ~(3U << 0);
	GPIOA_MODER |= (2U << 0);

	GPIOA_AFRL &= ~(0xFU << 0);
	GPIOA_AFRL |= (0x1U << 0); // AF1 tim2_ch1

	TIM2_PSC = presc -1;
	TIM2_ARR = arrv;

	TIM2_CCMR1 &= ~(0x7U << 4); //clear bits OC1M [6:4]
	TIM2_CCMR1 |= (0x6U << 4); //PWM mode 1
 	TIM2_CCMR1 |= (1U << 3); //OC1PE = 1 preload enable

 	TIM2_CCER |= (1U << 0); //enable timer output

 	TIM2_CR1 |= (1U << 7);

 	TIM2_EGR |= (1U << 0); // UG Bit

 	TIM2_CR1 |= (1U << 0); //start timer
}

void PWM_SetDuty(uint32_t duty) {

	if(duty > arr) duty = arr;
	TIM2_CCR1 = duty;
}

uint32_t PWM_GetARR(void) {
	return arr;
}
