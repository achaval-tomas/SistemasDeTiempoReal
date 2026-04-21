#include "my_tasks.h"
#include "bmp280.h"

// Update pressure and temperature data at a fixed rate
void BMP280Task(void *pvParameters) {
    bmp280_td sensor = {0};
    TickType_t lastTick = xTaskGetTickCount();
    const TickType_t sensorDelayTicks = pdMS_TO_TICKS(DT_ms);

    while (1) {
        bmp280_read_data(&sensor);
        xQueueOverwrite(varioQueue, &sensor);
        vTaskDelayUntil(&lastTick, sensorDelayTicks);
    }
    
}