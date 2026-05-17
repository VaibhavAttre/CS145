#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>


void keypad_init(void);

char keypad_scan(void);

void keypad_loop(void);

#endif
