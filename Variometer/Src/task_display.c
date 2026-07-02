#include "my_tasks.h"
#include "lcd.h"
#include "FreeRTOSConfig.h"
#include <stdint.h>
#include "stdbool.h"
#include <stdio.h>
#include "portmacrocommon.h"
#include <math.h>

float absf(float x) {
    return (x < 0.0f) ? -x : x;
}

// Estimar altitud con la fórmula estándar de la atmósfera
uint32_t estimate_altitude(float pressure_Pa, float seaLevelPressure_Pa){
    if (seaLevelPressure_Pa <= 0.0f) seaLevelPressure_Pa = 101325.0f;

    return (uint32_t)(44330.0f * (1.0f - powf(pressure_Pa / seaLevelPressure_Pa, 0.19029495f)));
}

bool get_elapsed_time(TickType_t start, uint8_t *hours, uint8_t *minutes, uint8_t *seconds){
    uint32_t elapsed_seconds = (xTaskGetTickCount() - start) / configTICK_RATE_HZ;
    uint8_t new_hours   = elapsed_seconds / 3600;
    uint8_t new_minutes = (elapsed_seconds % 3600) / 60;
    uint8_t new_seconds = elapsed_seconds % 60;

    if (new_seconds != *seconds) {
        *hours = new_hours;
        *minutes = new_minutes;
        *seconds = new_seconds;
        return true; // Time has changed
    }
    return false; // Time has not changed
}

typedef struct {
   uint8_t index;
   uint8_t symbols[8];
} climbHistory_td;

const climbHistory_td clear_history = (climbHistory_td){
    .index = 0,
    .symbols = {CHAR_NO_CLIMB, CHAR_NO_CLIMB, CHAR_NO_CLIMB, CHAR_NO_CLIMB,
                CHAR_NO_CLIMB, CHAR_NO_CLIMB, CHAR_NO_CLIMB, CHAR_NO_CLIMB}
};
climbHistory_td climb_history = clear_history;

// Actualizar historial de símbolos de ascenso/descenso basado en el nuevo climb rate
void update_climb_history(float new_climb_rate) {
    uint8_t new_symbol = CHAR_NO_CLIMB;
    
    if (new_climb_rate <= varioConfig.sink_threshold)
        new_symbol = CHAR_SINK;
    else if (new_climb_rate >= varioConfig.lift_threshold+1.5f)
        new_symbol = CHAR_MAX_CLIMB;
    else if (new_climb_rate >= varioConfig.lift_threshold+0.5f)
        new_symbol = CHAR_BIG_CLIMB;
    else if (new_climb_rate >= varioConfig.lift_threshold)
        new_symbol = CHAR_SMALL_CLIMB;

    climb_history.symbols[climb_history.index] = new_symbol;
    climb_history.index = (climb_history.index + 1) % 8; // buffer circular
}

void DisplayTask(void *pvParameters) {
  lcd_init();
  
  displayQueueData_td disData;
  TickType_t flight_start_tick = 0;
  uint32_t altitude, takeoff_ASL_m = 0;
  uint8_t hours = 0, minutes = 0, seconds = -1; // Inicializar a -1 para forzar la actualización en el primer ciclo
  bool should_set_initial_stats = true;
  float climb_rate, temperature;

  while (1) {
    xQueueReceive(displayQueue, (void *)&disData, portMAX_DELAY);
      
    switch (disData.type) {
      case DISPLAY_ON:
        lcd_on();
        lcd_clear();
        break;

      case DISPLAY_UPDATE_MENU:

        // Actualizar la pantalla con el menú, mostrando hasta 4 lineas y un indicador de la linea seleccionada
        for (uint8_t i = 0; i < DISPLAY_LINE_COUNT; ++i) {
            if (i == disData.menuData.selectedLine) {
                lcd_printf_at(
                  i, 0,
                  (i == 0 && disData.menuData.totalPages > 0) ? "%c %-15.15s" : "%c %-18.18s",
                  CHAR_RIGHT_POINTER,
                  disData.menuData.lines[i]
                );
            } else {
                lcd_printf_at(
                  i, 0,
                  (i == 0 && disData.menuData.totalPages > 0) ? "%-17.17s" : "%-20.20s",
                  disData.menuData.lines[i]
                );
            }
        }

        // Mostrar el indicador de página
        if (disData.menuData.totalPages > 0)
          lcd_printf_at(0, 17, "%u/%u", disData.menuData.currentPage, disData.menuData.totalPages);

        break;
      
      case DISPLAY_UPDATE_VARIO:
        // Establecer los datos al momento del despegue si es la primera vez que se entra en DISPLAY_UPDATE_VARIO
        if (should_set_initial_stats) {
          climb_history = clear_history;
          flight_start_tick = xTaskGetTickCount();
          seconds = -1;
          takeoff_ASL_m = estimate_altitude(disData.varioData.pressure_Pa, varioConfig.sealevel_hPa*100);
          should_set_initial_stats = false;
          lcd_clear();
        }

        // Linea 3: Vario y flecha de ascenso/descenso
        // Se actualiza siempre que se recibe un nuevo evento de DISPLAY_UPDATE_VARIO
        climb_rate = disData.varioData.climb_rate_mps;
        if (absf(climb_rate) < 0.1f) climb_rate = 0.0f; // Deadzone para climb-rates bajos
        lcd_printf_at(2, 0, "V: %+.1fm/s ", climb_rate);          

        uint8_t arrow_char = (
          (climb_rate >= varioConfig.lift_threshold) ? CHAR_UP_ARROW
          : ((climb_rate <= varioConfig.sink_threshold) ? CHAR_DOWN_ARROW 
          : ' ')
        );
        lcd_printf_at(2, 19, "%c", (char)arrow_char);
        
        // Las lineas 1, 2, 4 y el gráfico de historial se actualizan sólo una vez por segundo
        if (get_elapsed_time(flight_start_tick, &hours, &minutes, &seconds)){
          // Linea 1: Tiempo y temperatura
          temperature = disData.varioData.temperature_C;
          lcd_printf_at(0, 0, "%02u:%02u:%02u       %3.0f\xDF""C", hours, minutes, seconds, temperature);
        
          // Linea 2: ASL estimada
          altitude = estimate_altitude(disData.varioData.pressure_Pa, varioConfig.sealevel_hPa*100);
          lcd_printf_at(1, 0, "A: %um     ", altitude);
          
          // Historial de símbolos de ascenso/descenso en la línea 3
          update_climb_history(climb_rate);
          for (int8_t i = 7; i >= 0; --i) {
            uint8_t symbol_index = (climb_history.index + i) % 8;
            lcd_printf_at(2, 11 + i, "%c", (char)climb_history.symbols[symbol_index]);
          }

          // Linea 4: Altura relativa al despegue
          lcd_printf_at(3, 0, "Desp: %+dm      ", (int)(altitude - takeoff_ASL_m));
        }

        // Delay de 200ms para que la pantalla no se actualice demasiado rápido y sea legible
        vTaskDelay(pdMS_TO_TICKS(200));

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
  }
}