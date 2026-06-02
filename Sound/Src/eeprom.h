#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

/*
 * eeprom.h
 *
 * Bare-metal I2C driver for a standard 24-series EEPROM
 * (e.g. AT24C32, 24LC256, M24C64 …).
 *
 * Hardware connections (I2C1, no HAL):
 *   SCL  = PB8   (I2C1_SCL, AF4)
 *   SDA  = PB9   (I2C1_SDA, AF4)
 *   A0/A1/A2  all tied to GND  => 7-bit device address 0x50
 *   WP        tied to GND      => write always enabled
 *
 * The driver exposes a simple byte-at-a-time read/write API and a
 * higher-level "note sequence" storage API used by the recorder.
 *
 * Note sequence storage layout in EEPROM:
 *   Byte 0       : sequence length  N   (0–MAX_SEQUENCE_LENGTH)
 *   Bytes 1–N    : key characters ('1'–'8')
 *
 * MAX_SEQUENCE_LENGTH is defined as 128 to keep things simple.
 */

#define EEPROM_I2C_ADDR         0x50U   /* A0=A1=A2=0 */
#define MAX_SEQUENCE_LENGTH     128U

/* ---------- low-level API ----------------------------------------- */

/* Initialise I2C1 peripheral and PB8/PB9 GPIO. */
void EEPROM_Init(void);

/*
 * Write a single byte to the EEPROM at the given 16-bit address.
 * Includes the mandatory page-write delay (5 ms) afterwards.
 * Returns 0 on success, -1 on timeout/error.
 */
int EEPROM_WriteByte(uint16_t mem_addr, uint8_t data);

/*
 * Read a single byte from the EEPROM at the given 16-bit address.
 * Returns the byte value (0–255) on success, -1 on timeout/error.
 */
int EEPROM_ReadByte(uint16_t mem_addr);

/* ---------- high-level sequence API ------------------------------- */

/*
 * Erase the stored sequence (write length = 0) and reset the
 * in-RAM recording buffer.  Call this when entering Record Mode.
 */
void EEPROM_ClearSequence(void);

/*
 * Append key character k to the sequence in EEPROM.
 * Does nothing if MAX_SEQUENCE_LENGTH has been reached.
 */
void EEPROM_AppendKey(char k);

/*
 * Read back the stored sequence into the provided buffer.
 * Returns the number of keys in the sequence (0 if none recorded).
 * buf must be at least MAX_SEQUENCE_LENGTH bytes long.
 */
uint8_t EEPROM_ReadSequence(char *buf);

#endif /* EEPROM_H */
