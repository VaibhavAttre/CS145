#include <stdint.h>

/* =========================
   Register Addresses
   ========================= */

/* RCC */
#define RCC_AHB2ENR     ((volatile uint32_t*) 0x44020C8C)
#define RCC_APB1LENR    ((volatile uint32_t*) 0x44020C9C)

/* GPIOA: Button on PA6 (Arduino A0) */
#define GPIOA_MODER     ((volatile uint32_t*) 0x42020000)
#define GPIOA_PUPDR     ((volatile uint32_t*) 0x4202000C)
#define GPIOA_IDR       ((volatile uint32_t*) 0x42020010)

/* GPIOC: LED on PC0 (Arduino A1) */
#define GPIOC_MODER     ((volatile uint32_t*) 0x42020800)
#define GPIOC_BSRR      ((volatile uint32_t*) 0x42020818)

/* GPIOD: USART3 TX/RX through ST-LINK VCP */
#define GPIOD_MODER     ((volatile uint32_t*) 0x42020C00)
#define GPIOD_PUPDR     ((volatile uint32_t*) 0x42020C0C)
#define GPIOD_AFRH      ((volatile uint32_t*) 0x42020C24)

/* USART3 */
#define USART3_CR1      ((volatile uint32_t*) 0x40004800)
#define USART3_CR2      ((volatile uint32_t*) 0x40004804)
#define USART3_CR3      ((volatile uint32_t*) 0x40004808)
#define USART3_BRR      ((volatile uint32_t*) 0x4000480C)
#define USART3_ISR      ((volatile uint32_t*) 0x4000481C)
#define USART3_RDR      ((volatile uint32_t*) 0x40004824)

/* =========================
   Pin Choices
   ========================= */

#define LED_PIN         0   // PC0  (Arduino A1)
#define BUTTON_PIN      6   // PA6  (Arduino A0)

/* USART3 through ST-LINK USB Virtual COM Port */
#define USART3_TX_PIN   8   // PD8  = USART3_TX
#define USART3_RX_PIN   9   // PD9  = USART3_RX

/* =========================
   Bit Masks
   ========================= */

/* RCC_AHB2ENR bits */
#define GPIOA_EN        (1U << 0)
#define GPIOC_EN        (1U << 2)
#define GPIOD_EN        (1U << 3)

/* RCC_APB1LENR bits */
#define USART3_EN       (1U << 18)

/* USART_ISR bits */
#define USART_ISR_RXNE  (1U << 5)

/* USART_CR1 bits */
#define USART_CR1_UE    (1U << 0)
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_TE    (1U << 3)
#define USART3_TDR      ((volatile uint32_t*) 0x40004828)
#define USART_ISR_TXE   (1U << 7)   // TX register empty

/* BSRR helpers for LED (PC0) */
#define LED_SET         (1U << LED_PIN)
#define LED_RESET       (1U << (LED_PIN + 16))

/* =========================
   System State
   ========================= */

typedef enum {
    STATE_POSITIVE = 0,
    STATE_NEGATIVE = 1
} SystemState;


void uart_send(uint8_t c)
{
    while ((*USART3_ISR & USART_ISR_TXE) == 0);  // wait until ready
    *USART3_TDR = c;
}

/* =========================
   Main
   ========================= */

int main(void)
{
    /* --- 1. Enable clocks for GPIOA, GPIOC, GPIOD and USART3 --- */
    *RCC_AHB2ENR  |= GPIOA_EN | GPIOC_EN | GPIOD_EN;
    *RCC_APB1LENR |= USART3_EN;

    /* --- 2. Configure PC0 (LED) as output --- */
    *GPIOC_MODER &= ~(0x3U << (LED_PIN * 2));   // clear PC0 mode bits
    *GPIOC_MODER |=  (0x1U << (LED_PIN * 2));   // 01 = output

    /* --- 3. Configure PA6 (button) as input --- */
    *GPIOA_MODER &= ~(0x3U << (BUTTON_PIN * 2)); // 00 = input

    /* --- 4. Pull-up on PA6
       Not pressed = 1
       Pressed     = 0
    */
    *GPIOA_PUPDR &= ~(0x3U << (BUTTON_PIN * 2));
    *GPIOA_PUPDR |=  (0x1U << (BUTTON_PIN * 2)); // 01 = pull-up

    /* --- 5. Configure PD8/PD9 as AF7 for USART3 through ST-LINK USB --- */

    /* Set PD8 and PD9 to alternate function mode: 10 */
    *GPIOD_MODER &= ~((0x3U << (USART3_TX_PIN * 2)) |
                      (0x3U << (USART3_RX_PIN * 2)));

    *GPIOD_MODER |=  ((0x2U << (USART3_TX_PIN * 2)) |
                      (0x2U << (USART3_RX_PIN * 2)));

    /* Select AF7 for PD8 and PD9 */
    *GPIOD_AFRH &= ~((0xFU << ((USART3_TX_PIN - 8) * 4)) |
                     (0xFU << ((USART3_RX_PIN - 8) * 4)));

    *GPIOD_AFRH |=  ((7U << ((USART3_TX_PIN - 8) * 4)) |
                     (7U << ((USART3_RX_PIN - 8) * 4)));

    /* Pull-up on RX pin PD9 because UART idle is HIGH */
    *GPIOD_PUPDR &= ~(0x3U << (USART3_RX_PIN * 2));
    *GPIOD_PUPDR |=  (0x1U << (USART3_RX_PIN * 2));

    /* --- 6. Configure USART3 at 115200 baud, 8N1, assuming 16 MHz clock --- */
    *USART3_CR1 = 0;
    *USART3_CR2 = 0;
    *USART3_CR3 = 0;

    /*
       Baud rate divider:
       16,000,000 / 115,200 ≈ 139
    */
    *USART3_BRR = 556; //

    /* Enable USART3, receiver, and transmitter */
    *USART3_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

    /* --- 7. Initial system state --- */
    SystemState state = STATE_POSITIVE;   // system initially POSITIVE
    *GPIOC_BSRR = LED_RESET;              // LED off to start

    while (1)
    {
        uint32_t pressed = (((*GPIOA_IDR >> BUTTON_PIN) & 1U) == 0);

        if ((*USART3_ISR & USART_ISR_RXNE) != 0)
        {
            uint8_t c = (uint8_t)(*USART3_RDR & 0xFFU);
            uart_send(c);

            if (c == 'p' || c == 'P')
            {
                state = STATE_POSITIVE;
            }
            else if (c == 'n' || c == 'N')
            {
                state = STATE_NEGATIVE;
            }
        }

        uint32_t led_on;

        if (state == STATE_POSITIVE)
        {
            led_on = pressed;
        }
        else
        {
            led_on = !pressed;
        }

        *GPIOC_BSRR = led_on ? LED_SET : LED_RESET;
    }
}
