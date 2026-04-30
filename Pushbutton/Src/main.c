#include <stdint.h>

/* =========================
   Register Addresses
   ========================= */

/* RCC */
#define RCC_AHB2ENR     ((volatile uint32_t*) 0x44020C8C)
#define RCC_APB1LENR    ((volatile uint32_t*) 0x44020C9C)

/* GPIOA: LED (PA1) + Button (PA0) */
#define GPIOA_MODER     ((volatile uint32_t*) 0x42020000)
#define GPIOA_PUPDR     ((volatile uint32_t*) 0x4202000C)
#define GPIOA_IDR       ((volatile uint32_t*) 0x42020010)
#define GPIOA_BSRR      ((volatile uint32_t*) 0x42020018)

/* GPIOB: USART3 TX/RX (PB10/PB11) */
#define GPIOB_MODER     ((volatile uint32_t*) 0x42020400)
#define GPIOB_PUPDR     ((volatile uint32_t*) 0x4202040C)
#define GPIOB_AFRH      ((volatile uint32_t*) 0x42020424)

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

#define LED_PIN         1   // PA1
#define BUTTON_PIN      0   // PA0

#define USART3_TX_PIN   10  // PB10
#define USART3_RX_PIN   11  // PB11


/* =========================
   Bit Masks
   ========================= */

/* RCC_AHB2ENR bits */
#define GPIOA_EN        (1U << 0)
#define GPIOB_EN        (1U << 1)

/* RCC_APB1LENR bits */
#define USART3_EN       (1U << 18)

/* USART_ISR bits */
#define USART_ISR_RXNE  (1U << 5)

/* USART_CR1 bits */
#define USART_CR1_UE    (1U << 0)
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_TE    (1U << 3)

/* BSRR helpers */
#define LED_SET         (1U << LED_PIN)
#define LED_RESET       (1U << (LED_PIN + 16))


/* =========================
   Delay
   ========================= */

static void delay(volatile uint32_t count)
{
    while (count--);
}


/* =========================
   Main
   ========================= */

int main(void)
{
    /* --- 1. Enable clocks --- */
    *RCC_AHB2ENR  |= GPIOA_EN | GPIOB_EN;
    *RCC_APB1LENR |= USART3_EN;

    /* --- 2. Configure PA1 (LED) as output --- */
    *GPIOA_MODER &= ~(0x3U << (LED_PIN * 2));   // clear
    *GPIOA_MODER |=  (0x1U << (LED_PIN * 2));   // 01 = output

    /* --- 3. Configure PA0 (button) as input --- */
    *GPIOA_MODER &= ~(0x3U << (BUTTON_PIN * 2)); // 00 = input

    /* --- 4. Pull-up on PA0 (pin floats HIGH, goes LOW when pressed) --- */
    *GPIOA_PUPDR &= ~(0x3U << (BUTTON_PIN * 2));
    *GPIOA_PUPDR |=  (0x1U << (BUTTON_PIN * 2)); // 01 = pull-up

    /* --- 5. Configure PB10/PB11 as alternate function (AF7 = USART3) --- */
    *GPIOB_MODER &= ~((0x3U << (USART3_TX_PIN * 2)) |
                      (0x3U << (USART3_RX_PIN * 2)));
    *GPIOB_MODER |=  ((0x2U << (USART3_TX_PIN * 2)) |  // 10 = AF
                      (0x2U << (USART3_RX_PIN * 2)));

    /* AFRH covers pins 8-15; pin 10 = bits [11:8], pin 11 = bits [15:12] */
    *GPIOB_AFRH &= ~((0xFU << ((USART3_TX_PIN - 8) * 4)) |
                     (0xFU << ((USART3_RX_PIN - 8) * 4)));
    *GPIOB_AFRH |=  ((7U  << ((USART3_TX_PIN - 8) * 4)) |  // AF7
                     (7U  << ((USART3_RX_PIN - 8) * 4)));

    /* Pull-up on RX pin */
    *GPIOB_PUPDR &= ~(0x3U << (USART3_RX_PIN * 2));
    *GPIOB_PUPDR |=  (0x1U << (USART3_RX_PIN * 2));

    /* --- 6. Configure USART3 (115200 baud @ 16 MHz HSI) --- */
    *USART3_CR1 = 0;                // disable while configuring
    *USART3_CR2 = 0;                // 1 stop bit
    *USART3_CR3 = 0;                // no flow control
    *USART3_BRR = 139;              // 16000000 / 115200 ≈ 139
    *USART3_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

    /* --- 7. LED off to start --- */
    *GPIOA_BSRR = LED_RESET;

    /* --- 8. Main loop --- */
    uint32_t led_state   = 0;
    uint32_t prev_button = 1;   // 1 = released (pull-up HIGH)

    while (1)
    {
        uint32_t current_button = (*GPIOA_IDR >> BUTTON_PIN) & 1U;

        /* Falling edge = button just pressed */
        if (prev_button == 1 && current_button == 0)
        {
            delay(80000);   // debounce ~5 ms @ 16 MHz

            if (((*GPIOA_IDR >> BUTTON_PIN) & 1U) == 0)  // confirm still pressed
            {
                led_state ^= 1;
                *GPIOA_BSRR = led_state ? LED_SET : LED_RESET;
            }
        }

        prev_button = current_button;
    }
}
