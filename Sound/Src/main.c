/*
 * main.c  –  Tone player / recorder  (Part 2)
 *
 * Modes (one LED lit at a time):
 *   Play Mode     PA6 LED  – press '*'
 *   Record Mode   PC0 LED  – press '0'  (clears EEPROM, then records)
 *   Playback Mode PC3 LED  – press '#'
 *
 * Play / Record:  keys 1-8 play tones.
 *                 In Record Mode each NEW press is saved to EEPROM.
 *                 The tone plays for as long as the key is held.
 *
 * Playback:       press '9' to replay the stored sequence.
 *                 Each note plays for 1 second.
 *
 * Hardware:
 *   Speaker  PA0   TIM2_CH1 PWM
 *   LEDs     PA6, PC0, PC3
 *   Keypad   PB7/6, PG14, PE13 (rows)   PE14/11/9, PG12 (cols)
 *   EEPROM   PB8=SCL  PB9=SDA  (I2C1, 100 kHz)
 */

#include "keypad.h"
#include "pwm_tone.h"
#include "tones.h"
#include "eeprom.h"
#include "mode_leds.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Blocking delay                                                      */
/* 64 MHz / ~3 cycles per loop / 1000 ms                              */
/* ------------------------------------------------------------------ */
#define LOOPS_PER_MS  21333UL

static void delay_ms(uint32_t ms)
{
    volatile uint32_t n = ms * LOOPS_PER_MS;
    while (n--) __asm__("nop");
}

/* ------------------------------------------------------------------ */
/* Wait until no key is pressed (debounced release)                   */
/* ------------------------------------------------------------------ */
static void wait_key_release(void)
{
    /* Keep polling until we see no key for two consecutive reads     */
    int stable = 0;
    while (stable < 2) {
        if (keypad_get_current_key() == 0)
            stable++;
        else
            stable = 0;
        delay_ms(10);
    }
}

/* ------------------------------------------------------------------ */
/* Playback helper                                                     */
/* ------------------------------------------------------------------ */
static void play_sequence(void)
{
    char buf[MAX_SEQUENCE_LENGTH];
    uint8_t n = EEPROM_ReadSequence(buf);

    for (uint8_t i = 0; i < n; i++) {
        uint32_t freq = Tone_Frequency_From_Key(buf[i]);
        if (freq != 0) {
            PWM_Tone_Play(freq);
        }
        delay_ms(1000);
        PWM_Tone_Stop();
        delay_ms(50);       /* brief silent gap between notes */
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    keypad_init();
    PWM_Tone_Init();
    EEPROM_Init();

    ModeLEDs_Set(MODE_PLAY);

    char last_key = 0;

    while (1)
    {
        char key = keypad_get_current_key();
        SystemMode mode = ModeLEDs_GetMode();

        /* ---------------------------------------------------------- */
        /* Mode-switch keys: act on the leading edge only             */
        /* ---------------------------------------------------------- */
        if (key == '*' && key != last_key) {
            PWM_Tone_Stop();
            ModeLEDs_Set(MODE_PLAY);
            last_key = key;
            continue;
        }

        if (key == '0' && key != last_key) {
            PWM_Tone_Stop();
            EEPROM_ClearSequence();
            ModeLEDs_Set(MODE_RECORD);
            last_key = key;
            continue;
        }

        if (key == '#' && key != last_key) {
            PWM_Tone_Stop();
            ModeLEDs_Set(MODE_PLAYBACK);
            last_key = key;
            continue;
        }

        /* ---------------------------------------------------------- */
        /* Playback mode: '9' triggers replay                         */
        /* ---------------------------------------------------------- */
        if (mode == MODE_PLAYBACK) {
            if (key == '9' && key != last_key) {
                play_sequence();
            }
            last_key = key;
            continue;
        }

        /* ---------------------------------------------------------- */
        /* Play / Record modes: keys '1'–'8'                          */
        /* ---------------------------------------------------------- */
        uint32_t freq = Tone_Frequency_From_Key(key);

        if (freq != 0) {
            if (key != last_key) {
                /* New key press detected */
                PWM_Tone_Play(freq);

                if (mode == MODE_RECORD) {
                    /*
                     * Save this key to EEPROM.
                     * EEPROM_AppendKey takes ~10 ms (two I2C writes +
                     * write-cycle delays).  We keep the tone playing
                     * the whole time so it still sounds responsive.
                     * After saving, wait for the key to be physically
                     * released before we will record another press –
                     * this prevents a single long hold from recording
                     * the same key multiple times and also prevents
                     * the EEPROM write delay from blocking the next
                     * key detection.
                     */
                    EEPROM_AppendKey(key);
                    wait_key_release();
                    PWM_Tone_Stop();
                    last_key = 0;   /* reset so next press is fresh */
                    continue;
                }

                last_key = key;
            }
            /* Same key still held in Play mode: keep tone playing */
        } else {
            /* No tone key pressed */
            if (last_key != 0) {
                PWM_Tone_Stop();
            }
            last_key = key;
        }
    }
}
