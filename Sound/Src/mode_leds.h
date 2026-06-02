#ifndef MODE_LEDS_H
#define MODE_LEDS_H

/*
 * mode_leds.h
 *
 * Three GPIO output LEDs, one per system mode.
 *   Play     LED -> PA6
 *   Record   LED -> PC0
 *   Playback LED -> PC3
 *
 * Exactly one LED is lit at any time.
 */

typedef enum {
    MODE_PLAY     = 0,
    MODE_RECORD   = 1,
    MODE_PLAYBACK = 2
} SystemMode;

/* Initialise PA6, PC0, PC3 as outputs (called from keypad_init or main). */
void ModeLEDs_Init(void);

/* Light the LED for the given mode; extinguish the other two. */
void ModeLEDs_Set(SystemMode mode);

/* Return the currently active mode. */
SystemMode ModeLEDs_GetMode(void);

#endif /* MODE_LEDS_H */
