#include "lcd.h"
#include "my_tasks.h"

void DisplayTask(void *pvParameters) {
  displayQueueData_td disData;
  char temp[17]; 
  float altitude, climb_rate, temperature;

  while (1) {
    xQueueReceive(displayQueue, (void *)&disData, portMAX_DELAY);
      
    switch (disData.type) {
      
      case DISPLAY_UPDATE:
        altitude = bmp280_estimate_altitude(disData.updateData.sensorData, SEA_LEVEL_PRESSURE_PA);
        climb_rate = disData.updateData.climb_rate;
        temperature = disData.updateData.sensorData.temperature_C;

        // Line 1: Altitude
        lcd_put_cur(0, 0);
        snprintf(temp, sizeof(temp), "A: %.0fm", altitude);
        lcd_printf("%-16s", temp);

        lcd_put_cur(0, 12);
        snprintf(temp, sizeof(temp), "%.0f\xDF""C", temperature);
        lcd_printf("%4s", temp);

        // Line 2: Climb Rate
        lcd_put_cur(1, 0);
        snprintf(temp, sizeof(temp), "V: %+.1fm/s", climb_rate);
        lcd_printf("%-16s", temp);
        break;

      case DISPLAY_CLEAR:
        lcd_clear();
        break;

      case DISPLAY_ON:
        lcd_on();
        
        lcd_put_cur(0, 0);
        lcd_printf("%-16s", "Variometer ON!");
        
        lcd_put_cur(1, 0);
        lcd_printf("%-16s", "Initializing...");
        break;

      case DISPLAY_OFF:
        lcd_off();
        break;

      default:
        // ignore
        break;
    }
    
  }
}