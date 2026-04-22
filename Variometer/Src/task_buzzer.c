#include "my_tasks.h"
#include "tim.h"

#define TIM2_TICKS_PER_SEC 1000000

uint32_t get_buzz_frequency(float climb_rate) {
    uint32_t freq = 0; // should never stay at 0

    if (climb_rate >= varioConfig.lift_threshold) {
        // Calculate frequency for lift
        freq =  varioConfig.lift_hz_base + (uint32_t)(climb_rate * varioConfig.lift_hz_scale);
    } else if (climb_rate <= varioConfig.sink_threshold) {
        // Calculate frequency for sink
        freq = varioConfig.sink_hz_base + (int)(climb_rate * varioConfig.sink_hz_scale);
        freq = (freq < varioConfig.sink_hz_min) ? varioConfig.sink_hz_min : freq;
    }

    return freq;
}

uint32_t get_buzz_duration(float climb_rate) {
    uint32_t next_delay = 0; // should never stay at 0

    if (climb_rate >= varioConfig.lift_threshold) {
        uint32_t cadence = 500 - (uint32_t)(climb_rate * 110);
        next_delay = (cadence < 60) ? 60 : cadence;
    } else if (climb_rate <= varioConfig.sink_threshold) {
        next_delay = 500;
    }

    return next_delay;
}

void Buzzer(buzzerParams_td buzzData){
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
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  vTaskDelay(pdMS_TO_TICKS(buzzData.durationMS));
  HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
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
  buzzerQueueData_td bqData;
  buzzerParams_td buzzParams;

  while (1){
    xQueueReceive(buzzerQueue, (void *)&bqData, portMAX_DELAY);

    switch(bqData.type){
      case BUZZ_VARIO:
        // Beep according to climb rate
        buzzParams.frequencyHZ = get_buzz_frequency(bqData.vario_climb_rate);
        buzzParams.durationMS = get_buzz_duration(bqData.vario_climb_rate);
        Buzzer(buzzParams);

        // Short delay to avoid continuous buzzing
        vTaskDelay(pdMS_TO_TICKS(50));
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
