#include "my_tasks.h"
#include "lcd.h"
#include <stdint.h>
#include <stdio.h>
#include "bmp280.h" // for altitude estimation service

float absf(float x) {
    return (x < 0.0f) ? -x : x;
}

void DisplayTask(void *pvParameters) {
  lcd_init();
  const TickType_t dislpayDT_ticks = pdMS_TO_TICKS(DISPLAY_DT_MS);
  
  displayQueueData_td disData;
  TickType_t lastTick = xTaskGetTickCount();
  
  char temp[17]; 
  float altitude, climb_rate, temperature;

  while (1) {
    xQueueReceive(displayQueue, (void *)&disData, portMAX_DELAY);
      
    switch (disData.type) {
      
      case DISPLAY_VARIO_UPDATE:
        altitude = bmp280_estimate_altitude(
          disData.updateData.pressure_Pa,
          disData.updateData.temperature_C,
          varioConfig.sealevel_pa
        );
        climb_rate = disData.updateData.climb_rate_mps;
        if (absf(climb_rate) < 0.1f) climb_rate = 0.0f; // Deadzone for small climb rates

        temperature = disData.updateData.temperature_C;

        // Line 1: Altitude
        snprintf(temp, sizeof(temp), "A: %.0fm", altitude);
        lcd_printf_at(0, 0, "%-16s", temp);

        // Line 1 end: Temperature
        snprintf(temp, sizeof(temp), "%.0f\xDF""C", temperature);
        lcd_printf_at(0, 12, "%4s", temp);

        // Line 2: Climb Rate
        snprintf(temp, sizeof(temp), "V: %+.1fm/s", climb_rate);
        lcd_printf_at(1, 0, "%-16s", temp);

        uint8_t arrow_char = (
             (climb_rate >= varioConfig.lift_threshold) ? CHAR_UP_ARROW
          : ((climb_rate <= varioConfig.sink_threshold) ? CHAR_DOWN_ARROW 
          : ' ')
        );
        lcd_printf_at(1, 11, "%c", arrow_char);

        break;

      case DISPLAY_CLEAR:
        lcd_clear();
        break;

      case DISPLAY_STARTUP:
        lcd_on();
        
        lcd_put_cur(0, 0);
        lcd_printf_at(0, 0, "%-16s", "Variometer ON!");
        
        lcd_put_cur(1, 0);
        lcd_printf_at(1, 0, "%-16s", "Initializing...");
        break;

      case DISPLAY_OFF:
        lcd_off();
        break;

      default:
        // ignore
        break;
    }

    // Update at most every 200ms
    vTaskDelayUntil(&lastTick, dislpayDT_ticks);
  }
}