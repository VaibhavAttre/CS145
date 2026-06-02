/*
 * eeprom.c  –  fixed version
 *
 * Key changes vs original:
 *   1. EEPROM_WriteByte now uses ACK-polling instead of a fixed 5 ms delay.
 *      After the STOP the driver repeatedly probes the EEPROM with a 1-byte
 *      write until it gets an ACK, which is the correct "write-cycle done"
 *      handshake defined in every 24Cxx datasheet.  This is both faster and
 *      more reliable than a blind delay.
 *
 *   2. A small I2C bus-recovery delay (≈100 µs) is inserted after every
 *      transaction (write OR read) before returning.  On the STM32H5 the
 *      BUSY flag can remain asserted for a short window after STOPF is
 *      cleared; without this gap the BUSY-wait at the start of the very
 *      next call times out and that call silently returns -1.  This was
 *      the direct cause of the length byte (EEPROM[0]) never being updated
 *      past 1, so playback only ever replayed the first recorded key.
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
#define GPIO_AFRH(b)    (*(volatile uint32_t *)((b) + 0x24))

#define RCC_AHB2ENR     (*(volatile uint32_t *)0x44020C8CUL)
#define RCC_APB1LENR    (*(volatile uint32_t *)0x44020C9CUL)

/* ------------------------------------------------------------------ */
/* I2C1 register map                                                   */
/* ------------------------------------------------------------------ */
#define I2C1_BASE       0x40005400UL
#define I2C1_CR1        (*(volatile uint32_t *)(I2C1_BASE + 0x00))
#define I2C1_CR2        (*(volatile uint32_t *)(I2C1_BASE + 0x04))
#define I2C1_TIMINGR    (*(volatile uint32_t *)(I2C1_BASE + 0x10))
#define I2C1_ISR        (*(volatile uint32_t *)(I2C1_BASE + 0x18))
#define I2C1_ICR        (*(volatile uint32_t *)(I2C1_BASE + 0x1C))
#define I2C1_TXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x28))
#define I2C1_RXDR       (*(volatile uint32_t *)(I2C1_BASE + 0x24))

#define I2C_ISR_TXIS    (1U <<  1)
#define I2C_ISR_RXNE    (1U <<  2)
#define I2C_ISR_NACKF   (1U <<  4)
#define I2C_ISR_STOPF   (1U <<  5)
#define I2C_ISR_TC      (1U <<  6)
#define I2C_ISR_BUSY    (1U << 15)

#define I2C_CR2_SADD_SHIFT   1
#define I2C_CR2_NBYTES_SHIFT 16
#define I2C_CR2_RD_WRN       (1U << 10)
#define I2C_CR2_AUTOEND      (1U << 25)
#define I2C_CR2_START        (1U << 13)

#define I2C_TIMING_100KHZ   0xF0420F13UL

/* ------------------------------------------------------------------ */
/* Timing constants                                                    */
/* ------------------------------------------------------------------ */
/*
 * FIX 1 – bus recovery gap.
 * After STOPF is cleared the STM32H5 I2C BUSY bit can stay set for a
 * short time while the peripheral finishes driving the bus lines.
 * 200 µs at 64 MHz is conservative and costs almost nothing.
 */
#define I2C_BUS_RECOVERY_LOOPS  12800UL   /* ~200 µs @ 64 MHz */

/*
 * FIX 2 – ACK-poll inter-probe delay.
 * Between consecutive probes during write-cycle polling we wait ~1 ms
 * so we don't hammer the bus.  Most 24Cxx EEPROMs complete in ≤5 ms.
 */
#define ACK_POLL_DELAY_LOOPS    64000UL   /* ~1 ms @ 64 MHz */
#define ACK_POLL_MAX_TRIES      20        /* up to 20 ms total */

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static void i2c_delay(volatile uint32_t n)
{
    while (n--) __asm__("nop");
}

