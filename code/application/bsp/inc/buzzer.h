#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdint.h>

void buzzer_set_volume(uint8_t volume);
void buzzer_on(void);
void buzzer_off(void);

#endif