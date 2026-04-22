#include "my_tasks.h"
#include "portmacrocommon.h"
#include "projdefs.h"


void VariometerTask(void *pvParameters) {
    sensorQueueData_td sData = {0};
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};

switched_off:
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Play start up tune and show initializing message on display
    buzzMsg.type = BUZZ_STARTUP;
    xQueueOverwrite(buzzerQueue, &buzzMsg);
    
    dispMsg.type = DISPLAY_STARTUP;
    xQueueOverwrite(displayQueue, &dispMsg);

    // Block to give display and buzzer time to initialize
    vTaskDelay(pdMS_TO_TICKS(3000));

    while (1) {
        // Receive the latest sensor data from the BMP280 task
        xQueueReceive(varioQueue, &sData, portMAX_DELAY);

        // Enqueue towards buzzer task with the latest data
        buzzMsg.type = BUZZ_VARIO;
        buzzMsg.vario_climb_rate = sData.climb_rate_mps;
        xQueueOverwrite(buzzerQueue, &buzzMsg);

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_VARIO_UPDATE;
        dispMsg.updateData.climb_rate = sData.climb_rate_mps;
        dispMsg.updateData.pressure_Pa = sData.pressure_Pa;
        dispMsg.updateData.temperature_C = sData.temperature_C;
        xQueueOverwrite(displayQueue, &dispMsg);

        // Check if user button was pressed to switch off
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            // Send shutdown commands to buzzer and display tasks
            buzzMsg.type = BUZZ_SHUTDOWN;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
            dispMsg.type = DISPLAY_OFF;
            xQueueOverwrite(displayQueue, &dispMsg);

            goto switched_off;
        }
        
    }
}