#ifndef BUZZER_H
#define BUZZER_H

#include "tim.h"
#include <stdint.h>

// ASSUMES TIM2 CONFIGURED FOR PWM AT 1 000 000 TICKS/SEC
void buzz_init();

void buzz_start(uint32_t frequencyHZ);
void buzz_stop();

#endif // BUZZER_H