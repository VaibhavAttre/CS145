#ifndef PWM_TONE_H
#define PWM_TONE_H

#include <stdint.h>


#define PWM_TIMER_CLOCK_HZ 64000000UL

void PWM_Tone_Init(void);
void PWM_Tone_Play(uint32_t frequency_hz);
void PWM_Tone_Stop(void);

#endif
