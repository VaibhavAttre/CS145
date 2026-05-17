#include "keypad.h"

#define RCC_BASE       0x44020C00UL
#define GPIOA_BASE     0x42020000UL
#define GPIOB_BASE     0x42020400UL
#define GPIOC_BASE     0x42020800UL
#define GPIOD_BASE     0x42020C00UL
#define GPIOE_BASE     0x42021000UL
#define GPIOG_BASE     0x42021800UL
#define USART3_BASE    0x40004800UL

#define RCC_AHB2ENR    (*((volatile uint32_t *)(RCC_BASE + 0x8C)))
#define RCC_APB1ENR1   (*((volatile uint32_t *)(RCC_BASE + 0xA4)))

#define GPIO_MODER(base)   (*((volatile uint32_t *)((base) + 0x00)))
#define GPIO_PUPDR(base)   (*((volatile uint32_t *)((base) + 0x0C)))
#define GPIO_IDR(base)     (*((volatile uint32_t *)((base) + 0x10)))
#define GPIO_ODR(base)     (*((volatile uint32_t *)((base) + 0x14)))
#define GPIO_BSRR(base)    (*((volatile uint32_t *)((base) + 0x18)))
#define GPIO_AFRH(base)    (*((volatile uint32_t *)((base) + 0x24)))

#define USART3_CR1     (*((volatile uint32_t *)(USART3_BASE + 0x00)))
#define USART3_BRR     (*((volatile uint32_t *)(USART3_BASE + 0x0C)))
#define USART3_ISR     (*((volatile uint32_t *)(USART3_BASE + 0x1C)))
#define USART3_TDR     (*((volatile uint32_t *)(USART3_BASE + 0x28)))

static const char keymap[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 64000; i++)
        __asm__("nop");
}

static void usart3_send_char(char c)
{
    while (!(USART3_ISR & (1UL << 7)));
    USART3_TDR = (uint32_t)c;
}

static void usart3_print(const char *str)
{
    while (*str)
        usart3_send_char(*str++);
}

static void set_row(int row, int level)
{
    switch (row)
    {
        case 0: GPIO_BSRR(GPIOA_BASE) = level ? (1UL << 0)  : (1UL << 16); break;
        case 1: GPIO_BSRR(GPIOA_BASE) = level ? (1UL << 1)  : (1UL << 17); break;
        case 2: GPIO_BSRR(GPIOA_BASE) = level ? (1UL << 4)  : (1UL << 20); break;
        case 3: GPIO_BSRR(GPIOB_BASE) = level ? (1UL << 0)  : (1UL << 16); break;
    }
}

static int read_col(int col)
{
    switch (col)
    {
        case 0: return !(GPIO_IDR(GPIOC_BASE) & (1UL << 1));
        case 1: return !(GPIO_IDR(GPIOC_BASE) & (1UL << 0));
        case 2: return !(GPIO_IDR(GPIOG_BASE) & (1UL << 14));
        case 3: return !(GPIO_IDR(GPIOE_BASE) & (1UL << 13));
    }

    return 0;
}

void keypad_init(void)
{
    RCC_AHB2ENR |= (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 3) | (1UL << 4) | (1UL << 6);

    GPIO_MODER(GPIOA_BASE) &= ~((3UL << 0) | (3UL << 2) | (3UL << 8));
    GPIO_MODER(GPIOA_BASE) |=  ((1UL << 0) | (1UL << 2) | (1UL << 8));

    GPIO_MODER(GPIOB_BASE) &= ~(3UL << 0);
    GPIO_MODER(GPIOB_BASE) |=  (1UL << 0);

    GPIO_ODR(GPIOA_BASE) |= (1UL << 0) | (1UL << 1) | (1UL << 4);
    GPIO_ODR(GPIOB_BASE) |= (1UL << 0);

    GPIO_MODER(GPIOC_BASE) &= ~((3UL << 2) | (3UL << 0));
    GPIO_PUPDR(GPIOC_BASE) &= ~((3UL << 2) | (3UL << 0));
    GPIO_PUPDR(GPIOC_BASE) |=  ((1UL << 2) | (1UL << 0));

    GPIO_MODER(GPIOG_BASE) &= ~(3UL << 28);
    GPIO_PUPDR(GPIOG_BASE) &= ~(3UL << 28);
    GPIO_PUPDR(GPIOG_BASE) |=  (1UL << 28);

    GPIO_MODER(GPIOE_BASE) &= ~(3UL << 26);
    GPIO_PUPDR(GPIOE_BASE) &= ~(3UL << 26);
    GPIO_PUPDR(GPIOE_BASE) |=  (1UL << 26);

    GPIO_MODER(GPIOD_BASE) &= ~(3UL << 16);
    GPIO_MODER(GPIOD_BASE) |=  (2UL << 16);

    GPIO_AFRH(GPIOD_BASE) &= ~(0xFUL << 0);
    GPIO_AFRH(GPIOD_BASE) |=  (7UL << 0);

    RCC_APB1ENR1 |= (1UL << 18);

    USART3_BRR = 278;
    USART3_CR1 = (1UL << 3) | (1UL << 0);

    usart3_print("Keypad ready.\r\n");
}

char keypad_scan(void)
{
    for (int row = 0; row < 4; row++)
    {
        set_row(row, 0);
        delay_ms(2);

        for (int col = 0; col < 4; col++)
        {
            if (read_col(col))
            {
                set_row(row, 1);
                return keymap[row][col];
            }
        }

        set_row(row, 1);
    }

    return 0;
}

void keypad_loop(void)
{
    static char last_key = 0;

    char key = keypad_scan();

    if (key != 0 && key != last_key)
    {
        usart3_print("Key pressed: ");
        usart3_send_char(key);
        usart3_print("\r\n");
    }

    last_key = key;
    delay_ms(50);
}
