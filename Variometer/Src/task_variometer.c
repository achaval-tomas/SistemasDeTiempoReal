#include "my_tasks.h"
#include "portmacrocommon.h"
#include "projdefs.h"

extern TaskHandle_t sensor_task_handle;

void VariometerTask(void *pvParameters) {
    sensorQueueData_td sData = {0};
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};

variometer_off:
    // Wake up from button press notification
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Notify sensor task to wake up
    xTaskNotifyGive(sensor_task_handle);

    // Play start up tune and show initializing message on display
    buzzMsg.type = BUZZ_STARTUP;
    xQueueOverwrite(buzzerQueue, &buzzMsg);
    
    dispMsg.type = DISPLAY_STARTUP;
    xQueueOverwrite(displayQueue, &dispMsg);

    while (1) {
        // Receive the latest sensor data from the BMP280 task
        xQueueReceive(varioQueue, &sData, portMAX_DELAY);

        // Enqueue towards buzzer task with the latest data
        buzzMsg.type = BUZZ_VARIO;
        buzzMsg.vario_climb_rate = sData.climb_rate_mps;
        xQueueOverwrite(buzzerQueue, &buzzMsg);

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_VARIO_UPDATE;
        dispMsg.updateData = (displayUpdateData_td)sData;
        xQueueOverwrite(displayQueue, &dispMsg);

        // Check if user button was pressed to switch off
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            // Send shutdown commands to buzzer and display tasks
            buzzMsg.type = BUZZ_SHUTDOWN;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
            dispMsg.type = DISPLAY_OFF;
            xQueueOverwrite(displayQueue, &dispMsg);
            
            // Notify sensor task to sleep
            xTaskNotifyGive(sensor_task_handle);

            goto variometer_off;
        }
        
    }
}