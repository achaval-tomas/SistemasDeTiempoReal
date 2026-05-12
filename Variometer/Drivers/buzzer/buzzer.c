#include "buzzer.h"

#define TIM2_TICKS_PER_SEC 1000000

void buzz_init(){
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

void buzz_start(uint32_t frequencyHZ){
    // Skip invalid input
    if (frequencyHZ == 0) return;

    uint32_t arr, pulse;

    // Calculate ARR from frequency in HZ
    arr = (TIM2_TICKS_PER_SEC / frequencyHZ) - 1;

    // Set pulse to 50% arr for max volume
    pulse = (arr+1) / 2;

    // Set up timer to start PWM output
    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
}

void buzz_stop(){
    // Stop by setting pulse to 0
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
}