static int i2c_wait_flag(uint32_t flag, uint32_t timeout)
{
    while (!(I2C1_ISR & flag)) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

/*
 * FIX 2 – ACK polling.
 * Probe the EEPROM with a zero-byte write (just device address).
 * The EEPROM NACKs while its internal write cycle is in progress and
 * ACKs when it is done.  Returns 0 when ACK received, -1 on timeout.
 */
static int eeprom_wait_ready(void)
{
    for (int attempt = 0; attempt < ACK_POLL_MAX_TRIES; attempt++) {
        /* Small gap before each probe */
        i2c_delay(ACK_POLL_DELAY_LOOPS);

        /* Wait for bus to be free */
        uint32_t t = 50000;
        while ((I2C1_ISR & I2C_ISR_BUSY) && --t);
        if (t == 0) continue;

        I2C1_ICR = 0xFFU;

        /*
         * Send START + device address + 0 data bytes with AUTOEND.
         * If EEPROM is ready it will ACK the address and we get STOPF.
         * If still busy internally it will NACK and we get NACKF.
         */
        I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
                 | (0UL << I2C_CR2_NBYTES_SHIFT)
                 | I2C_CR2_AUTOEND
                 | I2C_CR2_START;

        /* Wait for either STOPF (ACK = ready) or NACKF (busy) */
        uint32_t deadline = 200000;
        while (!((I2C1_ISR & I2C_ISR_STOPF) || (I2C1_ISR & I2C_ISR_NACKF))) {
            if (--deadline == 0) break;
        }

        if (I2C1_ISR & I2C_ISR_STOPF) {
            /* EEPROM acknowledged – it is ready */
            I2C1_ICR = 0xFFU;
            i2c_delay(I2C_BUS_RECOVERY_LOOPS);
            return 0;
        }

        /* NACK or timeout – clear flags and try again */
        I2C1_ICR = 0xFFU;
        i2c_delay(I2C_BUS_RECOVERY_LOOPS);
    }
    return -1;  /* never became ready */
}

/* ------------------------------------------------------------------ */
/* Public: EEPROM_Init                                                 */
/* ------------------------------------------------------------------ */
void EEPROM_Init(void)
{
    RCC_AHB2ENR  |= (1U << 1);
    RCC_APB1LENR |= (1U << 21);

    GPIO_MODER(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));
    GPIO_MODER(GPIOB_BASE) |=  ((2UL << (8*2)) | (2UL << (9*2)));
    GPIO_OTYPER(GPIOB_BASE) |= (1UL << 8) | (1UL << 9);
    GPIO_OSPEEDR(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));
    GPIO_OSPEEDR(GPIOB_BASE) |=  ((1UL << (8*2)) | (1UL << (9*2)));
    GPIO_PUPDR(GPIOB_BASE) &= ~((3UL << (8*2)) | (3UL << (9*2)));
    GPIO_AFRH(GPIOB_BASE) &= ~(0xFFUL);
    GPIO_AFRH(GPIOB_BASE) |=  (4UL << 0) | (4UL << 4);

    I2C1_CR1    &= ~(1U << 0);
    I2C1_TIMINGR = I2C_TIMING_100KHZ;
    I2C1_CR1    |=  (1U << 0);
}

/* ------------------------------------------------------------------ */
/* Low-level write/read                                                */
/* ------------------------------------------------------------------ */
int EEPROM_WriteByte(uint16_t mem_addr, uint8_t data)
{
    /* FIX 1: wait for bus to be fully free before starting */
    uint32_t t = 500000;
    while ((I2C1_ISR & I2C_ISR_BUSY) && --t);
    if (t == 0) return -1;

    I2C1_ICR = 0xFFU;

    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (3UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF) return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = data;

    if (i2c_wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);

    /* FIX 1+2: bus recovery gap then ACK-poll until write cycle done */
    i2c_delay(I2C_BUS_RECOVERY_LOOPS);
    return eeprom_wait_ready();
}

int EEPROM_ReadByte(uint16_t mem_addr)
{
    /* FIX 1: extended BUSY timeout */
    uint32_t t = 500000;
    while ((I2C1_ISR & I2C_ISR_BUSY) && --t);
    if (t == 0) return -1;

    I2C1_ICR = 0xFFU;

    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (2UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_START;

    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    if (I2C1_ISR & I2C_ISR_NACKF) return -1;
    I2C1_TXDR = (mem_addr >> 8) & 0xFFU;

    if (i2c_wait_flag(I2C_ISR_TXIS, 100000) < 0) return -1;
    I2C1_TXDR = mem_addr & 0xFFU;

    if (i2c_wait_flag(I2C_ISR_TC, 200000) < 0) return -1;

    I2C1_CR2 = ((uint32_t)(EEPROM_I2C_ADDR) << I2C_CR2_SADD_SHIFT)
             | (1UL << I2C_CR2_NBYTES_SHIFT)
             | I2C_CR2_RD_WRN
             | I2C_CR2_AUTOEND
             | I2C_CR2_START;

    if (i2c_wait_flag(I2C_ISR_RXNE, 200000) < 0) return -1;
    uint8_t rx = (uint8_t)(I2C1_RXDR & 0xFFU);

    if (i2c_wait_flag(I2C_ISR_STOPF, 200000) < 0) return -1;
    I2C1_ICR = (1U << 5);

    /* FIX 1: bus recovery after read too */
    i2c_delay(I2C_BUS_RECOVERY_LOOPS);

    return (int)rx;
}

/* ------------------------------------------------------------------ */
/* High-level sequence API                                             */
/* ------------------------------------------------------------------ */
static uint8_t seq_len = 0xFF;

void EEPROM_ClearSequence(void)
{
    seq_len = 0;
    EEPROM_WriteByte(0, 0);
}

void EEPROM_AppendKey(char k)
{
    if (seq_len == 0xFF) {
        int v = EEPROM_ReadByte(0);
        seq_len = (v < 0) ? 0 : (uint8_t)v;
    }

    if (seq_len >= MAX_SEQUENCE_LENGTH) return;

    EEPROM_WriteByte((uint16_t)(1U + seq_len), (uint8_t)k);
    seq_len++;
    EEPROM_WriteByte(0, seq_len);
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
