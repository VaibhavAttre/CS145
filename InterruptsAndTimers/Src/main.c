#include <stdint.h>

/* RCC */
#define RCC_AHB2ENR ((volatile uint32_t*) 0x44020C8C)
#define RCC_APB1LENR ((volatile uint32_t*) 0x44020C9C)

/* GPIOA: Button */
#define GPIOA_MODER ((volatile uint32_t*) 0x42020000)
#define GPIOA_PUPDR ((volatile uint32_t*) 0x4202000C)
#define GPIOA_IDR ((volatile uint32_t*) 0x42020010)

/* GPIOC: LED */
#define GPIOC_MODER ((volatile uint32_t*) 0x42020800)
#define GPIOC_BSRR ((volatile uint32_t*) 0x42020818)

/* NVIC */
#define NVIC_ISER1 ((volatile uint32_t*))
#define TIM2_NVIC_BIT 13U

/* TIMER */
#define TIM2_CR1
#define TIM2_DIER
#define TIM2_SR
#define TIM2_EGR
#define TIM2_PSC
#define TIM2_ARR

#define LED_PIN 1U   // PC0
#define BUTTON_PIN 6   // PA6


/* RCC_AHB2ENR  */
#define GPIOA_EN (1U << 0)
#define GPIOC_EN (1U << 2)
#define GPIOD_EN (1U << 3)

#define LED_SET (1U << LED_PIN)
#define LED_RESET (1U << (LED_PIN + 16))

int main(void)
{


}

