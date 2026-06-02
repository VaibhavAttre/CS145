/*
 * eeprom.c
 *
 * Bare-metal I2C1 driver for 24LC32A on NUCLEO-H563ZI.
 *   SCL = PB8  (I2C1_SCL, AF4)
 *   SDA = PB9  (I2C1_SDA, AF4)
 *   Device address = 0x50 (A0=A1=A2=GND)
 *
 * Yes – this uses I2C as the communication protocol between the MCU
 * and the EEPROM.  The STM32H563 I2C peripheral uses the
 * NBYTES/AUTOEND/RELOAD transfer model (same as STM32F0/L0/G0).
 * This driver is pure polling – no interrupts, no DMA.
 *
 * EEPROM memory layout:
 *   Address 0       : sequence length N (0–128)
 *   Addresses 1..N  : key characters '1'–'8'
 */

#include "eeprom.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* GPIO / RCC registers                                                */
/* ------------------------------------------------------------------ */
#define GPIOB_BASE       0x42020400UL
#define GPIO_MODER(b)    (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_OTYPER(b)   (*(volatile uint32_t *)((b) + 0x04))
#define GPIO_OSPEEDR(b)  (*(volatile uint32_t *)((b) + 0x08))
#define GPIO_PUPDR(b)    (*(volatile uint32_t *)((b) + 0x0C))
#define GPIO_AFRH(b)     (*(volatile uint32_t *)((b) + 0x24))

#define RCC_AHB2ENR      (*(volatile uint32_t *)0x44020C8CUL)
#define RCC_APB1LENR     (*(volatile uint32_t *)0x44020C9CUL)

/* ------------------------------------------------------------------ */
/* I2C1 registers                                                      */
/* ------------------------------------------------------------------ */
#define I2C1_BASE        0x40005400UL
#define I2C1_CR1         (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2         (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_TIMINGR     (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_ISR         (*(volatile uint32_t *)(I2C1_BASE + 0x18))
#define I2C1_ICR         (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_RXDR        (*(volatile uint32_t *)(I2C1_BASE + 0x24))
#define I2C1_TXDR        (*(volatile uint32_t *)(I2C1_BASE + 0x28))

/* ISR flag bits */
#define I2C_ISR_TXIS     (1U <<  1)
#define I2C_ISR_RXNE     (1U <<  2)
#define I2C_ISR_NACKF    (1U <<  4)
#define I2C_ISR_STOPF    (1U <<  5)
#define I2C_ISR_TC       (1U <<  6)
#define I2C_ISR_BUSY     (1U << 15)

/* CR2 field helpers */
#define I2C_CR2_SADD_SHIFT    1U
#define I2C_CR2_NBYTES_SHIFT  16U
#define I2C_CR2_RD_WRN        (1U << 10)
#define I2C_CR2_AUTOEND       (1U << 25)
#define I2C_CR2_START         (1U << 13)

/*
 * TIMINGR for 100 kHz I2C at 64 MHz I2CCLK (PCLK1 on NUCLEO-H563ZI).
 * PRESC=15, SCLDEL=4, SDADEL=2, SCLH=0x0F, SCLL=0x13
 */
#define I2C_TIMING_100KHZ    0xF0420F13UL

/* 24LC32A write-cycle time is 5 ms max.  At 64 MHz, ~320 000 nop loops. */
#define EEPROM_WRITE_DELAY   320000UL

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static void i2c_delay(volatile uint32_t n)
{
    while (n--) __asm__("nop");
}

