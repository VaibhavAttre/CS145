#include "lcd.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Delay helpers
 * ============================================================ */

static void delay_cycles(volatile uint32_t cycles)
{
    while (cycles--)
    {
        __asm volatile ("nop");
    }
}

static void delay_us(uint32_t us)
{
    /*
     * Rough delay.
     * If your clock is around 64 MHz, this is enough for LCD timing.
     */
    while (us--)
    {
        delay_cycles(80U);
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000U);
    }
}

/* ============================================================
 * GPIO helpers
 * ============================================================ */

static void gpio_output(uint32_t base, uint32_t pin)
{
    /* MODER: 01 = general purpose output */
    GPIO_MODER(base) &= ~(3UL << (pin * 2U));
    GPIO_MODER(base) |=  (1UL << (pin * 2U));

    /* OTYPER: 0 = push-pull */
    GPIO_OTYPER(base) &= ~(1UL << pin);

    /* OSPEEDR: 10 = high speed */
    GPIO_OSPEEDR(base) &= ~(3UL << (pin * 2U));
    GPIO_OSPEEDR(base) |=  (2UL << (pin * 2U));

    /* PUPDR: 00 = no pull-up/pull-down */
    GPIO_PUPDR(base) &= ~(3UL << (pin * 2U));
}

static void gpio_set(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << pin);
}

static void gpio_clear(uint32_t base, uint32_t pin)
{
    GPIO_BSRR(base) = (1UL << (pin + 16U));
}

static void gpio_write(uint32_t base, uint32_t pin, uint8_t value)
{
    if (value)
    {
        gpio_set(base, pin);
    }
    else
    {
        gpio_clear(base, pin);
    }
}

/* ============================================================
 * LCD low-level helpers
 * ============================================================ */

static void lcd_enable_pulse(void)
{
    gpio_set(LCD_EN_BASE, LCD_EN_PIN);
    delay_us(2U);

    gpio_clear(LCD_EN_BASE, LCD_EN_PIN);
    delay_us(80U);
}

static void lcd_write_nibble(uint8_t nibble)
{
    gpio_write(LCD_D4_BASE, LCD_D4_PIN, (nibble >> 0U) & 1U);
    gpio_write(LCD_D5_BASE, LCD_D5_PIN, (nibble >> 1U) & 1U);
    gpio_write(LCD_D6_BASE, LCD_D6_PIN, (nibble >> 2U) & 1U);
    gpio_write(LCD_D7_BASE, LCD_D7_PIN, (nibble >> 3U) & 1U);

    lcd_enable_pulse();
}

static void lcd_send(uint8_t byte, uint8_t rs)
{
    gpio_write(LCD_RS_BASE, LCD_RS_PIN, rs);

    /* RW = 0 means write mode */
    gpio_clear(LCD_RW_BASE, LCD_RW_PIN);

    /* Send high nibble first, then low nibble */
    lcd_write_nibble(byte >> 4U);
    lcd_write_nibble(byte & 0x0FU);
}

static void lcd_command(uint8_t cmd)
{
    lcd_send(cmd, 0U);

    if (cmd == 0x01U || cmd == 0x02U)
    {
        delay_ms(2U);
    }
    else
    {
        delay_us(80U);
    }
}

static void lcd_data(uint8_t data)
{
    lcd_send(data, 1U);
    delay_us(80U);
}

/* ============================================================
 * Public API
 * ============================================================ */

void lcd_init(void)
{
    /*
     * Enable GPIO clocks:
     * GPIOA = bit 0
     * GPIOB = bit 1
     * GPIOC = bit 2
     * GPIOD = bit 3
     * GPIOF = bit 5
     */
    RCC_AHB2ENR |= (1UL << 0U)
                |  (1UL << 1U)
                |  (1UL << 2U)
                |  (1UL << 3U)
                |  (1UL << 5U);

    /* Small delay after enabling clocks */
    delay_ms(1U);

    gpio_output(LCD_D4_BASE, LCD_D4_PIN);
    gpio_output(LCD_D5_BASE, LCD_D5_PIN);
    gpio_output(LCD_D6_BASE, LCD_D6_PIN);
    gpio_output(LCD_D7_BASE, LCD_D7_PIN);

    gpio_output(LCD_RS_BASE, LCD_RS_PIN);
    gpio_output(LCD_RW_BASE, LCD_RW_PIN);
    gpio_output(LCD_EN_BASE, LCD_EN_PIN);

    gpio_clear(LCD_D4_BASE, LCD_D4_PIN);
    gpio_clear(LCD_D5_BASE, LCD_D5_PIN);
    gpio_clear(LCD_D6_BASE, LCD_D6_PIN);
    gpio_clear(LCD_D7_BASE, LCD_D7_PIN);

    gpio_clear(LCD_RS_BASE, LCD_RS_PIN);
    gpio_clear(LCD_RW_BASE, LCD_RW_PIN);
    gpio_clear(LCD_EN_BASE, LCD_EN_PIN);

    /*
     * HD44780 initialization sequence for 4-bit mode.
     * This is the important part.
     */
    delay_ms(50U);

    gpio_clear(LCD_RS_BASE, LCD_RS_PIN);
    gpio_clear(LCD_RW_BASE, LCD_RW_PIN);

    lcd_write_nibble(0x03U);
    delay_ms(5U);

    lcd_write_nibble(0x03U);
    delay_ms(5U);

    lcd_write_nibble(0x03U);
    delay_us(200U);

    lcd_write_nibble(0x02U);
    delay_us(200U);

    /*
     * Now LCD is in 4-bit mode.
     */
    lcd_command(0x28U);   /* 4-bit, 2 lines, 5x8 font */
    lcd_command(0x08U);   /* display off */
    lcd_command(0x01U);   /* clear display */
    lcd_command(0x06U);   /* entry mode: increment cursor */
    lcd_command(0x0CU);   /* display on, cursor off, blink off */
}

void lcd_clear(void)
{
    lcd_command(0x01U);
}

void lcd_home(void)
{
    lcd_command(0x02U);
}

void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0U)
    {
        address = 0x00U + col;
    }
    else
    {
        address = 0x40U + col;
    }

    lcd_command(0x80U | address);
}

void lcd_putc(char c)
{
    lcd_data((uint8_t)c);
}

void lcd_print(const char *str)
{
    if (str == NULL)
    {
        return;
    }

    while (*str != '\0')
    {
        lcd_putc(*str);
        str++;
    }
}

void lcd_putuint64(uint64_t n)
{
    char buffer[21];
    int i = 20;

    buffer[i] = '\0';

    if (n == 0)
    {
        lcd_putc('0');
        return;
    }

    while (n > 0)
    {
        i--;
        buffer[i] = (char)('0' + (n % 10U));
        n /= 10U;
    }

    lcd_print(&buffer[i]);
}

void lcd_clear_top(void)
{
    lcd_set_cursor(0, 0);

    for (int i = 0; i < 16; i++)
    {
        lcd_putc(' ');
    }

    lcd_set_cursor(0, 0);
}

void lcd_clear_bottom(void)
{
    lcd_set_cursor(1, 0);

    for (int i = 0; i < 16; i++)
    {
        lcd_putc(' ');
    }

    lcd_set_cursor(1, 0);
}
