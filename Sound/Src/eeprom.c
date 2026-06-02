/*
 * eeprom.c
 *
 * Bare-metal I2C1 driver for a 24-series EEPROM (AT24C32 / 24LC256 /
 * M24C64 …) on NUCLEO-H563ZI.
 *
 * Hardware:
 *   SCL = PB8  (I2C1_SCL, AF4)
 *   SDA = PB9  (I2C1_SDA, AF4)
 *   EEPROM I2C address = 0x50 (A0=A1=A2=GND)
 *
 * The STM32H563 I2C peripheral uses the "NBYTES / RELOAD / AUTOEND"
 * transfer-control model introduced with the STM32F0/L0 generation.
 * This driver uses polling mode (no interrupts, no DMA).
 *
 * Note sequence layout:
 *   EEPROM[0]       = N  (number of keys stored, 0–128)
 *   EEPROM[1..N]    = key characters '1'–'8'
 */

#include "eeprom.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* GPIO & RCC register macros                                          */
/* ------------------------------------------------------------------ */
#define GPIOB_BASE      0x42020400UL
#define GPIO_MODER(b)   (*(volatile uint32_t *)((b) + 0x00))
#define GPIO_OTYPER(b)  (*(volatile uint32_t *)((b) + 0x04))
#define GPIO_OSPEEDR(b) (*(volatile uint32_t *)((b) + 0x08))
#define GPIO_PUPDR(b)   (*(volatile uint32_t *)((b) + 0x0C))
#define GPIO_AFRH(b)    (*(volatile uint32_t *)((b) + 0x24))  /* pins 8-15 */

#define RCC_AHB2ENR     (*(volatile uint32_t *)0x44020C8CUL)  /* GPIOBEN bit1 */
#define RCC_APB1LENR    (*(volatile uint32_t *)0x44020C9CUL)  /* I2C1EN  bit21 */

/* ------------------------------------------------------------------ */
/* I2C1 register map (STM32H563 RM, chapter "I2C")                    */
/* ------------------------------------------------------------------ */
#define I2C1_BASE       0x40005400UL
#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_TIMINGR    (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_ISR        (*(volatile uint32_t *)(I2C1_BASE + 0x18))
#define I2C1_ICR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_TXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x28))
#define I2C1_RXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x24))

/* ISR bits */
#define I2C_ISR_TXE     (1U <<  0)
#define I2C_ISR_TXIS    (1U <<  1)
#define I2C_ISR_RXNE    (1U <<  2)
#define I2C_ISR_TC      (1U <<  6)
#define I2C_ISR_TCR     (1U <<  7)
#define I2C_ISR_BUSY    (1U << 15)
#define I2C_ISR_NACKF   (1U <<  4)
#define I2C_ISR_STOPF   (1U <<  5)

/* CR2 bits / fields */
#define I2C_CR2_SADD_SHIFT   1
#define I2C_CR2_NBYTES_SHIFT 16
#define I2C_CR2_RD_WRN       (1U << 10)
#define I2C_CR2_AUTOEND      (1U << 25)
#define I2C_CR2_RELOAD       (1U << 24)
#define I2C_CR2_START        (1U << 13)
#define I2C_CR2_STOP         (1U << 14)

/* ------------------------------------------------------------------ */
/* Timing register value for 100 kHz I2C @ 64 MHz I2CCLK              */
/* Generated with STM32CubeMX I2C timing calculator:                  */
/*   PRESC=15, SCLL=0x13, SCLH=0x0F, SDADEL=0x2, SCLDEL=0x4          */
/*   => 0x F0 4 2 0F 13  (PRESC | SCLDEL | SDADEL | SCLH | SCLL)      */
/* Encoded as: PRESC[31:28] SCLDEL[23:20] SDADEL[19:16] SCLH[15:8] SCLL[7:0] */
#define I2C_TIMING_100KHZ   0xF0420F13UL

/* EEPROM write-cycle time: 5 ms (most 24Cxx EEPROMs) */
#define EEPROM_WRITE_DELAY_LOOPS  320000UL   /* ~5 ms @ 64 MHz */

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static void i2c_delay(volatile uint32_t n)
{
    while (n--) __asm__("nop");
}