/* Wait until ISR bit 'flag' is set; return 0 on success, -1 on timeout. */
static int wait_flag(uint32_t flag, uint32_t timeout)
{
    while (!(I2C1_ISR & flag)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

/* Wait until BUSY bit clears; return 0 on success, -1 on timeout. */
static int wait_not_busy(void)
{
    uint32_t t = 200000;
    while (I2C1_ISR & I2C_ISR_BUSY) {
        if (--t == 0) return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* EEPROM_Init                                                         */
/* ------------------------------------------------------------------ */
void EEPROM_Init(void)
{
    /* Enable GPIOB and I2C1 clocks */
    RCC_AHB2ENR  |= (1U << 1);   /* GPIOBEN */
    RCC_APB1LENR |= (1U << 21);  /* I2C1EN  */

    /* PB8 = SCL, PB9 = SDA: AF4, open-drain, medium speed, no pull */
    GPIO_MODER(GPIOB_BASE)   &= ~((3UL << 16) | (3UL << 18));
    GPIO_MODER(GPIOB_BASE)   |=  ((2UL << 16) | (2UL << 18)); /* AF mode */
    GPIO_OTYPER(GPIOB_BASE)  |=  (1U << 8) | (1U << 9);       /* open-drain */
    GPIO_OSPEEDR(GPIOB_BASE) &= ~((3UL << 16) | (3UL << 18));
    GPIO_OSPEEDR(GPIOB_BASE) |=  ((1UL << 16) | (1UL << 18)); /* medium speed */
    GPIO_PUPDR(GPIOB_BASE)   &= ~((3UL << 16) | (3UL << 18)); /* no pull */

    /* AFRH bits [3:0] = PB8 AF, bits [7:4] = PB9 AF  (both = 4) */
    GPIO_AFRH(GPIOB_BASE) &= ~(0xFFUL);
    GPIO_AFRH(GPIOB_BASE) |=  (4UL << 0) | (4UL << 4);

    /* Configure I2C1 */
    I2C1_CR1    &= ~(1U << 0);          /* disable PE before configuring */
    I2C1_TIMINGR = I2C_TIMING_100KHZ;
    I2C1_CR1    |=  (1U << 0);          /* enable PE */
}

/* ------------------------------------------------------------------ */
/* EEPROM_WriteByte                                                    */
/* ------------------------------------------------------------------ */
int EEPROM_WriteByte(uint16_t mem_addr, uint8_t data)
{
    /* Wait until bus is free */
    if (wait_not_busy() < 0) return -1;

    /* Clear all status flags before starting */
    I2C1_ICR = 0xFFU;

    /*
     * Send: START | ADDR+W | mem_addr_hi | mem_addr_lo | data | STOP
     * NBYTES = 3 (two address bytes + one data byte), AUTOEND
     */
    I2C1_CR2 = ((uint32_t)EEPROM_I2C_ADDR << I2C_CR2_SADD_SHIFT)
             | (3UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    /* Memory address high byte */
    if (wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF)            return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    /* Memory address low byte */
    if (wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    /* Data byte */
    if (wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = data;

    /* Wait for the hardware to generate STOP (AUTOEND) */
    if (wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);   /* clear STOPF */

    /* 24LC32A needs up to 5 ms for the internal write cycle */
    i2c_delay(EEPROM_WRITE_DELAY);

    return 0;
}

/* ------------------------------------------------------------------ */
/* EEPROM_ReadByte                                                     */
/* ------------------------------------------------------------------ */
int EEPROM_ReadByte(uint16_t mem_addr)
{
    /* Wait until bus is free */
    if (wait_not_busy() < 0) return -1;

    I2C1_ICR = 0xFFU;

    /*
     * Phase 1 – write the 16-bit memory address (no AUTOEND so we can
     * issue a repeated START after).
     * NBYTES = 2, no AUTOEND, no RD_WRN.
     */
    I2C1_CR2 = ((uint32_t)EEPROM_I2C_ADDR << I2C_CR2_SADD_SHIFT)
             | (2UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_START;

    if (wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF)            return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    if (wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    /* Wait for TC – address phase complete, bus still held */
    if (wait_flag(I2C_ISR_TC, 200000) < 0) return -1;

    /*
     * Phase 2 – repeated START, read 1 byte, AUTOEND generates STOP.
     */
    I2C1_CR2 = ((uint32_t)EEPROM_I2C_ADDR << I2C_CR2_SADD_SHIFT)
             | (1UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_RD_WRN
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    if (wait_flag(I2C_ISR_RXNE,  200000) < 0) return -1;
    uint8_t rx = (uint8_t)(I2C1_RXDR & 0xFFU);

    if (wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);   /* clear STOPF */

    return (int)rx;
}

/* ------------------------------------------------------------------ */
/* High-level sequence API                                             */
/* ------------------------------------------------------------------ */

static uint8_t seq_len = 0xFF;   /* 0xFF = not yet loaded from EEPROM */

void EEPROM_ClearSequence(void)
{
    EEPROM_WriteByte(0, 0);
    seq_len = 0;
}

void EEPROM_AppendKey(char k)
{
    /* First call after power-up: read the current length from EEPROM */
    if (seq_len == 0xFF) {
        int v = EEPROM_ReadByte(0);
        seq_len = (v < 0) ? 0 : (uint8_t)v;
    }

    if (seq_len >= MAX_SEQUENCE_LENGTH) return;

    EEPROM_WriteByte((uint16_t)(1U + seq_len), (uint8_t)k);
    seq_len++;
    EEPROM_WriteByte(0, seq_len);   /* update the length byte */
}

uint8_t EEPROM_ReadSequence(char *buf)
{
    int v = EEPROM_ReadByte(0);
    if (v < 0) return 0;
    uint8_t n = (uint8_t)v;
    if (n > MAX_SEQUENCE_LENGTH) n = MAX_SEQUENCE_LENGTH;

    for (uint8_t i = 0; i < n; i++) {
        int b = EEPROM_ReadByte((uint16_t)(1U + i));
        buf[i] = (b < 0) ? '1' : (char)b;
    }

    seq_len = n;
    return n;
}
