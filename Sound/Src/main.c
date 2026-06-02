#include "keypad.h"
#include "pwm_tone.h"
#include "tones.h"
#include "eeprom.h"
#include "mode_leds.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Simple millisecond-scale blocking delay                             */
/* At 64 MHz, roughly 64 000 cycles per ms; each loop = ~3 cycles     */
/* ------------------------------------------------------------------ */
#define LOOPS_PER_MS   1000L

static void delay_ms(uint32_t ms)
{
    volatile uint32_t n = ms * LOOPS_PER_MS;
    while (n--) __asm__("nop");
}

/* ------------------------------------------------------------------ */
/* Playback: play recorded sequence from EEPROM                        */
/* Each note is played for 1 second regardless of original duration.  */
/* ------------------------------------------------------------------ */
static void play_sequence(void)
{
    char buf[MAX_SEQUENCE_LENGTH];
    uint8_t n = EEPROM_ReadSequence(buf);

    for (uint8_t i = 0; i < n; i++) {
        uint32_t freq = Tone_Frequency_From_Key(buf[i]);
        if (freq != 0) {
            PWM_Tone_Play(freq);
            delay_ms(1000);
            PWM_Tone_Stop();
            delay_ms(50);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    keypad_init();          /* configures keypad GPIO + mode LEDs */
    PWM_Tone_Init();        /* configures TIM2 + PA0 */
    EEPROM_Init();      /* configures I2C1 + PB8/PB9 */
    int v = EEPROM_ReadByte(0);
    if (v < 0 || (uint8_t)v > MAX_SEQUENCE_LENGTH) {
        EEPROM_ClearSequence();
    }
    /* Start in Play Mode */
    ModeLEDs_Set(MODE_PLAY);

    char last_key = 0;

    while (1)
    {
        char key = keypad_get_current_key();
        SystemMode mode = ModeLEDs_GetMode();

        /* ---------------------------------------------------------- */
        /* Mode-switch keys ('*', '0', '#')                           */
        /* ---------------------------------------------------------- */
        if (key == '*' && key != last_key) {
            /* Enter Play Mode */
            PWM_Tone_Stop();
            ModeLEDs_Set(MODE_PLAY);
            last_key = key;
            continue;
        }

        if (key == '0' && key != last_key) {
            /* Enter Record Mode – always clears previous recording */
            PWM_Tone_Stop();
            EEPROM_ClearSequence();
            ModeLEDs_Set(MODE_RECORD);
            last_key = key;
            continue;
        }

        if (key == '#' && key != last_key) {
            /* Enter Playback Mode */
            PWM_Tone_Stop();
            ModeLEDs_Set(MODE_PLAYBACK);
            last_key = key;
            continue;
        }

        /* ---------------------------------------------------------- */
        /* Playback Mode: '9' triggers replay                         */
        /* ---------------------------------------------------------- */
        if (mode == MODE_PLAYBACK) {
            if (key == '9' && key != last_key) {
                play_sequence();
            }
            last_key = key;
            continue;
        }

        /* ---------------------------------------------------------- */
        /* Play / Record modes: keys '1'–'8' play tones               */
        /* ---------------------------------------------------------- */
        uint32_t freq = Tone_Frequency_From_Key(key);

        if (freq != 0) {
            /* A tone key is pressed */
            if (key != last_key) {
                PWM_Tone_Play(freq);

                /* In Record Mode, append this key to the sequence */
                if (mode == MODE_RECORD) {
                    EEPROM_AppendKey(key);
                }

                last_key = key;
            }
            /* If same key held, tone keeps playing – do nothing */
        } else {
            /* No tone key pressed */
            if (last_key != 0) {
                PWM_Tone_Stop();
            }
            last_key = key;   /* may be 0 or a non-tone key like 'A' */
        }
    }
}
