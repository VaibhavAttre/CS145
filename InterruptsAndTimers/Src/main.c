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

#define LED_PIN 0   // PC0
#define BUTTON_PIN 6   // PA6

/* USART3 */
#define USART3_TX_PIN 8
#define USART3_RX_PIN 9


/* RCC_AHB2ENR  */
#define GPIOA_EN (1U << 0)
#define GPIOC_EN (1U << 2)
#define GPIOD_EN (1U << 3)

#define LED_SET (1U << LED_PIN)
#define LED_RESET (1U << (LED_PIN + 16))

int main(void)
{


}

