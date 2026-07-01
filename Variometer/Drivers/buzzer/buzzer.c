#include "buzzer.h"

#define TIM2_TICKS_PER_SEC 1000000

void buzz_init(){
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
}

// Low-level trigger pulse percentage of arr, for each volume level from 0 to 5
const float VOLUME_FRACTIONS[6] = {1.00f, 0.98f, 0.94f, 0.88f, 0.75f, 0.50f};

// Volume should be a value from 0 to 5, where 0 is silent and 5 is max volume
void buzz_start(uint32_t frequencyHZ, uint8_t volume) {
    // Skip invalid input
    if (frequencyHZ == 0) return;
    if (volume > 5) volume = 5;

    uint32_t arr, pulse;

    // Calculate ARR from frequency in HZ
    arr = (TIM2_TICKS_PER_SEC / frequencyHZ) - 1;

    // Adjust pulse according to volume
    pulse = (uint32_t)((arr + 1) * VOLUME_FRACTIONS[volume]);

    // Set up timer to start PWM output
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
}

void buzz_stop(){
    uint32_t current_arr = __HAL_TIM_GET_AUTORELOAD(&htim2);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, current_arr + 1);
}