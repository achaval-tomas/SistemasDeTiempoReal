#include "my_tasks.h"
#include "tim.h"

#define TIM2_TICKS_PER_SEC 1000000

void Buzzer(buzzerParams_td buzzData){
  uint32_t arr, pulse;

  // Calculate ARR from frequency in HZ
  arr = TIM2_TICKS_PER_SEC / buzzData.frequencyHZ;

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
  buzzerQueueData_td buzzQueueData;

  while (1){
    xQueueReceive(buzzerQueue, (void *)&buzzQueueData, portMAX_DELAY);

    if (buzzQueueData.type == BUZZ_VARIO){
      Buzzer(buzzQueueData.vario);
    } else if (buzzQueueData.type == BUZZ_STARTUP){
      PlayStartupTune();
    } else if (buzzQueueData.type == BUZZ_SHUTDOWN){
      PlaySwitchOffTune();
    }
  }
}
