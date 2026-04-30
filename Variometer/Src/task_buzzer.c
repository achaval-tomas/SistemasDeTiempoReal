#include "my_tasks.h"
#include "tim.h"
#include <stdint.h>

#define TIM2_TICKS_PER_SEC 1000000

uint32_t get_buzz_frequency(float climb_rate) {
    uint32_t freq = 0; // no freq if thresholds are not exceeded
    float delta_rate = 0.0f;

    // Calculate lift/sink frequency adjusted by distance from thresholds
    if (climb_rate >= varioConfig.lift_threshold) {

        delta_rate = climb_rate - varioConfig.lift_threshold;
        freq = (uint32_t)(varioConfig.lift_hz_base + (delta_rate * varioConfig.lift_hz_scale));

    } else if (climb_rate <= varioConfig.sink_threshold) {

        delta_rate = varioConfig.sink_threshold - climb_rate;
        freq = (uint32_t)(varioConfig.sink_hz_base + (delta_rate * varioConfig.sink_hz_scale));
        
        // Clamp to minimum frequency
        if (freq < (uint32_t)varioConfig.sink_hz_min) {
            freq = (uint32_t)varioConfig.sink_hz_min;
        }
    }

    return freq;
}

uint32_t get_buzz_duration(float climb_rate) {
    uint32_t duration = 0; // no duration if thresholds are not exceeded

    if (climb_rate >= varioConfig.lift_threshold) {
        duration = 400 - (uint32_t)((climb_rate - varioConfig.lift_threshold) * 100);
        duration = (duration < 50) ? 50 : duration;
    } else if (climb_rate <= varioConfig.sink_threshold) {
        duration = 500;
    }

    return duration;
}

uint32_t get_buzz_silence(float climb_rate){
    uint32_t silence_ms = 0; // skip beep-to-beep delay when there is no beep

    if (climb_rate >= varioConfig.lift_threshold) {
        // increasingly shorter delays during steep climbs
        silence_ms = 50 - (uint32_t)((climb_rate-varioConfig.lift_threshold) * 10);
        silence_ms = (silence_ms < 10) ? 10 : silence_ms;
    } else if (climb_rate <= varioConfig.sink_threshold) {
        silence_ms = 100;
    }

    return silence_ms;
}

void Buzzer(buzzerParams_td buzzData){
  // Skip invalid inputs
  if (buzzData.frequencyHZ == 0 || buzzData.durationMS == 0) return;

  uint32_t arr, pulse;

  // Calculate ARR from frequency in HZ
  arr = (TIM2_TICKS_PER_SEC / buzzData.frequencyHZ) - 1;

  // Set pulse to 50% arr for max volume
  pulse = (arr+1) / 2;

  // Set up timer for PWM output
  __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);

  // Beep at the specified frequency and duration
  vTaskDelay(pdMS_TO_TICKS(buzzData.durationMS));

  // Stop the buzzer by setting pulse to 0
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
}

void PlayStartupTune(){
  buzzerParams_td buzzData = {0};

  for (int i = 0; i < 3; i++){
    buzzData.frequencyHZ = 500 + i*200; // Ascending frequencies
    buzzData.durationMS = 250 - i*50; // Decreasing duration

    Buzzer(buzzData);
    if (i == 2) break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void PlaySwitchOffTune(){
  buzzerParams_td buzzData = {0};

  for (int i = 0; i < 4; i++){
    buzzData.frequencyHZ = 1000 - i*200; // Descending frequencies
    buzzData.durationMS = 100 + i*10; // Increasing duration

    Buzzer(buzzData);
    if (i == 3) break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void BuzzerTask(void *pvParameters){
  // Start PWM signal at 0% "volume"
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  buzzerQueueData_td bqData;
  buzzerParams_td buzzParams;

  uint32_t silenceMS = 0;

  while (1){
    xQueueReceive(buzzerQueue, (void *)&bqData, portMAX_DELAY);

    switch(bqData.type){
      case BUZZ_VARIO:
        // Beep according to climb rate
        buzzParams.frequencyHZ = get_buzz_frequency(bqData.vario_climb_rate);
        buzzParams.durationMS = get_buzz_duration(bqData.vario_climb_rate);
        
        Buzzer(buzzParams);

        // Dynamic delay between beeps
        silenceMS = get_buzz_silence(bqData.vario_climb_rate);
        if (silenceMS) vTaskDelay(pdMS_TO_TICKS(silenceMS));
        break;

      case BUZZ_STARTUP:
        PlayStartupTune();
        break;

      case BUZZ_SHUTDOWN:
        PlaySwitchOffTune();
        break;

      default:
        // ignore
        break;
    }
  }
}
