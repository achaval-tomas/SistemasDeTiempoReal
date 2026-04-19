#include "my_tasks.h"
#include "tim.h"

#define TIM2_TICKS_PER_SEC 1000000

void Buzzer(buzzerParams_td buzzData){
  if (buzzData.frequencyHZ == 0 || buzzData.durationMS == 0) return;

  uint32_t arr, pulse;

  // Calculate ARR from frequency in HZ
  arr = (TIM2_TICKS_PER_SEC / buzzData.frequencyHZ) - 1;

  // Set pulse to 50% arr for max volume
  pulse = arr / 2;

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
  int freq;

  while (1){
    xQueueReceive(buzzerQueue, (void *)&bqData, portMAX_DELAY);

    switch(bqData.type){
      case BUZZ_VARIO:
        // Determine buzzer parameters based on climb rate
        if (bqData.vario_climb_rate > 0) {
          buzzParams.frequencyHZ = varioConfig.lift_hz_base + (int)(bqData.vario_climb_rate * varioConfig.lift_hz_scale);
          buzzParams.durationMS = 80;
        } else {
            freq = varioConfig.sink_hz_base + (int)(bqData.vario_climb_rate * varioConfig.sink_hz_scale);
            buzzParams.frequencyHZ = (freq < varioConfig.sink_hz_min) ? varioConfig.sink_hz_min : freq;
            buzzParams.durationMS = 200;
        }
        Buzzer(buzzParams);
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
