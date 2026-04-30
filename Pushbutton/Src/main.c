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

/* GPIOD: USART3 TX/RX*/
#define GPIOD_MODER ((volatile uint32_t*) 0x42020C00)
#define GPIOD_PUPDR ((volatile uint32_t*) 0x42020C0C)
#define GPIOD_AFRH ((volatile uint32_t*) 0x42020C24)

/* USART3 */
#define USART3_CR1 ((volatile uint32_t*) 0x40004800)
#define USART3_CR2 ((volatile uint32_t*) 0x40004804)
#define USART3_CR3 ((volatile uint32_t*) 0x40004808)
#define USART3_BRR ((volatile uint32_t*) 0x4000480C)
#define USART3_ISR ((volatile uint32_t*) 0x4000481C)
#define USART3_RDR ((volatile uint32_t*) 0x40004824)

#define LED_PIN 0   // PC0
#define BUTTON_PIN 6   // PA6

/* USART3 */
#define USART3_TX_PIN 8
#define USART3_RX_PIN 9



/* RCC_AHB2ENR  */
#define GPIOA_EN (1U << 0)
#define GPIOC_EN (1U << 2)
#define GPIOD_EN (1U << 3)

/* RCC_APB1LENR */
#define USART3_EN (1U << 18)

/* USART_ISR bits */
#define USART_ISR_RXNE (1U << 5)

/* USART_CR1 bits */
#define USART_CR1_UE (1U << 0)
#define USART_CR1_RE (1U << 2)
#define USART_CR1_TE (1U << 3)
#define USART3_TDR ((volatile uint32_t*) 0x40004828)
#define USART_ISR_TXE (1U << 7)


#define LED_SET (1U << LED_PIN)
#define LED_RESET (1U << (LED_PIN + 16))


typedef enum {
    STATE_POSITIVE = 0,
    STATE_NEGATIVE = 1
} SystemState;


void uart_send(uint8_t c) {
    while ((*USART3_ISR & USART_ISR_TXE) == 0);
    *USART3_TDR = c;
}

int main(void) {
    /*Enable Clocks*/
    *RCC_AHB2ENR  |= GPIOA_EN | GPIOC_EN | GPIOD_EN;
    *RCC_APB1LENR |= USART3_EN;

    /* Configure led */
    *GPIOC_MODER &= ~(0x3U << (LED_PIN * 2));
    *GPIOC_MODER |=  (0x1U << (LED_PIN * 2));

    /* Configure button */
    *GPIOA_MODER &= ~(0x3U << (BUTTON_PIN * 2));

    *GPIOA_PUPDR &= ~(0x3U << (BUTTON_PIN * 2));
    *GPIOA_PUPDR |=  (0x1U << (BUTTON_PIN * 2));

    /*Configure UART*/

    *GPIOD_MODER &= ~((0x3U << (USART3_TX_PIN * 2)) | (0x3U << (USART3_RX_PIN * 2)));

    *GPIOD_MODER |=  ((0x2U << (USART3_TX_PIN * 2)) | (0x2U << (USART3_RX_PIN * 2)));

    *GPIOD_AFRH &= ~((0xFU << ((USART3_TX_PIN - 8) * 4)) | (0xFU << ((USART3_RX_PIN - 8) * 4)));

    *GPIOD_AFRH |=  ((7U << ((USART3_TX_PIN - 8) * 4)) | (7U << ((USART3_RX_PIN - 8) * 4)));

    *GPIOD_PUPDR &= ~(0x3U << (USART3_RX_PIN * 2));
    *GPIOD_PUPDR |=  (0x1U << (USART3_RX_PIN * 2));

    *USART3_CR1 = 0;
    *USART3_CR2 = 0;
    *USART3_CR3 = 0;

    *USART3_BRR = 278; // 69 //556 //

    *USART3_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

    SystemState state = STATE_POSITIVE;
    *GPIOC_BSRR = LED_RESET;

    /*for (int i = 0; i < 3; i++) {
        *GPIOC_BSRR = LED_SET;
        for (volatile int d = 0; d < 500000; d++);
        *GPIOC_BSRR = LED_RESET;
        for (volatile int d = 0; d < 500000; d++);
    }*/


    while (1)  {
        uint32_t pressed = (((*GPIOA_IDR >> BUTTON_PIN) & 1U) == 0);

        if ((*USART3_ISR & USART_ISR_RXNE) != 0) {
            uint8_t c = (uint8_t)(*USART3_RDR & 0xFFU);
            uart_send(c);

            if (c == 'p' || c == 'P') {
                state = STATE_POSITIVE;
                //uart_send('P');
                //uart_send('\r');
                //uart_send('\n');
            } else if (c == 'n' || c == 'N') {
                state = STATE_NEGATIVE;
                //uart_send('N');
                //uart_send('\r');
                //uart_send('\n');
            }
        }

        uint32_t led_on;

        if (state == STATE_POSITIVE) {
            led_on = pressed;
        } else {
            led_on = !pressed;
        }

        *GPIOC_BSRR = led_on ? LED_SET : LED_RESET;
    }
}
