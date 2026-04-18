#include "my_tasks.h"


void VariometerTask(void *pvParameters){
switched_off:

  // Wait until it is turned on by button
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

  // Send startup sound command to buzzer task
  buzzerQueueData_td buzzQueueData = {.type = BUZZ_STARTUP, .vario = {0}};
  xQueueSend(buzzerQueue, (void *)&buzzQueueData, 0);
  
  displayQueueData_td displayQueueData = {
    .type = DISPLAY_ON
  };
  xQueueSend(displayQueue, (void *)&displayQueueData, 0);
  
  
  bmp280_td bmp280;
  uint64_t delayMS = 100, delay_climb = 100;
  
  float p0, pnew, p_prev, dp_dt, climb_rate, climb_rate_filt;
  p0 = pnew = p_prev = dp_dt = climb_rate = climb_rate_filt = 0.0f;
  
  // Timing variables for main loop
  float dt = 0.1f;
  TickType_t lastTick;
  
  buzzerParams_td buzzData = {1000, 200};

  // Stabilize initial pressure reading
  for (uint16_t i = 0; i < 30; i++){
    bmp280_read_data(&bmp280);
    p0 += bmp280.pressure_Pa;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  p0 /= 30.0f;
  pnew = p0;
  p_prev = p0;

  lastTick = xTaskGetTickCount();
  dt = 0.1; // Initial guess

  // Main variometer function loop
  while (1){
    bmp280_read_data(&bmp280);

    // Low-pass filter pressure
    pnew = pnew * (1 - ALPHA) + bmp280.pressure_Pa * ALPHA;

    // Pressure rate (Pa/s)
    dp_dt = (pnew - p_prev) / dt;
    p_prev = pnew;

    // Convert to vertical speed (m/s) using 12Pa ~ 1m approximation 
    climb_rate = -dp_dt * 0.083f;

    // Low-pass filter climb rate
    climb_rate_filt = climb_rate_filt * (1.0f - BETA) + climb_rate * BETA;

    // SOUND FEEDBACK
    if (climb_rate_filt >= CLIMB_RATE_THRESHOLD){
      // Frequency increases with climb rate
      buzzData.frequencyHZ = CLIMB_FREQ_BASE + (int)(climb_rate_filt * CLIMB_FREQ_SCALE);
      // Short beep
      buzzData.durationMS = 80;

      // Faster beeps for stronger climb
      delay_climb = 200 - (uint32_t)(climb_rate_filt * 80);

      delayMS = delay_climb < 60 ? 60 : delay_climb;
    }
    else if (climb_rate_filt <= DESCENT_RATE_THRESHOLD){
      // Lower pitch for descent
      buzzData.frequencyHZ = DESCENT_FREQ_BASE + (int)(climb_rate_filt * DESCENT_FREQ_SCALE);
      if (buzzData.frequencyHZ < DESCENT_FREQ_MIN) buzzData.frequencyHZ = DESCENT_FREQ_MIN;

      buzzData.durationMS = 200;

      delayMS = 300;
    }
    else{
      // Dead zone, not enough climb or descent to trigger sound
      delayMS = 100;
    }

    // Send sound command to buzzer task
    if (climb_rate_filt >= CLIMB_RATE_THRESHOLD || climb_rate_filt <= DESCENT_RATE_THRESHOLD){
      buzzQueueData.type = BUZZ_VARIO;
      buzzQueueData.vario = buzzData;
      xQueueSend(buzzerQueue, (void *)&buzzQueueData, 0);
    }

    // Always update display
    displayQueueData.type = DISPLAY_UPDATE;
    displayQueueData.updateData.sensorData = (bmp280_td){bmp280.temperature_C, pnew};
    displayQueueData.updateData.climb_rate = climb_rate_filt;
    xQueueSend(displayQueue, (void *)&displayQueueData, 0);

    vTaskDelay(pdMS_TO_TICKS(delayMS));
    
    // Check if button was pressed to switch off
    if (ulTaskNotifyTake(pdTRUE, 0) != 0){

      buzzQueueData.type = BUZZ_SHUTDOWN;
      xQueueSend(buzzerQueue, (void *)&buzzQueueData, 0);

      displayQueueData.type = DISPLAY_OFF;
      xQueueSend(displayQueue, (void *)&displayQueueData, 0);

      goto switched_off;
    }
  
    // Update dt for next iteration
    dt = (xTaskGetTickCount() - lastTick) / (float)configTICK_RATE_HZ;
    lastTick = xTaskGetTickCount();
  
  }
}