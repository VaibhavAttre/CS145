#ifndef BREATHING_LED_H
#define BREATHING_LED_H

#include <stdint.h>

void BREATHING_LED_Init(void);

void BREATHING_LED_Update(uint32_t adc_value);

#endif
