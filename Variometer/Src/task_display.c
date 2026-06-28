#include "my_tasks.h"
#include "lcd.h"
#include "FreeRTOSConfig.h"
#include <stdint.h>
#include "stdbool.h"
#include <stdio.h>
#include "bmp280.h" // Para estimaciones de altitud
#include "portmacrocommon.h"

float absf(float x) {
    return (x < 0.0f) ? -x : x;
}

void get_elapsed_time(TickType_t start, uint8_t *hours, uint8_t *minutes, uint8_t *seconds){
    uint32_t elapsed_seconds = (xTaskGetTickCount() - start) / configTICK_RATE_HZ;
    *hours   = elapsed_seconds / 3600;
    *minutes = (elapsed_seconds % 3600) / 60;
    *seconds = elapsed_seconds % 60;
}

void DisplayTask(void *pvParameters) {
  lcd_init();
  const TickType_t dislpayDT_ticks = pdMS_TO_TICKS(DISPLAY_DT_MS);
  
  displayQueueData_td disData;
  TickType_t lastTick = xTaskGetTickCount(), flight_start_tick = 0;
  uint16_t takeoff_ASL_m = 0;
  uint8_t hours, minutes, seconds;
  bool should_set_initial_stats = true;
  float altitude, climb_rate, temperature;

  while (1) {
    xQueueReceive(displayQueue, (void *)&disData, portMAX_DELAY);
      
    switch (disData.type) {
      case DISPLAY_ON:
        lcd_on();
        break;

      case DISPLAY_UPDATE_MENU:
        // Actualizar la pantalla con el menú, mostrando hasta 4 lineas y un indicador de la linea seleccionada
        for (uint8_t i = 0; i<DISPLAY_LINE_COUNT; ++i){
          if (i == disData.menuData.selectedLine)
            lcd_printf_at(i, 0, "%c %-18s", CHAR_RIGHT_POINTER, disData.menuData.lines[i]);
          else
            lcd_printf_at(i, 0, "%-20s", disData.menuData.lines[i]);
        }
        break;
      
      case DISPLAY_UPDATE_VARIO:
        // Establecer los datos al momento del despegue si es la primera vez que se entra en DISPLAY_UPDATE_VARIO
        if (should_set_initial_stats) {
          flight_start_tick = xTaskGetTickCount();
          takeoff_ASL_m = bmp280_estimate_altitude(disData.varioData.pressure_Pa, disData.varioData.temperature_C, varioConfig.sealevel_hPa*100);
          should_set_initial_stats = false;
          lcd_clear();
        }
        
        // Linea 1: Tiempo y temperatura
        temperature = disData.varioData.temperature_C;
        get_elapsed_time(flight_start_tick, &hours, &minutes, &seconds);
        lcd_printf_at(0, 0, "%02u:%02u:%02u       %3.0f\xDF""C", hours, minutes, seconds, temperature);
        
        // Linea 2: ASL estimada
        altitude = bmp280_estimate_altitude(disData.varioData.pressure_Pa, disData.varioData.temperature_C, varioConfig.sealevel_hPa*100);
        lcd_printf_at(1, 0, "A: %.0fm     ", altitude);
        
        // Linea 3: Vario y flecha de dirección
        climb_rate = disData.varioData.climb_rate_mps;
        if (absf(climb_rate) < 0.1f) climb_rate = 0.0f; // Deadzone para climb-rates bajos
        uint8_t arrow_char = (
          (climb_rate >= varioConfig.lift_threshold) ? CHAR_UP_ARROW
          : ((climb_rate <= varioConfig.sink_threshold) ? CHAR_DOWN_ARROW 
          : ' ')
        );
        lcd_printf_at(2, 0, "V: %+.1fm/s %c     ", climb_rate, arrow_char);

        // Linea 4: Altura relativa al despegue
        lcd_printf_at(3, 0, "Despegue: %+.0fm      ", altitude - takeoff_ASL_m);
        break;

      case DISPLAY_CLEAR:
        lcd_clear();
        break;

      case DISPLAY_START_FLIGHT:
        lcd_printf_at(0, 0, "%-20s", "");
        lcd_printf_at(1, 0, "%-20s", "     Calibrando");
        lcd_printf_at(2, 0, "%-20s", "     sensores...");
        lcd_printf_at(3, 0, "%-20s", "");
        should_set_initial_stats = true; 
        break;

      case DISPLAY_OFF:
        lcd_off();
        break;

      default:
        // ignorar
        break;
    }

    // Actualizar, a lo sumo, cada 200ms
    vTaskDelayUntil(&lastTick, dislpayDT_ticks);
  }
}