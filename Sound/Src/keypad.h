#ifndef KEYPAD_H
#define KEYPAD_H

void keypad_init(void);


char keypad_get_current_key(void);


char keypad_get_key(void);

void keypad_delay_short(void);

#endif
