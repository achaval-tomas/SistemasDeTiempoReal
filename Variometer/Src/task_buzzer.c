#include "my_tasks.h"
#include "buzzer.h"
#include <stdint.h>

void get_buzz_params(float climb_rate, uint32_t *frequencyHZ, uint32_t *durationMS, uint32_t *silenceMS){
    if (frequencyHZ == NULL || durationMS == NULL || silenceMS == NULL) return;

    uint32_t freq = 0, duration = 0, silence = 0; // no values if thresholds are not exceeded
    float delta_rate = 0.0f;

    // Calculate parameters adjusted by distance from thresholds
    if (climb_rate >= varioConfig.lift_threshold) {

        // Calculate lift beep frequency
        delta_rate = climb_rate - varioConfig.lift_threshold;
        freq = (uint32_t)(varioConfig.lift_hz_base + (delta_rate * varioConfig.lift_hz_scale));

        // Calculate lift beep duration
        duration = 400 - (uint32_t)((climb_rate - varioConfig.lift_threshold) * 100);
        duration = (duration < 50) ? 50 : duration;

        // Calculate silence between lift beeps
        silence = 50 - (uint32_t)((climb_rate-varioConfig.lift_threshold) * 10);
        silence = (silence < 10) ? 10 : silence;

    } else if (climb_rate <= varioConfig.sink_threshold) {

        // Calculate sink beep frequency
        delta_rate = varioConfig.sink_threshold - climb_rate;
        freq = (uint32_t)(varioConfig.sink_hz_base + (delta_rate * varioConfig.sink_hz_scale));

        if (freq < (uint32_t)varioConfig.sink_hz_min) {
            freq = (uint32_t)varioConfig.sink_hz_min;
        }

        // Calculate sink beep duration
        duration = 500;

        // Calculate silence between sink beeps
        silence = 100;
    }

    *frequencyHZ = freq;
    *durationMS = duration;
    *silenceMS = silence;
}

void Buzzer(uint32_t frequencyHZ, uint32_t durationMS){
  // Skip invalid inputs
  if (frequencyHZ == 0 || durationMS == 0) return;

  // Beep at the specified frequency and duration
  buzz_start(frequencyHZ, (uint8_t)varioConfig.volume);
  vTaskDelay(pdMS_TO_TICKS(durationMS));
  buzz_stop();
}

void PlayStartupTune(){
  uint32_t frequencyHZ, durationMS;

  for (int i = 0; i < 3; i++){
    frequencyHZ = 500 + i*200; // Ascending frequencies
    durationMS = 250 - i*50; // Decreasing duration

    Buzzer(frequencyHZ, durationMS);
    if (i == 2) break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void PlaySwitchOffTune(){
  uint32_t frequencyHZ, durationMS;

  for (int i = 0; i < 4; i++){
    frequencyHZ = 1000 - i*200; // Descending frequencies
    durationMS = 100 + i*10; // Increasing duration

    Buzzer(frequencyHZ, durationMS);
    if (i == 3) break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void BuzzerTask(void *pvParameters){
  // Start PWM signal at 0% "volume"
  buzz_init();

  buzzerQueueData_td bqData;
  uint32_t frequencyHZ = 0, durationMS = 0, silenceMS = 0;

  while (1){
    xQueueReceive(buzzerQueue, (void *)&bqData, portMAX_DELAY);

    switch(bqData.type){
      case BUZZ_VARIO:
        // Beep according to climb rate
        get_buzz_params(bqData.vario_climb_rate, &frequencyHZ, &durationMS, &silenceMS);
        
        if (frequencyHZ != 0){
          Buzzer(frequencyHZ, durationMS);
          vTaskDelay(pdMS_TO_TICKS(silenceMS));
        }
        
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