/* Wait for a bit in ISR; return 0 on success, -1 on timeout */
static int i2c_wait_flag(uint32_t flag, uint32_t timeout)
{
    while (!(I2C1_ISR & flag)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public: EEPROM_Init                                                 */
/* ------------------------------------------------------------------ */
void EEPROM_Init(void)
{
    /* 1. Enable clocks for GPIOB and I2C1 */
    RCC_AHB2ENR  |= (1U << 1);   /* GPIOBEN */
    RCC_APB1LENR |= (1U << 21);  /* I2C1EN  */

    /* 2. Configure PB8 (SCL) and PB9 (SDA) for AF4 open-drain         */
    /*    MODER = 10 (alternate function)                               */
    GPIO_MODER(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));
    GPIO_MODER(GPIOB_BASE) |=  ((2UL << (8*2)) | (2UL << (9*2)));

    /*    OTYPER = 1 (open-drain) for both                              */
    GPIO_OTYPER(GPIOB_BASE) |= (1UL << 8) | (1UL << 9);

    /*    OSPEEDR = 01 (medium speed) for both                          */
    GPIO_OSPEEDR(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));
    GPIO_OSPEEDR(GPIOB_BASE) |=  ((1UL << (8*2)) | (1UL << (9*2)));

    /*    PUPDR = 00 (no pull; external 4.7k pull-ups on the I2C bus)   */
    GPIO_PUPDR(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));

    /*    AFRH: pins 8 & 9 use AF4 (I2C1)                               */
    /*    AFRH[3:0]  = AF for pin 8 (bits [3:0] of AFRH)                */
    /*    AFRH[7:4]  = AF for pin 9 (bits [7:4] of AFRH)                */
    GPIO_AFRH(GPIOB_BASE) &= ~(0xFFUL);             /* clear bits 0..7 */
    GPIO_AFRH(GPIOB_BASE) |=  (4UL << 0) | (4UL << 4);  /* AF4 for PB8, PB9 */

    /* 3. Configure I2C1 peripheral                                     */
    I2C1_CR1    &= ~(1U << 0);         /* PE=0: disable while configuring */
    I2C1_TIMINGR = I2C_TIMING_100KHZ;
    I2C1_CR1    |=  (1U << 0);         /* PE=1: enable */
}

/* ------------------------------------------------------------------ */
/* Low-level write/read primitives                                     */
/* ------------------------------------------------------------------ */

/*
 * Write one byte to EEPROM at mem_addr.
 * Protocol:  START | DEV_ADDR+W | MEM_ADDR_HI | MEM_ADDR_LO | DATA | STOP
 * Returns 0 on success, -1 on error.
 */
int EEPROM_WriteByte(uint16_t mem_addr, uint8_t data)
{
    /* Wait if bus is busy */
    uint32_t t = 100000;
    while ((I2C1_ISR & I2C_ISR_BUSY) && --t);
    if (t == 0) return -1;

    /* --- Phase 1: send device address + 3 bytes (addr_hi, addr_lo, data) --- */
    I2C1_ICR = 0xFFU;   /* clear all flags */

    /* CR2: SADD(7-bit shifted to [7:1]), NBYTES=3, WRITE, AUTOEND, START */
    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (3UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    /* Send memory address high byte */
    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF) return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    /* Send memory address low byte */
    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    /* Send data byte */
    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = data;

    /* Wait for STOP (AUTOEND generates it automatically) */
    if (i2c_wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);   /* clear STOPF */

    /* EEPROM internal write cycle: wait ~5 ms */
    i2c_delay(EEPROM_WRITE_DELAY_LOOPS);

    return 0;
}

/*
 * Read one byte from EEPROM at mem_addr.
 * Protocol:  START | DEV_ADDR+W | ADDR_HI | ADDR_LO |
 *            RESTART | DEV_ADDR+R | DATA | NACK | STOP
 * Returns the byte value (0-255) or -1 on error.
 */
int EEPROM_ReadByte(uint16_t mem_addr)
{
    /* Wait until bus free */
    uint32_t t = 100000;
    while ((I2C1_ISR & I2C_ISR_BUSY) && --t);
    if (t == 0) return -1;

    I2C1_ICR = 0xFFU;

    /* --- Phase 1: write the 16-bit memory address (no AUTOEND) --- */
    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (2UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_START;          /* no AUTOEND – we do a restart */

    /* addr high */
    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF) return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    /* addr low */
    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    /* Wait for TC (transfer complete, no stop yet) */
    if (i2c_wait_flag(I2C_ISR_TC, 200000) < 0) return -1;

    /* --- Phase 2: repeated START, read 1 byte with AUTOEND --- */
    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (1UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_RD_WRN
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    /* Wait for receive data */
    if (i2c_wait_flag(I2C_ISR_RXNE, 200000) < 0) return -1;
    uint8_t rx = (uint8_t)(I2C1_RXDR & 0xFFU);

    /* Wait for STOP */
    if (i2c_wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);   /* clear STOPF */

    return (int)rx;
}

/* ------------------------------------------------------------------ */
/* High-level sequence API                                             */
/* ------------------------------------------------------------------ */

/* Cached sequence length kept in RAM for fast appending */
static uint8_t seq_len = 0xFF;  /* 0xFF = "not yet read from EEPROM" */

void EEPROM_ClearSequence(void)
{
    EEPROM_WriteByte(0, 0);   /* store length = 0 */
    seq_len = 0;
}

void EEPROM_AppendKey(char k)
{
    /* Load length from EEPROM on first call after power-up */
    if (seq_len == 0xFF) {
        int v = EEPROM_ReadByte(0);
        seq_len = (v < 0) ? 0 : (uint8_t)v;
    }

    if (seq_len >= MAX_SEQUENCE_LENGTH) return;   /* buffer full */

    /* Store key at position (seq_len + 1); index 0 is the length byte */
    EEPROM_WriteByte((uint16_t)(1U + seq_len), (uint8_t)k);
    seq_len++;
    EEPROM_WriteByte(0, seq_len);   /* update length byte */
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

    seq_len = n;    /* keep cache in sync */
    return n;
}
