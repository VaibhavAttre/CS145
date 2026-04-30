#include <stdint.h>

/* =========================
   Register Addresses
   ========================= */

/* RCC */
#define RCC_AHB2ENR     ((volatile uint32_t*) 0x44020C8C)
#define RCC_APB1LENR    ((volatile uint32_t*) 0x44020C9C)

/* GPIOC: LED on PC0 */
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
#define USART3_TDR      ((volatile uint32_t*) 0x40004828)

/* =========================
   Pin Choices
   ========================= */

#define LED_PIN         0
#define USART3_TX_PIN   8   // PD8 = USART3_TX
#define USART3_RX_PIN   9   // PD9 = USART3_RX

/* =========================
   Bit Masks
   ========================= */

#define GPIOC_EN        (1U << 2)
#define GPIOD_EN        (1U << 3)
#define USART3_EN       (1U << 18)

#define USART_CR1_UE    (1U << 0)
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_TE    (1U << 3)

#define USART_ISR_TXE   (1U << 7)
#define USART_ISR_TC    (1U << 6)

#define LED_SET         (1U << LED_PIN)
#define LED_RESET       (1U << (LED_PIN + 16))

/* =========================
   Delay
   ========================= */

void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm volatile ("nop");
    }
}

/* =========================
   UART helpers
   ========================= */

void uart_send_char(char c)
{
    while ((*USART3_ISR & USART_ISR_TXE) == 0);
    *USART3_TDR = (uint32_t)c;
}

void uart_send_string(const char *s)
{
    while (*s)
    {
        uart_send_char(*s++);
    }
}

void uart_send_uint(uint32_t value)
{
    char buf[11];
    int i = 0;

    if (value == 0)
    {
        uart_send_char('0');
        return;
    }

    while (value > 0)
    {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        uart_send_char(buf[--i]);
    }
}

void uart_wait_done(void)
{
    while ((*USART3_ISR & USART_ISR_TC) == 0);
}

/* =========================
   USART setup with variable BRR
   ========================= */

void usart3_set_brr(uint32_t brr)
{
    /*
       Safest way:
       disable USART before changing BRR.
    */
    *USART3_CR1 = 0;

    *USART3_CR2 = 0;
    *USART3_CR3 = 0;
    *USART3_BRR = brr;

    *USART3_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
}

/* =========================
   Main
   ========================= */

int main(void)
{
    /* Enable clocks for GPIOC, GPIOD, USART3 */
    *RCC_AHB2ENR  |= GPIOC_EN | GPIOD_EN;
    *RCC_APB1LENR |= USART3_EN;

    /* Configure PC0 LED as output */
    *GPIOC_MODER &= ~(0x3U << (LED_PIN * 2));
    *GPIOC_MODER |=  (0x1U << (LED_PIN * 2));

    /* Configure PD8/PD9 as alternate function mode */
    *GPIOD_MODER &= ~((0x3U << (USART3_TX_PIN * 2)) |
                      (0x3U << (USART3_RX_PIN * 2)));

    *GPIOD_MODER |=  ((0x2U << (USART3_TX_PIN * 2)) |
                      (0x2U << (USART3_RX_PIN * 2)));

    /* Select AF7 for PD8 and PD9 */
    *GPIOD_AFRH &= ~((0xFU << ((USART3_TX_PIN - 8) * 4)) |
                     (0xFU << ((USART3_RX_PIN - 8) * 4)));

    *GPIOD_AFRH |=  ((7U << ((USART3_TX_PIN - 8) * 4)) |
                     (7U << ((USART3_RX_PIN - 8) * 4)));

    /* Pull-up on RX pin PD9 */
    *GPIOD_PUPDR &= ~(0x3U << (USART3_RX_PIN * 2));
    *GPIOD_PUPDR |=  (0x1U << (USART3_RX_PIN * 2));

    /*
       Candidate BRR values for 115200 baud with possible USART clocks.

       If the text becomes readable at one of these, that BRR is correct.
    */
    uint32_t brr_values[] = {
        139,    // 16 MHz
        278,    // 32 MHz
        417,    // 48 MHz
        556,    // 64 MHz
        694,    // 80 MHz
        868,    // 100 MHz
        1042,   // 120 MHz
        1085,   // 125 MHz
        1389,   // 160 MHz
        1736,   // 200 MHz
        2083,   // 240 MHz
        2170    // 250 MHz
    };

    uint32_t num_brr_values = sizeof(brr_values) / sizeof(brr_values[0]);

    while (1)
    {
        for (uint32_t i = 0; i < num_brr_values; i++)
        {
            uint32_t brr = brr_values[i];

            usart3_set_brr(brr);

            /* Blink LED so you know the firmware is alive */
            *GPIOC_BSRR = LED_SET;
            delay(500000);
            *GPIOC_BSRR = LED_RESET;

            /*
               Send the same message several times.
               The correct BRR should appear readable on the PC.
            */
            for (int repeat = 0; repeat < 5; repeat++)
            {
                uart_send_string("\r\n==============================\r\n");
                uart_send_string("BAUDSCAN BRR=");
                uart_send_uint(brr);
                uart_send_string("\r\n");
                uart_send_string("If you can read this clearly, use this BRR.\r\n");
                uart_send_string("==============================\r\n");

                uart_wait_done();
                delay(1500000);
            }
        }
    }
}
